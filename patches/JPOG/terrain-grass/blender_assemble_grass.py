"""
Blender 4.2 headless script to assemble opaque 3D grass clusters from Maxtree Vol 21
(MT_PM_Imperata_cylindrica_01_08 & _09) and export both:
  - 3ds Max-ready FBX clusters (fbx/)
  - RTX Remix-ready Y-up USDA clusters (usd/meshes/) with zero-alpha opaque materials.

Run via build_grass_clusters.py.
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector
import numpy as np


def parse_args():
    argv = sys.argv
    if "--" not in argv:
        raise RuntimeError("Expected -- <out_dir> <maxtree_root>")
    idx = argv.index("--")
    return argv[idx + 1], argv[idx + 2]


def load_opacity_grid(opacity_path):
    img = bpy.data.images.load(opacity_path)
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape((h, w, 4))[:, :, 0]
    return px, w, h


def import_and_clean_tuft(fbx_path, opacity_px, op_w, op_h, target_ratio):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx_path)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    if not meshes:
        raise RuntimeError(f"No mesh in {fbx_path}")

    obj = meshes[0]
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Remove tiny zero-alpha stray faces so the mesh is 100% opaque geometry.
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    uv_layer = bm.loops.layers.uv.active
    to_delete = []
    if uv_layer is not None:
        for f in bm.faces:
            us = [l[uv_layer].uv.x for l in f.loops]
            vs = [l[uv_layer].uv.y for l in f.loops]
            u = int(np.clip(np.mean(us), 0.0, 0.9999) * op_w)
            v = int(np.clip(np.mean(vs), 0.0, 0.9999) * op_h)
            if opacity_px[v, u] < 0.25:
                to_delete.append(f)
    if to_delete:
        bmesh.ops.delete(bm, geom=to_delete, context="FACES")

    # Center horizontally at origin and ground roots at Z = 0.
    xs = [v.co.x for v in bm.verts]
    ys = [v.co.y for v in bm.verts]
    zs = [v.co.z for v in bm.verts]
    cx = 0.5 * (min(xs) + max(xs))
    cy = 0.5 * (min(ys) + max(ys))
    min_z = min(zs)
    for v in bm.verts:
        v.co.x -= cx
        v.co.y -= cy
        v.co.z -= min_z

    bm.to_mesh(obj.data)
    bm.free()

    if target_ratio < 0.999:
        mod = obj.modifiers.new(name="Decimate", type="DECIMATE")
        mod.decimate_type = "COLLAPSE"
        mod.ratio = target_ratio
        mod.delimit = {"UV"}
        bpy.ops.object.modifier_apply(modifier=mod.name)

    # Triangulate cleanly and compact to only vertices referenced by triangles.
    mesh = obj.data
    mesh.calc_loop_triangles()
    uv_data = mesh.uv_layers.active.data if mesh.uv_layers.active else None
    remap = {}
    verts = []
    tris = []
    uvs = []
    for tri in mesh.loop_triangles:
        remapped = []
        for vi in tri.vertices:
            new_i = remap.get(vi)
            if new_i is None:
                new_i = len(verts)
                remap[vi] = new_i
                co = mesh.vertices[vi].co
                verts.append((co.x, co.y, co.z))
            remapped.append(new_i)
        tris.append((remapped[0], remapped[1], remapped[2]))
        if uv_data:
            uvs.append((
                (uv_data[tri.loops[0]].uv.x, uv_data[tri.loops[0]].uv.y),
                (uv_data[tri.loops[1]].uv.x, uv_data[tri.loops[1]].uv.y),
                (uv_data[tri.loops[2]].uv.x, uv_data[tri.loops[2]].uv.y),
            ))
        else:
            uvs.append(((0.0, 0.0), (0.0, 0.0), (0.0, 0.0)))
    return verts, tris, uvs


def assemble_cluster(tufts, placements, name, out_fbx, out_usda, fbx_tex_dir, usd_mat_rel):
    bpy.ops.wm.read_factory_settings(use_empty=True)

    all_verts = []
    all_tris = []
    all_tri_uvs = []

    for tuft_idx, x, y, yaw_deg, sx, sy, sz in placements:
        verts, tris, uvs = tufts[tuft_idx]
        rot = Matrix.Rotation(math.radians(yaw_deg), 4, "Z")
        base_v = len(all_verts)
        for vx, vy, vz in verts:
            p = rot @ Vector((vx * sx, vy * sy, vz * sz))
            all_verts.append((p.x + x, p.y + y, p.z))
        for i0, i1, i2 in tris:
            all_tris.append((base_v + i0, base_v + i1, base_v + i2))
        all_tri_uvs.extend(uvs)

    # Build Blender mesh for FBX export (3ds Max friendly) and clean normals.
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(all_verts, [], all_tris)
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for poly_idx, poly in enumerate(mesh.polygons):
        tri_uv = all_tri_uvs[poly_idx]
        for k, loop_idx in enumerate(poly.loop_indices):
            uv_layer.data[loop_idx].uv = tri_uv[k]
    mesh.update()

    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

    # Attach opaque PBR material with Maxtree textures for 3ds Max FBX preview.
    mat = bpy.data.materials.new(name="ImperataGrass_Opaque")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    albedo_path = os.path.join(fbx_tex_dir, "Imperata_Leaf_Albedo.jpg")
    rough_path = os.path.join(fbx_tex_dir, "Imperata_Leaf_Roughness.png")
    normal_path = os.path.join(fbx_tex_dir, "Imperata_Leaf_Normal.jpg")

    if bsdf and os.path.exists(albedo_path):
        tex_albedo = nt.nodes.new("ShaderNodeTexImage")
        tex_albedo.image = bpy.data.images.load(albedo_path)
        nt.links.new(tex_albedo.outputs["Color"], bsdf.inputs["Base Color"])
    if bsdf and os.path.exists(rough_path):
        tex_rough = nt.nodes.new("ShaderNodeTexImage")
        tex_rough.image = bpy.data.images.load(rough_path)
        tex_rough.image.colorspace_settings.name = "Non-Color"
        nt.links.new(tex_rough.outputs["Color"], bsdf.inputs["Roughness"])
    if bsdf and os.path.exists(normal_path):
        tex_norm = nt.nodes.new("ShaderNodeTexImage")
        tex_norm.image = bpy.data.images.load(normal_path)
        tex_norm.image.colorspace_settings.name = "Non-Color"
        norm_map = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(tex_norm.outputs["Color"], norm_map.inputs["Color"])
        nt.links.new(norm_map.outputs["Normal"], bsdf.inputs["Normal"])

    obj.data.materials.append(mat)

    os.makedirs(os.path.dirname(out_fbx), exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=out_fbx,
        use_selection=True,
        axis_forward="-Z",
        axis_up="Y",
        apply_scale_options="FBX_SCALE_ALL",
        path_mode="RELATIVE",
        mesh_smooth_type="FACE",
    )

    # Export clean Y-up USDA directly for RTX Remix (Z-up -> Y-up via (x, z, -y), det = +1).
    mesh.calc_loop_triangles()
    pts_yup = [(v.co.x, v.co.z, -v.co.y) for v in mesh.vertices]
    nrm_yup = [(v.normal.x, v.normal.z, -v.normal.y) for v in mesh.vertices]
    xs = [p[0] for p in pts_yup]
    ys = [p[1] for p in pts_yup]
    zs = [p[2] for p in pts_yup]

    fv_counts = ["3"] * len(all_tris)
    fv_indices = []
    st_values = []
    for t_idx, (i0, i1, i2) in enumerate(all_tris):
        fv_indices.append(f"{i0}, {i1}, {i2}")
        u0, u1, u2 = all_tri_uvs[t_idx]
        st_values.append(f"({u0[0]:.5f}, {u0[1]:.5f}), ({u1[0]:.5f}, {u1[1]:.5f}), ({u2[0]:.5f}, {u2[1]:.5f})")

    pts_str = ", ".join(f"({p[0]:.5f}, {p[1]:.5f}, {p[2]:.5f})" for p in pts_yup)
    nrm_str = ", ".join(f"({n[0]:.5f}, {n[1]:.5f}, {n[2]:.5f})" for n in nrm_yup)

    os.makedirs(os.path.dirname(out_usda), exist_ok=True)
    with open(out_usda, "w", encoding="utf-8") as f:
        f.write(f"""#usda 1.0
(
    defaultPrim = "{name}"
    metersPerUnit = 1
    upAxis = "Y"
)

def Xform "{name}"
{{
    def Mesh "mesh"
    {{
        uniform bool doubleSided = 1
        float3[] extent = [({min(xs):.5f}, {min(ys):.5f}, {min(zs):.5f}), ({max(xs):.5f}, {max(ys):.5f}, {max(zs):.5f})]
        int[] faceVertexCounts = [{", ".join(fv_counts)}]
        int[] faceVertexIndices = [{", ".join(fv_indices)}]
        normal3f[] normals = [{nrm_str}] (
            interpolation = "vertex"
        )
        point3f[] points = [{pts_str}]
        texCoord2f[] primvars:st = [{", ".join(st_values)}] (
            interpolation = "faceVarying"
        )
        uniform token subdivisionScheme = "none"
    }}
}}
""")

    width_x = max(xs) - min(xs)
    height_y = max(ys) - min(ys)
    width_z = max(zs) - min(zs)
    print(f"CLUSTER_BUILT: {name} | tris={len(all_tris)} verts={len(pts_yup)} size=({width_x:.2f}m x {width_z:.2f}m, H={height_y:.2f}m)")


def main():
    out_dir, maxtree_root = parse_args()
    fbx_root = os.path.join(maxtree_root, "MT_PM_V21_FBX")
    maps_root = os.path.join(maxtree_root, "MT_PM_V21_Maps")
    opacity_path = os.path.join(maps_root, "MT_M_Imperata_cylindrica_Leaf_01_opacity.jpg")

    op_px, op_w, op_h = load_opacity_grid(opacity_path)

    # Load Imperata_08 (medium height ~39cm) and Imperata_09 (dense short ~24cm) with full lush blades at all 3 LODs.
    tuft08_near = import_and_clean_tuft(os.path.join(fbx_root, "MT_PM_Imperata_cylindrica_01_08.fbx"), op_px, op_w, op_h, 0.065)
    tuft09_near = import_and_clean_tuft(os.path.join(fbx_root, "MT_PM_Imperata_cylindrica_01_09.fbx"), op_px, op_w, op_h, 0.022)

    tuft08_mid = import_and_clean_tuft(os.path.join(fbx_root, "MT_PM_Imperata_cylindrica_01_08.fbx"), op_px, op_w, op_h, 0.032)
    tuft09_mid = import_and_clean_tuft(os.path.join(fbx_root, "MT_PM_Imperata_cylindrica_01_09.fbx"), op_px, op_w, op_h, 0.010)

    tuft08_far = import_and_clean_tuft(os.path.join(fbx_root, "MT_PM_Imperata_cylindrica_01_08.fbx"), op_px, op_w, op_h, 0.020)
    tuft09_far = import_and_clean_tuft(os.path.join(fbx_root, "MT_PM_Imperata_cylindrica_01_09.fbx"), op_px, op_w, op_h, 0.0065)

    fbx_dir = os.path.join(out_dir, "fbx")
    fbx_tex_dir = os.path.join(fbx_dir, "textures")
    usd_mesh_dir = os.path.join(out_dir, "usd", "meshes")
    mat_path = "/RootNode/TerrainGrass/ImperataGrassMaterial"

    # Near clusters (~1.7m diameter, 7 lush tufts each)
    near_a = [
        (0,  0.00,  0.00,   0.0, 1.20, 1.20, 1.12),
        (1,  0.42,  0.14,  62.0, 1.30, 1.30, 1.38),
        (0, -0.44,  0.26, 128.0, 1.15, 1.15, 1.00),
        (1, -0.34, -0.42, 195.0, 1.32, 1.32, 1.32),
        (0,  0.36, -0.38, 265.0, 1.18, 1.18, 1.08),
        (1,  0.04,  0.50, 315.0, 1.25, 1.25, 1.30),
        (0, -0.52, -0.14,  38.0, 1.12, 1.12, 1.05),
    ]
    near_b = [
        (1,  0.04, -0.04,  35.0, 1.35, 1.35, 1.40),
        (0, -0.48, -0.08, 108.0, 1.20, 1.20, 1.08),
        (0,  0.40,  0.36, 175.0, 1.15, 1.15, 1.18),
        (1,  0.28, -0.46, 240.0, 1.30, 1.30, 1.35),
        (0, -0.24,  0.46, 305.0, 1.18, 1.18, 1.02),
        (1, -0.42, -0.36,  78.0, 1.28, 1.28, 1.30),
        (0,  0.50, -0.06, 212.0, 1.15, 1.15, 1.10),
    ]

    # Mid clusters (~4.0m diameter, 18 full tufts each for seamless mid-range carpet)
    def build_mid(seed, parity):
        rng = random.Random(seed)
        pts = [(parity, 0.0, 0.0, rng.uniform(0, 360), 1.95, 1.95, 1.25)]
        for i in range(6):
            ang = i * (2.0 * math.pi / 6.0) + rng.uniform(-0.20, 0.20)
            rad = rng.uniform(0.62, 0.96)
            t = (i + parity) % 2
            pts.append((t, rad * math.cos(ang), rad * math.sin(ang),
                        rng.uniform(0, 360), 1.85, 1.85, 1.25 if t == 0 else 1.52))
        for i in range(11):
            ang = i * (2.0 * math.pi / 11.0) + 0.25 + rng.uniform(-0.16, 0.16)
            rad = rng.uniform(1.30, 1.78)
            t = (i + 1 - parity) % 2
            pts.append((t, rad * math.cos(ang), rad * math.sin(ang),
                        rng.uniform(0, 360), 1.90, 1.90, 1.28 if t == 0 else 1.55))
        return pts

    mid_a = build_mid(101, 0)
    mid_b = build_mid(202, 1)

    # Far clusters (~7.4m diameter, 28 broad lush tufts each for solid distant meadow coverage)
    def build_far(seed, parity):
        rng = random.Random(seed)
        pts = [(parity, 0.0, 0.0, rng.uniform(0, 360), 3.1, 3.1, 1.40)]
        for i in range(6):
            ang = i * (2.0 * math.pi / 6.0) + rng.uniform(-0.18, 0.18)
            rad = rng.uniform(0.95, 1.35)
            t = (i + parity) % 2
            pts.append((t, rad * math.cos(ang), rad * math.sin(ang),
                        rng.uniform(0, 360), 3.0, 3.0, 1.40 if t == 0 else 1.70))
        for i in range(9):
            ang = i * (2.0 * math.pi / 9.0) + 0.30 + rng.uniform(-0.16, 0.16)
            rad = rng.uniform(1.90, 2.45)
            t = (i + 1 - parity) % 2
            pts.append((t, rad * math.cos(ang), rad * math.sin(ang),
                        rng.uniform(0, 360), 3.1, 3.1, 1.42 if t == 0 else 1.72))
        for i in range(12):
            ang = i * (2.0 * math.pi / 12.0) + 0.12 + rng.uniform(-0.14, 0.14)
            rad = rng.uniform(2.85, 3.55)
            t = (i + parity) % 2
            pts.append((t, rad * math.cos(ang), rad * math.sin(ang),
                        rng.uniform(0, 360), 3.2, 3.2, 1.45 if t == 0 else 1.75))
        return pts

    far_a = build_far(303, 0)
    far_b = build_far(404, 1)

    assemble_cluster([tuft08_near, tuft09_near], near_a, "GrassCluster_Near_A",
                     os.path.join(fbx_dir, "GrassCluster_Near_A.fbx"),
                     os.path.join(usd_mesh_dir, "GrassCluster_Near_A.usda"), fbx_tex_dir, mat_path)
    assemble_cluster([tuft08_near, tuft09_near], near_b, "GrassCluster_Near_B",
                     os.path.join(fbx_dir, "GrassCluster_Near_B.fbx"),
                     os.path.join(usd_mesh_dir, "GrassCluster_Near_B.usda"), fbx_tex_dir, mat_path)
    assemble_cluster([tuft08_mid, tuft09_mid], mid_a, "GrassCluster_Mid_A",
                     os.path.join(fbx_dir, "GrassCluster_Mid_A.fbx"),
                     os.path.join(usd_mesh_dir, "GrassCluster_Mid_A.usda"), fbx_tex_dir, mat_path)
    assemble_cluster([tuft08_mid, tuft09_mid], mid_b, "GrassCluster_Mid_B",
                     os.path.join(fbx_dir, "GrassCluster_Mid_B.fbx"),
                     os.path.join(usd_mesh_dir, "GrassCluster_Mid_B.usda"), fbx_tex_dir, mat_path)
    assemble_cluster([tuft08_far, tuft09_far], far_a, "GrassCluster_Far_A",
                     os.path.join(fbx_dir, "GrassCluster_Far_A.fbx"),
                     os.path.join(usd_mesh_dir, "GrassCluster_Far_A.usda"), fbx_tex_dir, mat_path)
    assemble_cluster([tuft08_far, tuft09_far], far_b, "GrassCluster_Far_B",
                     os.path.join(fbx_dir, "GrassCluster_Far_B.fbx"),
                     os.path.join(usd_mesh_dir, "GrassCluster_Far_B.usda"), fbx_tex_dir, mat_path)


if __name__ == "__main__":
    main()
