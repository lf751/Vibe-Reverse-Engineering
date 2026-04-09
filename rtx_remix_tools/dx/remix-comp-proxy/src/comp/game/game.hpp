#pragma once
#include "structs.hpp"

namespace comp::game
{
	// JPOG: TRenderContext* captured from SetRenderMatrices hook.
	// +0x8C = WorldViewMatrix (camera view, world→view) — should be D3DTS_VIEW.
	// +0x4C = ModelViewMatrix (View × Model combined) — baked into D3DTS_WORLD by the game.
	// Fix: VIEW = WorldView, WORLD = inverse(WorldView) × ModelView (model-only transform).
	extern void* g_render_ctx;

	// Installs the SetRenderMatrices hook in TRenderD3DInterface.dll.
	// Must be called after the game DLLs are loaded (from renderer::renderer()).
	// Called from SetVertexShaderConstantF: captures c17-c20 (ModelView, column-major).
	extern void on_set_vs_constant_f(UINT start_reg, const float* data, UINT count);

	// Called pre-draw: sets D3DTS_WORLD = MV * inv(View) so Remix places replacements correctly.
	extern void inject_world_pre_draw(IDirect3DDevice9* dev);

	extern void install_render_hooks();

	extern void init_game_addresses();
}
