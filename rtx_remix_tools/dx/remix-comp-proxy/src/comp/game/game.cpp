#include "std_include.hpp"
#include "shared/common/flags.hpp"
#include "shared/globals.hpp"
#include "shared/utils/hooking.hpp"
#include "game.hpp"

namespace comp::game
{
	// TRenderContext* captured from the SetRenderMatrices hook each draw dispatch.
	void* g_render_ctx = nullptr;

	static void* orig_SetRenderMatrices = nullptr;

	// Last known VIEW matrix with real camera translation, persisted across frames.
	// Prevents Remix from seeing near-identity VIEW during camera cut transitions.
	static D3DXMATRIX s_last_real_view;
	static bool s_has_real_view = false;

	// MVP captured from VS constant c0-c3 (transposed to row-major).
	static D3DXMATRIX s_mvp_matrix;
	static bool s_mvp_dirty = false;

	// Hook for TRenderContextD3D::SetRenderMatrices (TRenderD3DInterface.dll + 0x7180).
	//
	// The original sets D3DTS_VIEW = IDENTITY and D3DTS_PROJECTION = this->ProjectionMatrix.
	// After calling the original we override VIEW with the real WorldView matrix so that
	// RTX Remix receives a proper camera matrix instead of identity.
	//
	// Only sends VIEW when the matrix has significant translation. If the camera matrix
	// briefly loses translation (e.g. during a cut), we hold the last good VIEW to prevent
	// Remix from detecting a camera jump, triggering clear() and crashing with replacement assets.
	//
	// Calling convention: __thiscall mapped to __fastcall (ECX = this, EDX ignored).
	static void __fastcall hk_SetRenderMatrices(void* thisptr, void* /*edx*/)
	{
		g_render_ctx = thisptr;

		typedef void(__thiscall* fn_t)(void*);
		reinterpret_cast<fn_t>(orig_SetRenderMatrices)(thisptr);

		// this+0x8C = WorldViewMatrix: actual camera view (world->view space).
		const float* wv = reinterpret_cast<const float*>(static_cast<char*>(thisptr) + 0x8C);

		// Only accept as real view when translation is significant.
		// Row-major layout: translation at [12..14]; column-major: [3,7,11].
		const float tRow2 = wv[12]*wv[12] + wv[13]*wv[13] + wv[14]*wv[14];
		const float tCol2 = wv[3]*wv[3]   + wv[7]*wv[7]   + wv[11]*wv[11];
		if (tRow2 > 1.0f || tCol2 > 1.0f)
		{
			memcpy(&s_last_real_view, wv, sizeof(D3DXMATRIX));
			s_has_real_view = true;
		}

		const D3DMATRIX* view_to_send = s_has_real_view
			? reinterpret_cast<const D3DMATRIX*>(&s_last_real_view)
			: reinterpret_cast<const D3DMATRIX*>(wv);
		shared::globals::d3d_device->SetTransform(D3DTS_VIEW, view_to_send);
	}

	// Called from SetVertexShaderConstantF interception.
	// JPOG VS layout: c0-c3 = transpose(MVP) per object.
	void on_set_vs_constant_f(UINT start_reg, const float* data, UINT count)
	{
		// c0-c3: MVP (column-major in shader, transpose to row-major)
		constexpr UINT MVP_START = 0;
		constexpr UINT MVP_END   = 4;

		const UINT end_reg = start_reg + count;
		if (start_reg <= MVP_START && end_reg >= MVP_END)
		{
			const float* src = data + (MVP_START - start_reg) * 4;
			D3DXMatrixTranspose(&s_mvp_matrix, reinterpret_cast<const D3DXMATRIX*>(src));
			s_mvp_dirty = true;
		}
	}

	// Called pre-draw. Computes WORLD = MVP * inv(P) * inv(V) and sets D3DTS_WORLD so
	// RTX Remix can place replacement assets at correct world-space positions.
	//
	// Split into two inverses (not inv(V*P)) to avoid precision loss from inverting
	// the combined matrix when the camera has large world-space translations.
	void inject_world_pre_draw(IDirect3DDevice9* dev)
	{
		if (!s_mvp_dirty || !s_has_real_view || !g_render_ctx)
			return;
		s_mvp_dirty = false;

		// Projection matrix from TRenderContext+0x484
		const D3DXMATRIX* proj = reinterpret_cast<const D3DXMATRIX*>(
			static_cast<char*>(g_render_ctx) + 0x484);

		D3DXMATRIX inv_proj;
		if (!D3DXMatrixInverse(&inv_proj, nullptr, proj))
			return;

		D3DXMATRIX inv_view;
		if (!D3DXMatrixInverse(&inv_view, nullptr, &s_last_real_view))
			return;

		// MVP * inv(P) -> MV, then MV * inv(V) -> World
		D3DXMATRIX mv, world;
		D3DXMatrixMultiply(&mv, &s_mvp_matrix, &inv_proj);
		D3DXMatrixMultiply(&world, &mv, &inv_view);
		world._14 = 0.f; world._24 = 0.f; world._34 = 0.f; world._44 = 1.f;

		dev->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));
	}

	void install_render_hooks()
	{
		const HMODULE h_render_d3d = GetModuleHandleA("TRenderD3DInterface.dll");
		if (!h_render_d3d)
		{
			shared::common::log("Game", "TRenderD3DInterface.dll not found -- camera fix not installed",
				shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return;
		}

		const DWORD addr = reinterpret_cast<DWORD>(h_render_d3d) + 0x7180;
		if (shared::utils::hook::detour(addr, hk_SetRenderMatrices, &orig_SetRenderMatrices))
			shared::common::log("Game", "Hooked TRenderContextD3D::SetRenderMatrices (camera fix active)",
				shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);
		else
			shared::common::log("Game", "Failed to hook SetRenderMatrices",
				shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
	}

	void init_game_addresses()
	{
		// Called from DllMain -- game DLLs are not yet loaded here.
		// install_render_hooks() is called later from renderer::renderer().
		shared::common::log("Game", "init_game_addresses (JPOG): hooks deferred to renderer init",
			shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
