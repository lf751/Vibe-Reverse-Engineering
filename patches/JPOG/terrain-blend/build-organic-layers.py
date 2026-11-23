"""Build the JPOG terrain weight map and independently tiled PBR materials.

Requires NumPy, Pillow, and ispc-texcomp. The input stage supplies the original
Remix textures, repeat distances, and native island mask.
"""

import argparse
from pathlib import Path
import re
import shutil
import struct

import ispc_texcomp as bc
import numpy as np
from PIL import Image


MATERIAL_INPUTS = ('diffuse', 'normalmap', 'reflectionroughness', 'height')
MATERIAL_SEEDS = {'grass': 571, 'sand': 359, 'dirt': 233, 'deadGrass': 683, 'rock': 883}
SOIL_SATURATION = {'grass': 0.90, 'sand': 0.86, 'dirt': 0.66, 'deadGrass': 0.83, 'rock': 0.88}
ROTATIONS = np.array((((1, 0), (0, 1)), ((0, -1), (1, 0)),
                      ((-1, 0), (0, -1)), ((0, 1), (-1, 0))))


def smooth(start, end, value):
    t = np.clip((value - start) / (end - start), 0, 1)
    return t * t * (3 - 2 * t)


def noise_field(size, cells, rng):
    """Periodic value noise with a different seed per spatial frequency."""
    values = rng.random((cells, cells), dtype=np.float32)
    axis = np.arange(size, dtype=np.float32) * (cells / size)
    lower = axis.astype(np.int32)
    upper = (lower + 1) % cells
    phase = smooth(0, 1, axis - lower).astype(np.float32)
    top = values[lower[:, None], lower] * (1 - phase) + values[lower[:, None], upper] * phase
    bottom = values[upper[:, None], lower] * (1 - phase) + values[upper[:, None], upper] * phase
    return top * (1 - phase[:, None]) + bottom * phase[:, None]


def sample_mask(mask, x, y):
    size = len(mask)
    x, y = np.clip(x, 0, size - 1), np.clip(y, 0, size - 1)
    x0, y0 = x.astype(np.int32), y.astype(np.int32)
    x1, y1 = np.minimum(x0 + 1, size - 1), np.minimum(y0 + 1, size - 1)
    dx, dy = (x - x0)[..., None], (y - y0)[..., None]
    top = mask[y0, x0] * (1 - dx) + mask[y0, x1] * dx
    bottom = mask[y1, x0] * (1 - dx) + mask[y1, x1] * dx
    return (top * (1 - dy) + bottom * dy).astype(np.float32)


def organic_mask(path):
    native = np.asarray(Image.open(path).convert('RGBA'), dtype=np.float32) / 255
    size = len(native)
    rng = np.random.default_rng(57082)
    inland = np.clip(native.sum(axis=2), 0, 1)
    x, y = np.meshgrid(np.arange(size, dtype=np.float32), np.arange(size, dtype=np.float32))
    # Keep the native shoreline in place and make the inland transitions irregular.
    warp = 1.5 + 8 * smooth(0.55, 0.96, inland)
    x += warp * (0.64 * noise_field(size, 43, rng) + 0.36 * noise_field(size, 134, rng) - 0.5)
    y += warp * (0.64 * noise_field(size, 47, rng) + 0.36 * noise_field(size, 141, rng) - 0.5)
    warped = sample_mask(native, x, y)
    grass, dry_grass, dirt, rock = (warped[..., i] for i in range(4))
    sand = np.maximum(0, 1 - warped.sum(axis=2))

    moisture = (0.50 * noise_field(size, 27, rng) + 0.31 * noise_field(size, 76, rng)
                + 0.19 * noise_field(size, 184, rng))
    soil = (0.51 * noise_field(size, 49, rng) + 0.30 * noise_field(size, 113, rng)
            + 0.19 * noise_field(size, 259, rng))
    withered = grass * 0.84 * (1 - smooth(0.30, 0.51, moisture))
    exposed = (grass - withered) * 0.88 * smooth(0.53, 0.73, soil)
    regrowth = dirt * 0.88 * smooth(0.49, 0.72, moisture) * (1 - 0.55 * smooth(0.44, 0.75, soil))
    dry_edges = regrowth * (1 - smooth(0.50, 0.64, moisture))
    eroded = dry_grass * 0.63 * smooth(0.45, 0.70, soil)
    damp_sand = sand * 0.44 * smooth(0.12, 0.62, inland) * smooth(0.40, 0.68, soil)

    weights = np.stack((grass - withered - exposed + regrowth - dry_edges,
                        dry_grass + withered + dry_edges - eroded,
                        dirt + exposed - regrowth + eroded + damp_sand,
                        rock, sand - damp_sand), axis=2)
    return weights / weights.sum(axis=2, keepdims=True)


def blur_height(height, sigma):
    radius = int(3 * sigma)
    distance = np.arange(-radius, radius + 1, dtype=np.float32)
    kernel = np.exp(-0.5 * (distance / sigma) ** 2)
    kernel /= kernel.sum()
    for axis in (0, 1):
        source = np.swapaxes(height, 0, axis)
        padded = np.pad(source, ((radius, radius), (0, 0)), mode='edge')
        filtered = sum(padded[i:i + len(source)] * weight for i, weight in enumerate(kernel))
        height = np.swapaxes(filtered, 0, axis)
    return height


def weather_mountains(weights, height):
    """Expose bedrock on ridges and retain weathered soil in sheltered terrain."""
    size = len(weights)
    if height.ndim != 2 or max(height.shape) > size or not np.isfinite(height).all():
        raise ValueError('The native heightfield must fit the weight map')
    height = np.pad(height.astype(np.float32), ((0, size - height.shape[0]),
                                               (0, size - height.shape[1])), mode='edge')
    rng = np.random.default_rng(39105)
    fine = blur_height(height, 1.65)
    dz, dx = np.gradient(fine)
    grade = np.hypot(dx, dz)
    relief = fine - blur_height(height, 5.8)
    prominence = blur_height(height, 3.5) - blur_height(height, 13.0)
    mottling = 0.68 * noise_field(size, 78, rng) + 0.32 * noise_field(size, 221, rng)

    upland = smooth(5.2, 12, fine)
    cliff = smooth(0.33, 0.77, grade)
    ridge = smooth(0.12, 1.25, relief + 0.17 * prominence)
    gully = smooth(0.04, 0.86, -relief)
    exposure = upland * (0.58 * cliff + 0.35 * ridge) * (0.78 + 0.22 * mottling)
    ledge = 1 - smooth(0.18, 0.46, grade)
    soil = upland * (0.22 * gully + 0.23 * ledge) * smooth(0.25, 0.72, mottling)
    # Weathered seams follow elevation only where there is an exposed slope.
    phase = (fine + 2.2 * noise_field(size, 61, rng)) * np.float32(2 * np.pi / 3.8)
    soil += upland * cliff * (1 - ridge) * 0.16 * smooth(0.42, 0.96, np.sin(phase))

    result = weights.copy()
    vegetation = result[..., :2] * exposure[..., None]
    result[..., :2] -= vegetation
    result[..., 3] += vegetation.sum(axis=2)
    deposits = result[..., 3] * soil
    result[..., 3] -= deposits
    result[..., 2] += deposits
    return result


def volcanic_albedo(path):
    """Grade the existing rock toward basalt and leave patchy iron staining."""
    rgb = np.asarray(Image.open(path).convert('RGB'), dtype=np.float32) / 255
    linear = rgb ** 2.2
    luma = linear @ np.array((0.2126, 0.7152, 0.0722), dtype=np.float32)
    rng = np.random.default_rng(1139)
    deposits = smooth(0.25, 0.80, 0.56 * noise_field(len(rgb), 4, rng)
                      + 0.44 * noise_field(len(rgb), 11, rng))
    saturation = (0.20 + 0.35 * deposits)[..., None]
    stone = luma[..., None] + (linear - luma[..., None]) * saturation
    basalt_tint = np.array((0.90, 1.0, 1.18), dtype=np.float32)
    tone = 0.88 + 0.23 * noise_field(len(rgb), 6, rng)
    return to_bytes((stone * basalt_tint * tone[..., None]) ** (1 / 2.2))


def encode_mask(weights):
    # Largest remainder quantization shares exactly 255 levels across five layers.
    scaled = np.clip(weights, 0, 1) * 255
    quantized = scaled.astype(np.uint8)
    remaining = 255 - quantized.sum(axis=2)
    order = np.argsort(scaled - quantized, axis=2)[..., ::-1]
    rank = np.argsort(order, axis=2)
    return quantized + (rank < remaining[..., None])


def unpack_normal(encoded):
    rg = encoded.astype(np.float32) / 127.5 - 1
    x = (rg[..., 0] + rg[..., 1]) * 0.5
    y = (rg[..., 0] - rg[..., 1]) * 0.5
    n = np.stack((x, y, np.maximum(1 - np.abs(x) - np.abs(y), 0)), axis=2)
    return n / np.linalg.norm(n, axis=2, keepdims=True)


def pack_normal(n):
    xy = n[..., :2] / np.abs(n).sum(axis=2, keepdims=True)
    return np.stack((xy[..., 0] + xy[..., 1], xy[..., 0] - xy[..., 1]), axis=2) * 0.5 + 0.5


def to_bytes(values):
    return np.rint(np.clip(values, 0, 1) * 255).astype(np.uint8)


def load_scan(folder, material):
    """Read the four source maps, converting OpenGL normals to Remix's tangent convention."""
    def read(channel, rgb=False):
        with Image.open(folder / f'{channel}.png') as image:
            pixels = np.asarray(image.convert('RGB') if rgb else image)
        if not rgb and pixels.ndim == 3:
            pixels = pixels[..., 0]
        return pixels.astype(np.float32) / np.iinfo(pixels.dtype).max

    albedo = read('diffuse', rgb=True) ** 2.2
    normal = read('normalmap', rgb=True) * 2 - 1
    normal[..., 1] *= -1
    normal[..., 2] = np.maximum(np.abs(normal[..., 2]), 1e-6)
    normal /= np.linalg.norm(normal, axis=2, keepdims=True)
    roughness = read('reflectionroughness')
    height = read('height')
    if material == 'grass':
        leaf = smooth(0, 0.035, albedo[..., 1] - albedo[..., 0])
        albedo[..., 1] *= 1 + 0.22 * leaf
    return albedo, normal, roughness, height


def sample_patches(maps, material, size):
    """Synthesize a twice-wide tile at the source texel density for each channel."""
    rng = np.random.default_rng(MATERIAL_SEEDS[material])
    original = len(maps[3])
    cells = 4
    offsets = rng.integers(0, original, (cells, cells, 2))
    # Preserve the main orientation of the rock bedding across patch boundaries.
    orientations = ROTATIONS[::2] if material == 'rock' else ROTATIONS
    rotations = orientations[rng.integers(0, len(orientations), (cells, cells))]
    exposure = rng.uniform(0.87, 1.14, (cells, cells)).astype(np.float32)
    broad_tone = 0.78 + 0.34 * noise_field(size, 3, rng) + 0.15 * noise_field(size, 9, rng)
    outputs = [np.empty((size, size, *texture.shape[2:]), np.uint8) for texture in maps]
    outputs[1] = np.empty((size, size, 2), np.uint8)

    x = np.arange(size, dtype=np.int32)[None, :]
    grid_x = x * cells / size
    i = grid_x.astype(np.int32)
    u = smooth(0, 1, grid_x - i).astype(np.float32)
    for start in range(0, size, 96):
        stop = min(start + 96, size)
        y = np.arange(start, stop, dtype=np.int32)[:, None]
        grid_y = y * cells / size
        j = grid_y.astype(np.int32)
        v = smooth(0, 1, grid_y - j).astype(np.float32)
        blended = [np.zeros((stop - start, size, *texture.shape[2:]), np.float32) for texture in maps]
        total = np.zeros((stop - start, size), np.float32)
        for right, down, spatial in ((0, 0, (1 - u) * (1 - v)), (1, 0, u * (1 - v)),
                                     (0, 1, (1 - u) * v), (1, 1, u * v)):
            row = (j + down) % cells
            column = (i + right) % cells
            transform = rotations[row, column]
            phase = offsets[row, column]
            src_x = (transform[..., 0, 0] * x + transform[..., 0, 1] * y + phase[..., 0]) % original
            src_y = (transform[..., 1, 0] * x + transform[..., 1, 1] * y + phase[..., 1]) % original
            height = maps[3][src_y, src_x]
            weight = (spatial * spatial) * np.exp2(4.2 * (height - 0.5))
            total += weight
            color = maps[0][src_y, src_x] * exposure[row, column][..., None]
            normal = maps[1][src_y, src_x]
            normal[..., :2] = np.einsum('...ji,...j->...i', transform, normal[..., :2])
            blended[0] += color * weight[..., None]
            blended[1] += normal * weight[..., None]
            blended[2] += maps[2][src_y, src_x] * weight
            blended[3] += height * weight

        color = blended[0] / total[..., None]
        gray = color @ np.array((0.2126, 0.7152, 0.0722), dtype=np.float32)
        color = gray[..., None] + (color - gray[..., None]) * SOIL_SATURATION[material]
        color *= broad_tone[start:stop, :, None]
        outputs[0][start:stop] = to_bytes(np.maximum(color, 0) ** (1 / 2.2))
        outputs[1][start:stop] = to_bytes(pack_normal(blended[1]))
        outputs[2][start:stop] = to_bytes(blended[2] / total)
        outputs[3][start:stop] = to_bytes(blended[3] / total)
    return outputs


def shrink(values, maximum):
    w = len(values) // 2
    cells = values.reshape(w, 2, w, 2, *values.shape[2:])
    if maximum:
        return cells.max(axis=(1, 3))
    return (cells.sum(axis=(1, 3), dtype=np.uint16) + 2) // 4


def compress(values, dxgi):
    width = len(values)
    padded = max(4, width)
    if dxgi == 99:
        rgba = np.full((width, width, 4), 255, np.uint8)
        rgba[..., :3] = values
        values = rgba
    if padded != width:
        extra = [(0, padded - width)] * 2 + [(0, 0)] * (values.ndim - 2)
        values = np.pad(values, extra, mode='edge')
    surface = bc.RGBASurface(values.tobytes(), padded, padded, padded * (values.shape[2] if values.ndim == 3 else 1))
    if dxgi == 99:
        return bc.compress_blocks_bc7(surface, bc.BC7EncSettings.from_profile('fast'))
    if dxgi == 83:
        return bc.compress_blocks_bc5(surface)
    return bc.compress_blocks_bc4(surface)


def write_dds(path, values, dxgi):
    size = len(values)
    mip_count = size.bit_length()
    compressed = dxgi in (80, 83, 99)
    channels = values.shape[2] if values.ndim == 3 else 1
    pitch = len(compress(values[:4, :4], dxgi)) * (size // 4)**2 if compressed else size * channels
    flags = 0xA1007 if compressed else 0x2100F
    header = [124, flags, size, size, pitch, 0, mip_count, *([0] * 11),
              32, 4, 0x30315844, 0, 0, 0, 0, 0, 0x401008, 0, 0, 0, 0]
    with path.open('wb') as out:
        out.write(b'DDS ' + struct.pack('<31I5I', *header, dxgi, 3, 0, 1, 0))
        while True:
            out.write(compress(values, dxgi) if compressed else values.tobytes())
            if len(values) == 1:
                break
            if dxgi == 99:
                image = Image.fromarray(values)
                values = np.asarray(image.resize((len(values) // 2,) * 2, Image.Resampling.BOX))
            elif dxgi == 28:
                half = len(values) // 2
                stored = values.reshape(half, 2, half, 2, 4).mean(axis=(1, 3), dtype=np.float32) / 255
                sand = np.maximum(0, 1 - stored.sum(axis=2, keepdims=True))
                values = encode_mask(np.concatenate((stored, sand), axis=2))[..., :4]
            else:
                values = shrink(values, maximum=(dxgi == 61)).astype(np.uint8)


def build(stage_path, output, heightfield=None, scans=None):
    stage = stage_path.read_text(encoding='utf-8')
    material_pattern = r'(        def Material "(\w+)"\n        \{.*?\n        \})'
    materials = re.findall(material_pattern, stage, flags=re.S)
    if len(materials) != 5 or {material for _, material in materials} != set(MATERIAL_SEEDS):
        raise ValueError('The input stage must provide grass, dead grass, sand, dirt and rock')

    textures = output / 'textures'
    textures.mkdir(parents=True, exist_ok=True)
    old_control = re.search(r'controlMap = @\./([^@]+)@', stage)[1]
    weights = organic_mask(stage_path.parent / old_control)
    if heightfield is not None:
        weights = weather_mountains(weights, np.load(heightfield))
    write_dds(textures / 'organic-control.dds', encode_mask(weights)[..., :4], 28)
    stage = stage.replace(f'@./{old_control}@', '@./textures/organic-control.dds@')

    for block, material in materials:
        paths = [re.search(rf'inputs:{name}_texture = @\./([^@]+)@', block)[1] for name in MATERIAL_INPUTS]
        if material == 'rock' and scans is None:
            for role, relative in zip(MATERIAL_INPUTS, paths):
                source = stage_path.parent / relative
                filename = source.name
                if role == 'diffuse' and heightfield is not None:
                    filename = 'rock-diffuse.dds'
                    write_dds(textures / filename, volcanic_albedo(source), 99)
                else:
                    shutil.copy2(source, textures / filename)
                stage = stage.replace(f'@./{relative}@', f'@./textures/{filename}@')
            continue
        if scans is None:
            images = [np.asarray(Image.open(stage_path.parent / path).convert('RGB')) for path in paths]
            maps = (images[0].astype(np.float32) ** 2.2 / (255 ** 2.2), unpack_normal(images[1][..., :2]),
                    images[2][..., 0].astype(np.float32) / 255, images[3][..., 0].astype(np.float32) / 255)
        else:
            maps = load_scan(scans / material, material)
        size = len(maps[0])
        if size & (size - 1) or any(image.shape[:2] != (size, size) for image in maps):
            raise ValueError(f'{material} needs four square PBR maps of the same power-of-two size')
        outputs = sample_patches(maps, material, 2 * size)
        for path, name, values, dxgi in zip(paths, MATERIAL_INPUTS, outputs, (99, 83, 80, 61)):
            target = f'{material}-{name}.dds'
            write_dds(textures / target, values, dxgi)
            stage = stage.replace(f'@./{path}@', f'@./textures/{target}@')
        print(f'Built {material}: {len(outputs[0])}² PBR', flush=True)

    main_repeats = r'[0-9.]+, [0-9.]+, [0-9.]+' if scans is not None else r'[0-9.]+, [0-9.]+'
    for pattern in (r'(?<=unitsPerRepeat = \()' + main_repeats, r'(?<=dirtUnitsPerRepeat = )[0-9.]+',
                    r'(?<=deadGrassUnitsPerRepeat = )[0-9.]+'):
        stage, count = re.subn(pattern, lambda m: ', '.join(f'{float(v) * 2:g}' for v in m[0].split(', ')), stage)
        if count != 1:
            raise ValueError(f'Expected one stage parameter matching {pattern}')
    stage = re.sub(r'(?<=heightBlendStrength = )[0-9.]+', '0.68', stage)
    (output / 'mod.usda').write_text(stage, encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline_stage', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--heightfield', type=Path, help='Native island heightfield in NumPy .npy format')
    parser.add_argument('--scans', type=Path, help='Per-material folders containing the four source PNG maps')
    arguments = parser.parse_args()
    build(arguments.baseline_stage, arguments.output, arguments.heightfield, arguments.scans)
