from pathlib import Path
import re

root = Path('D:/Unity/dxvk-remix_numos')
shader_root = root / 'src/dxvk/shaders/rtx/pass'
names = set()
for relative in ['gbuffer/gbuffer_closesthit.rchit.slang', 'gbuffer/gbuffer_psr_closesthit.rchit.slang', 'integrate/integrate_indirect.slang', 'integrate/integrate_indirect_closesthit.rchit.slang']:
    path = shader_root / relative
    source = path.read_text()
    blocks = re.findall(r'//!variant ([^\n]+)\n((?://!>[^\n]*\n|\n)*)', source)
    additions = []
    for filename, defines in blocks:
        stem, extension = filename.rsplit('.', 1)
        names.add(stem)
        additions.append(f'//!variant {stem}_terrain.{extension}\n//!>       TERRAIN_BLEND\n{defines}')
    source = source.replace('//!end-variants', '\n'.join(additions) + '\n//!end-variants')
    path.write_text(source)

for relative in ['gbuffer/gbuffer.slang', 'gbuffer/gbuffer_rayquery.comp.slang']:
    path = shader_root / relative
    source = path.read_text()
    source = re.sub(r'(//!> order [^\n]+)', r'//!> axis terrain -\n//!> axis terrain terrain TERRAIN_BLEND\n\1 terrain', source)
    path.write_text(source)

path = root / 'src/dxvk/rtx_render/rtx_pathtracer_integrate_indirect.cpp'
source = path.read_text()
source = re.sub(r'(#include <rtx_shaders/([^>]+)\.h>)', lambda m: m[1] + f'\n#include <rtx_shaders/{m[2]}_terrain.h>' if m[2] in names else m[1], source)
source = re.sub(r'GET_SHADER_VARIANT\(\s*([^,]+),\s*([^,]+),\s*([^\s)]+)\s*\)', lambda m: f'(getSceneManager().getTerrainBlendArgs().enabled != 0\n          ? GET_SHADER_VARIANT({m[1]}, {m[2]}, {m[3]}_terrain)\n          : GET_SHADER_VARIANT({m[1]}, {m[2]}, {m[3]}))' if m[3] in names else m[0], source)
path.write_text(source)

path = root / 'src/dxvk/rtx_render/rtx_pathtracer_gbuffer.cpp'
source = path.read_text()
source = re.sub(r'(#include <rtx_shaders/([^>]+)\.h>)', lambda m: m[1] + f'\n#include <rtx_shaders/{m[2]}_terrain.h>' if m[2] in names else m[1], source)
source = source.replace('constexpr uint32_t kGbufferVariantPSR', 'constexpr uint32_t kGbufferVariantTerrain = 1u << 11;\n\n    constexpr uint32_t kGbufferVariantPSR', 1)
source = source.replace('GBUFFER_RAYQUERY_RAYGEN_CASE(features, psr, pipeline, nrc, wboit, code)', 'GBUFFER_RAYQUERY_RAYGEN_CASE(features, psr, pipeline, nrc, wboit, terrain, code)')
source = source.replace('case getGbufferMatrixVariantKey(features, psr, nrc, wboit):', 'case getGbufferMatrixVariantKey(features, psr, nrc, wboit) | (terrain ? kGbufferVariantTerrain : 0u):')
source = source.replace('GBUFFER_TRACE_RAYGEN_CASE(features, psr, pipeline, nrc, wboit, code)', 'GBUFFER_TRACE_RAYGEN_CASE(features, psr, pipeline, nrc, wboit, terrain, code)')
source = source.replace('case getGbufferTraceRayGenVariantKey(features, psr, pipeline, nrc, wboit):', 'case getGbufferTraceRayGenVariantKey(features, psr, pipeline, nrc, wboit) | (terrain ? kGbufferVariantTerrain : 0u):')
source = source.replace('GBUFFER_RAYQUERY_COMPUTE_CASE(features, pass, nrc, wboit, code)', 'GBUFFER_RAYQUERY_COMPUTE_CASE(features, pass, nrc, wboit, terrain, code)')
source = source.replace('case getGbufferRayQueryMatrixVariantKey(features, pass, nrc, wboit):', 'case getGbufferRayQueryMatrixVariantKey(features, pass, nrc, wboit) | (terrain ? kGbufferVariantTerrain : 0u):')
source = source.replace('getGbufferClosestHitShader(const uint32_t key)', 'getGbufferClosestHitShader(const uint32_t key, const bool terrainEnabled)')
source = re.sub(r'GET_SHADER_VARIANT\(\s*([^,]+),\s*([^,]+),\s*([^\s)]+)\s*\)', lambda m: f'(terrainEnabled\n          ? GET_SHADER_VARIANT({m[1]}, {m[2]}, {m[3]}_terrain)\n          : GET_SHADER_VARIANT({m[1]}, {m[2]}, {m[3]}))' if m[3] in names else m[0], source)
source = source.replace('const uint32_t rayGenBaseKey = getGbufferVariantKey(featureVariant, isPSRPass, nrcEnabled, wboitEnabled);', 'const bool terrainEnabled = getSceneManager().getTerrainBlendArgs().enabled != 0;\n    const uint32_t terrainKey = terrainEnabled ? kGbufferVariantTerrain : 0u;\n    const uint32_t rayGenBaseKey = getGbufferVariantKey(featureVariant, isPSRPass, nrcEnabled, wboitEnabled) | terrainKey;')
source = source.replace('getGbufferClosestHitShader(closestHitKey)', 'getGbufferClosestHitShader(closestHitKey, terrainEnabled)')
source = source.replace('return getGbufferRayQueryComputeShader(getGbufferRayQueryVariantKey(featureVariant, isPSRPass, nrcEnabled, wboitEnabled));', 'const uint32_t terrainKey = getSceneManager().getTerrainBlendArgs().enabled != 0 ? kGbufferVariantTerrain : 0u;\n    return getGbufferRayQueryComputeShader(getGbufferRayQueryVariantKey(featureVariant, isPSRPass, nrcEnabled, wboitEnabled) | terrainKey);')
path.write_text(source)
