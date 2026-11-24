"""
Blender 4.2 headless script: imports 5 Maxtree Vol 154 palm variants,
strips any zero-alpha polygons, cleanly decimates to ~50k-60k tris while
preserving trunk/petiole detail, centers XY at trunk base (Z=0, Z-up meters),
scales to match each JPOG vanilla palm's exact height, and exports:
  - Per-material Z-up USDA mesh files for RTX Remix (test-game mod)
  - 3ds Max-ready FBX copies in patches/JPOG/palms-v154/fbx/
"""
import os
import sys
import json
import glob
import bmesh
import bpy
import numpy as np
from mathutils import Vector

argv = sys.argv
args = argv[argv.index("--") + 1:] if "--" in argv else []
OUT_DIR = args[0]
V154_ROOT = args[1]

FBX_DIR = os.path.join(V154_ROOT, "MT_PM_V154_FBX")
MAPS_DIR = os.path.join(V154_ROOT, "MT_PM_V154_Maps")

# Exact 5 JPOG palm mesh replacements (all heights matched to existing vegetation.usda palms)
PALM_SPECS = [
    {
        "out_name": "Palm_Roystonea_01",
        "mesh_hash": "mesh_1ED7E05C5B77EA38",
        "old_ref": "ref_d62de1f3e8414d988e5df3e94f29f0ee",
        "fbx": "MT_PM_V154_Roystonea_oleracea_01.fbx",
        "obj": "MT_PM_V154_Roystonea_oleracea_01_01",
        "target_height_m": 10.5,
        "target_tris": 56000,
    },
    {
        "out_name": "DatePalm_Caryota_04",
        "mesh_hash": "mesh_A52962A5D4E16306",
        "old_ref": "ref_2dff8af6fc9e4250ab65fcb7ea9ef1fb",
        "fbx": "MT_PM_V154_Caryota_urens_01.fbx",
        "obj": "MT_PM_V154_Caryota_urens_01_04",
        "target_height_m": 9.6,
        "target_tris": 58000,
        "strip_materials": ["Fruit_01"],
    },
    {
        "out_name": "SagoPalmTall_Archontophoenix_02",
        "mesh_hash": "mesh_17D371F59020F1E8",
        "old_ref": "ref_b7096fb06ce34a59b6edaa60a8d1fea8",
        "fbx": "MT_PM_V154_Archontophoenix_cunninghamiana_01.fbx",
        "obj": "MT_PM_V154_Archontophoenix_cunninghamiana_01_02",
        "target_height_m": 9.1,
        "target_tris": 54000,
    },
    {
        "out_name": "SagoPalmShort_Syagrus_01",
        "mesh_hash": "mesh_F170165D5CCFCD7D",
        "old_ref": "ref_33d70ef66edd41229f1097dacd76ce91",
        "fbx": "MT_PM_V154_Syagrus_coronata_01.fbx",
        "obj": "MT_PM_V154_Syagrus_coronata_01_01",
        "target_height_m": 4.6,
        "target_tris": 52000,
    },
    {
        "out_name": "SagoPalmWide_Syagrus_03",
        "mesh_hash": "mesh_4B520816B40FDE06",
        "old_ref": "ref_b0f11a278dae4d8f80a937ab976299de",
        "fbx": "MT_PM_V154_Syagrus_coronata_01.fbx",
        "obj": "MT_PM_V154_Syagrus_coronata_01_03",
        "target_height_m": 7.0,
        "target_tris": 56000,
    },
]


def clean_mat_name(name):
    if "." in name and name.split(".")[-1].isdigit():
        name = name.rsplit(".", 1)[0]
    return name


def load_opacity_array(mat_base):
    matches = glob.glob(os.path.join(MAPS_DIR, mat_base + "*opacity*.jpg"))
    if not matches:
        return None
    img = bpy.data.images.load(matches[0])
    w, h = img.size
    pix = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(pix)
    arr = pix.reshape((h, w, 4))[:, :, 0].copy()
    bpy.data.images.remove(img)
    return arr


def process_palm(spec):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    fbx_path = os.path.join(FBX_DIR, spec["fbx"])
    bpy.ops.import_scene.fbx(filepath=fbx_path)

    src = bpy.data.objects.get(spec["obj"])
    if src is None:
        raise RuntimeError(f"Object {spec['obj']} not found in {fbx_path}")

    # Delete all other objects from the scene so we work solely on the chosen variant.
    for o in list(bpy.data.objects):
        if o != src:
            bpy.data.objects.remove(o, do_unlink=True)

    bpy.context.view_layer.objects.active = src
    src.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    mesh = src.data

    # 1. Strip explicitly excluded alpha-card materials (e.g., hanging fruit tassels) and zero-alpha faces.
    strip_tags = [t.lower() for t in spec.get("strip_materials", [])]
    strip_mat_idxs = {
        idx for idx, m in enumerate(mesh.materials)
        if m is not None and any(tag in m.name.lower() for tag in strip_tags)
    }
    mat_op = {}
    for idx, m in enumerate(mesh.materials):
        if m is not None and idx not in strip_mat_idxs:
            op = load_opacity_array(clean_mat_name(m.name))
            if op is not None:
                mat_op[idx] = op

    if (mat_op or strip_mat_idxs) and mesh.uv_layers.active:
        bm = bmesh.new()
        bm.from_mesh(mesh)
        uv_layer = bm.loops.layers.uv.active
        to_del = []
        for f in bm.faces:
            if f.material_index in strip_mat_idxs:
                to_del.append(f)
                continue
            op_arr = mat_op.get(f.material_index)
            if op_arr is None:
                continue
            oh, ow = op_arr.shape
            # Keep face if centroid OR any vertex lands inside solid opaque leaf (>0.2)
            us = [l[uv_layer].uv.x for l in f.loops]
            vs = [l[uv_layer].uv.y for l in f.loops]
            cu = sum(us) / len(us)
            cv = sum(vs) / len(vs)
            cpx = int((cu % 1.0) * (ow - 1))
            cpy = int((cv % 1.0) * (oh - 1))
            max_a = float(op_arr[cpy, cpx])
            if max_a < 0.2:
                for u, v in zip(us, vs):
                    px = int((u % 1.0) * (ow - 1))
                    py = int((v % 1.0) * (oh - 1))
                    max_a = max(max_a, float(op_arr[py, px]))
            if max_a < 0.2:
                to_del.append(f)
        if to_del:
            bmesh.ops.delete(bm, geom=to_del, context="FACES")
            bm.to_mesh(mesh)
        bm.free()
        mesh.update()

    # 2. Scale uniformly to exact JPOG height & place trunk base at (0, 0, 0) in Z-up meters.
    zs = [v.co.z for v in mesh.vertices]
    min_z, max_z = min(zs), max(zs)
    cur_h = max(max_z - min_z, 1e-4)
    scale_f = spec["target_height_m"] / cur_h

    # Find trunk base XY center from vertices within bottom 8% of tree height.
    base_z_cut = min_z + cur_h * 0.08
    base_verts = [v.co for v in mesh.vertices if v.co.z <= base_z_cut]
    if base_verts:
        cx = sum(v.x for v in base_verts) / len(base_verts)
        cy = sum(v.y for v in base_verts) / len(base_verts)
    else:
        cx = sum(v.co.x for v in mesh.vertices) / len(mesh.vertices)
        cy = sum(v.co.y for v in mesh.vertices) / len(mesh.vertices)

    for v in mesh.vertices:
        v.co.x = (v.co.x - cx) * scale_f
        v.co.y = (v.co.y - cy) * scale_f
        v.co.z = (v.co.z - min_z) * scale_f
    mesh.update()

    # 3. Decimate to target triangle budget while preserving UVs and material boundaries.
    mesh.calc_loop_triangles()
    orig_tris = len(mesh.loop_triangles)
    if orig_tris > spec["target_tris"]:
        ratio = spec["target_tris"] / float(orig_tris)
        mod = src.modifiers.new(name="Decimate", type="DECIMATE")
        mod.decimate_type = "COLLAPSE"
        mod.ratio = max(0.15, min(1.0, ratio))
        mod.use_collapse_triangulate = True
        mod.delimit = {"MATERIAL", "UV"}
        bpy.ops.object.modifier_apply(modifier=mod.name)

    mesh = src.data
    mesh.calc_loop_triangles()

    # 4. Export 3ds Max-ready FBX copy.
    fbx_out = os.path.join(OUT_DIR, "fbx", f"{spec['out_name']}.fbx")
    os.makedirs(os.path.dirname(fbx_out), exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=fbx_out,
        use_selection=True,
        axis_forward="-Y",
        axis_up="Z",
        apply_scale_options="FBX_SCALE_ALL",
        path_mode="RELATIVE",
        mesh_smooth_type="FACE",
    )

    # 5. Export clean Z-up USDA with one child Mesh prim per material (for reliable USD MaterialBindingAPI).
    uv_data = mesh.uv_layers.active.data if mesh.uv_layers.active else None
    per_mat_tris = {}
    for tri in mesh.loop_triangles:
        m = mesh.materials[tri.material_index] if tri.material_index < len(mesh.materials) else None
        if m is None:
            continue
        mname = clean_mat_name(m.name)
        per_mat_tris.setdefault(mname, []).append(tri)

    out_usda = os.path.join(OUT_DIR, "usd", "meshes", f"{spec['out_name']}.usda")
    os.makedirs(os.path.dirname(out_usda), exist_ok=True)

    lines = [
        "#usda 1.0",
        "(",
        f'    defaultPrim = "{spec["out_name"]}"',
        "    metersPerUnit = 1",
        '    upAxis = "Z"',
        ")",
        "",
        f'def Xform "{spec["out_name"]}"',
        "{",
    ]

    total_tris_written = 0
    used_mats = []

    for mname, tri_list in sorted(per_mat_tris.items()):
        if not tri_list:
            continue
        used_mats.append(mname)
        is_double_sided = ("leaf" in mname.lower()) or ("sheath" in mname.lower())
        # Compact vertices per material submesh
        vmap = {}
        pts = []
        nrms = []
        fv_indices = []
        st_vals = []
        for tri in tri_list:
            idx3 = []
            for vi in tri.vertices:
                ni = vmap.get(vi)
                if ni is None:
                    ni = len(pts)
                    vmap[vi] = ni
                    co = mesh.vertices[vi].co
                    nm = mesh.vertices[vi].normal
                    pts.append((co.x, co.y, co.z))
                    nrms.append((nm.x, nm.y, nm.z))
                idx3.append(ni)
            fv_indices.append(f"{idx3[0]}, {idx3[1]}, {idx3[2]}")
            if uv_data:
                u0 = uv_data[tri.loops[0]].uv
                u1 = uv_data[tri.loops[1]].uv
                u2 = uv_data[tri.loops[2]].uv
                st_vals.append(f"({u0.x:.5f}, {u0.y:.5f}), ({u1.x:.5f}, {u1.y:.5f}), ({u2.x:.5f}, {u2.y:.5f})")
            else:
                st_vals.append("(0, 0), (0, 0), (0, 0)")

        total_tris_written += len(tri_list)
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        zs_p = [p[2] for p in pts]
        safe_prim = mname.replace("-", "_").replace(".", "_")
        fv_counts = ", ".join(["3"] * len(tri_list))
        pts_str = ", ".join(f"({p[0]:.5f}, {p[1]:.5f}, {p[2]:.5f})" for p in pts)
        nrm_str = ", ".join(f"({n[0]:.5f}, {n[1]:.5f}, {n[2]:.5f})" for n in nrms)

        lines.extend([
            f'    def Mesh "{safe_prim}"',
            "    {",
            f"        uniform bool doubleSided = {1 if is_double_sided else 0}",
            f"        float3[] extent = [({min(xs):.5f}, {min(ys):.5f}, {min(zs_p):.5f}), ({max(xs):.5f}, {max(ys):.5f}, {max(zs_p):.5f})]",
            f"        int[] faceVertexCounts = [{fv_counts}]",
            f"        int[] faceVertexIndices = [{', '.join(fv_indices)}]",
            f"        normal3f[] normals = [{nrm_str}] (",
            '            interpolation = "vertex"',
            "        )",
            f"        point3f[] points = [{pts_str}]",
            f"        texCoord2f[] primvars:st = [{', '.join(st_vals)}] (",
            '            interpolation = "faceVarying"',
            "        )",
            '        uniform token subdivisionScheme = "none"',
            "    }",
        ])

    lines.append("}")
    with open(out_usda, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    final_zs = [v.co.z for v in mesh.vertices]
    final_xs = [v.co.x for v in mesh.vertices]
    print(
        f"PALM_BUILT: {spec['out_name']} | orig_tris={orig_tris} -> tris={total_tris_written} "
        f"| H={max(final_zs)-min(final_zs):.2f}m W={max(final_xs)-min(final_xs):.2f}m "
        f"| mats={len(used_mats)}"
    )
    return {
        "out_name": spec["out_name"],
        "mesh_hash": spec["mesh_hash"],
        "old_ref": spec["old_ref"],
        "materials": used_mats,
        "tris": total_tris_written,
    }


def main():
    manifest = []
    for spec in PALM_SPECS:
        manifest.append(process_palm(spec))
    manifest_path = os.path.join(OUT_DIR, "palms_manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    print("PALM_EXPORT_DONE")


if __name__ == "__main__":
    main()
