#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <time.h>
#include <math.h>

#include <stdint.h>
#include <stdio.h>

#include <d3dcompiler.h>

#define EXPORT __declspec(dllexport)

typedef struct
{
	HWND wallpaper_window;

	IDXGISwapChain *swapchain;
	ID3D11Device *device;
	ID3D11DeviceContext *ctx;

	ID3D11SamplerState *sampler;
	ID3D11VertexShader *vs;
	ID3D11PixelShader *ps;

	ID3D11Texture2D *rtt;
	ID3D11RenderTargetView *rtv;

	ID3D11Texture2D *shared;
	ID3D11ShaderResourceView *srv;
	HANDLE shared_handle;

	uint32_t width;
	uint32_t height;
	int32_t number_of_monitors;
	RECT monitors [16];
} wallpaper_plugin_state_t;
static  wallpaper_plugin_state_t state = {0};

BOOL enum_monitors (HMONITOR hMonitor, HDC hDC, LPRECT lpRect, LPARAM lParam)
{
	if (state.number_of_monitors < 16)
	{
		MONITORINFO mi = {0};
		mi.cbSize = sizeof (MONITORINFO);
		GetMonitorInfoW (hMonitor, &mi);

		POINT top_left = {.x = mi.rcMonitor.left, .y = mi.rcMonitor.top};
		POINT bottom_right = {.x = mi.rcMonitor.right, .y = mi.rcMonitor.bottom};
		ScreenToClient (state.wallpaper_window, &top_left);
		ScreenToClient (state.wallpaper_window, &bottom_right);

		state.monitors [state.number_of_monitors ++] = (RECT) {
			.left = top_left.x,
			.top = top_left.y,
			.right = bottom_right.x,
			.bottom = bottom_right.y
		};

		return TRUE;
	}

	return FALSE;
}

EXPORT int wallpaper_plugin_init ()
{
	SetProcessDpiAwarenessContext (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	HWND progman = FindWindow (TEXT("Progman"), NULL);
	HWND worker = NULL;
	if (progman)
	{
		worker = FindWindowExW (progman, NULL, L"WorkerW", NULL);
	}

	RECT rect = {0};
	if (worker)
	{
		GetClientRect(progman, &rect);
		state.wallpaper_window = CreateWindowExW (WS_EX_TOOLWINDOW, L"STATIC", NULL, WS_POPUP, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, worker, NULL, GetModuleHandleW (NULL), NULL);
		SetParent (state.wallpaper_window, worker);
		ShowWindow (state.wallpaper_window, SW_SHOW);
		UpdateWindow (state.wallpaper_window);
	}

	uint32_t width = 0;
	uint32_t height = 0;
	if (state.wallpaper_window)
	{
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
		HRESULT result = D3D11CreateDeviceAndSwapChain (NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, NULL, 0, D3D11_SDK_VERSION, &(DXGI_SWAP_CHAIN_DESC){
			.BufferDesc = {
				.Width = width,
				.Height = height,
				.RefreshRate = {
					.Numerator = 60,
					.Denominator = 1,
				},

				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE,
				.Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
			},

			.SampleDesc = {
				.Count = 1,
				.Quality = 0
			},

			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER,
			.BufferCount = 2,
			.OutputWindow = state.wallpaper_window,
			.Windowed = TRUE,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.Flags = 0
		}, &state.swapchain, &state.device, NULL, &state.ctx);

		if (FAILED (result))
		{
			DestroyWindow (state.wallpaper_window);
			state.wallpaper_window = NULL;
		}
	}

	if (state.device)
	{
		IDXGISwapChain_GetBuffer (state.swapchain, 0, &IID_ID3D11Texture2D, &state.rtt);
		ID3D11Device_CreateRenderTargetView (state.device, (ID3D11Resource *) state.rtt, NULL, &state.rtv);

		ID3D11Device_CreateTexture2D (state.device, (&(D3D11_TEXTURE2D_DESC){
			.Width = width,
			.Height = height,
			.MipLevels = 1,
			.ArraySize = 1,
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.SampleDesc = {
				.Count = 1,
				.Quality = 0
			},

			.Usage = D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
			.CPUAccessFlags = 0,
			.MiscFlags = D3D11_RESOURCE_MISC_SHARED
		}), NULL, &state.shared);
		ID3D11Device_CreateShaderResourceView (state.device, (ID3D11Resource *) state.shared, NULL, &state.srv);


		IDXGIResource *dxgi_resource = NULL;
		ID3D11Texture2D_QueryInterface (state.shared, &IID_IDXGIResource, &dxgi_resource);
		IDXGIResource_GetSharedHandle (dxgi_resource, &state.shared_handle);
		IDXGIResource_Release (dxgi_resource);

		#define COMPILE_SHADER 0
		#if COMPILE_SHADER
		ID3DBlob *vs_blob = NULL;
		ID3DBlob *ps_blob = NULL;
		ID3DBlob *error_blob = NULL;
		HRESULT vs_result = D3DCompileFromFile (L"shader.hlsl", NULL, NULL, "vertex", "vs_5_0", 0, 0, &vs_blob, &error_blob);
		HRESULT ps_result = D3DCompileFromFile (L"shader.hlsl", NULL, NULL, "pixel", "ps_5_0", 0, 0, &ps_blob, &error_blob);

		size_t size = ID3D10Blob_GetBufferSize (vs_blob);
		uint32_t *b = ID3D10Blob_GetBufferPointer (vs_blob);
		uint32_t split = 0;
		printf ("= {\n");
		for (size_t i = 0; i < (size / 4); ++ i)
		{
			printf ("0x%.8X,", b[i]);
			if (++split == 8)
			{
				printf ("\n");
				split = 0;
			}
		}
		printf ("\n};\n");
		#endif
		static uint32_t vs_shader [] = {
			0x43425844,0xDAB4D79E,0x90979CC3,0x875A43E2,0x84C1B3BF,0x00000001,0x00000450,0x00000005,
			0x00000034,0x000000A0,0x000000D4,0x0000012C,0x000003B4,0x46454452,0x00000064,0x00000000,
			0x00000000,0x00000000,0x0000003C,0xFFFE0500,0x00000100,0x0000003C,0x31314452,0x0000003C,
			0x00000018,0x00000020,0x00000028,0x00000024,0x0000000C,0x00000000,0x7263694D,0x666F736F,
			0x52282074,0x4C482029,0x53204C53,0x65646168,0x6F432072,0x6C69706D,0x31207265,0x00312E30,
			0x4E475349,0x0000002C,0x00000001,0x00000008,0x00000020,0x00000000,0x00000006,0x00000001,
			0x00000000,0x00000101,0x565F5653,0x65747265,0x00444978,0x4E47534F,0x00000050,0x00000002,
			0x00000008,0x00000038,0x00000000,0x00000001,0x00000003,0x00000000,0x0000000F,0x00000044,
			0x00000000,0x00000000,0x00000003,0x00000001,0x00000C03,0x505F5653,0x7469736F,0x006E6F69,
			0x43584554,0x44524F4F,0xABABAB00,0x58454853,0x00000280,0x00010050,0x000000A0,0x0100086A,
			0x04000060,0x00101012,0x00000000,0x00000006,0x04000067,0x001020F2,0x00000000,0x00000001,
			0x03000065,0x00102032,0x00000001,0x02000068,0x00000001,0x04000069,0x00000000,0x00000006,
			0x00000004,0x04000069,0x00000001,0x00000006,0x00000004,0x09000036,0x00203032,0x00000000,
			0x00000000,0x00004002,0xBF800000,0xBF800000,0x00000000,0x00000000,0x09000036,0x00203032,
			0x00000000,0x00000001,0x00004002,0x3F800000,0x3F800000,0x00000000,0x00000000,0x09000036,
			0x00203032,0x00000000,0x00000002,0x00004002,0x3F800000,0xBF800000,0x00000000,0x00000000,
			0x09000036,0x00203032,0x00000000,0x00000003,0x00004002,0xBF800000,0xBF800000,0x00000000,
			0x00000000,0x09000036,0x00203032,0x00000000,0x00000004,0x00004002,0xBF800000,0x3F800000,
			0x00000000,0x00000000,0x09000036,0x00203032,0x00000000,0x00000005,0x00004002,0x3F800000,
			0x3F800000,0x00000000,0x00000000,0x09000036,0x00203032,0x00000001,0x00000000,0x00004002,
			0x00000000,0x00000000,0x00000000,0x00000000,0x09000036,0x00203032,0x00000001,0x00000001,
			0x00004002,0x3F800000,0x3F800000,0x00000000,0x00000000,0x09000036,0x00203032,0x00000001,
			0x00000002,0x00004002,0x3F800000,0x00000000,0x00000000,0x00000000,0x09000036,0x00203032,
			0x00000001,0x00000003,0x00004002,0x00000000,0x00000000,0x00000000,0x00000000,0x09000036,
			0x00203032,0x00000001,0x00000004,0x00004002,0x00000000,0x3F800000,0x00000000,0x00000000,
			0x09000036,0x00203032,0x00000001,0x00000005,0x00004002,0x3F800000,0x3F800000,0x00000000,
			0x00000000,0x05000036,0x00100012,0x00000000,0x0010100A,0x00000000,0x07000036,0x00102032,
			0x00000000,0x04203046,0x00000000,0x0010000A,0x00000000,0x07000036,0x00102032,0x00000001,
			0x04203046,0x00000001,0x0010000A,0x00000000,0x08000036,0x001020C2,0x00000000,0x00004002,
			0x00000000,0x00000000,0x00000000,0x3F800000,0x0100003E,0x54415453,0x00000094,0x00000011,
			0x00000001,0x00000000,0x00000003,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,
			0x00000000,0x0000000C,0x0000000E,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
			0x00000000,0x00000000,0x00000002,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
			0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
			0x00000000,0x00000000,0x00000000,0x00000000,
		};

		static uint32_t ps_shader [] = {
			0x43425844,0xDF04C03F,0x4FCA47D9,0x9F9399E3,0x4CFB15F4,0x00000001,0x00000290,0x00000005,
			0x00000034,0x000000F0,0x00000148,0x0000017C,0x000001F4,0x46454452,0x000000B4,0x00000000,
			0x00000000,0x00000002,0x0000003C,0xFFFF0500,0x00000100,0x0000008C,0x31314452,0x0000003C,
			0x00000018,0x00000020,0x00000028,0x00000024,0x0000000C,0x00000000,0x0000007C,0x00000003,
			0x00000000,0x00000000,0x00000000,0x00000000,0x00000001,0x00000001,0x00000084,0x00000002,
			0x00000005,0x00000004,0xFFFFFFFF,0x00000000,0x00000001,0x0000000D,0x706D6153,0x0072656C,
			0x74786554,0x00657275,0x7263694D,0x666F736F,0x52282074,0x4C482029,0x53204C53,0x65646168,
			0x6F432072,0x6C69706D,0x31207265,0x00312E30,0x4E475349,0x00000050,0x00000002,0x00000008,
			0x00000038,0x00000000,0x00000001,0x00000003,0x00000000,0x0000000F,0x00000044,0x00000000,
			0x00000000,0x00000003,0x00000001,0x00000303,0x505F5653,0x7469736F,0x006E6F69,0x43584554,
			0x44524F4F,0xABABAB00,0x4E47534F,0x0000002C,0x00000001,0x00000008,0x00000020,0x00000000,
			0x00000000,0x00000003,0x00000000,0x0000000F,0x545F5653,0x65677261,0xABAB0074,0x58454853,
			0x00000070,0x00000050,0x0000001C,0x0100086A,0x0300005A,0x00106000,0x00000000,0x04001858,
			0x00107000,0x00000000,0x00005555,0x03001062,0x00101032,0x00000001,0x03000065,0x001020F2,
			0x00000000,0x8B000045,0x800000C2,0x00155543,0x001020F2,0x00000000,0x00101046,0x00000001,
			0x00107E46,0x00000000,0x00106000,0x00000000,0x0100003E,0x54415453,0x00000094,0x00000002,
			0x00000000,0x00000000,0x00000002,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,
			0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,0x00000000,
			0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
			0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
			0x00000000,0x00000000,0x00000000,0x00000000
		};

		ID3D11Device_CreateVertexShader (state.device, vs_shader, sizeof(vs_shader), NULL, &state.vs);
		ID3D11Device_CreatePixelShader (state.device, ps_shader, sizeof(ps_shader), NULL, &state.ps);
		ID3D11Device_CreateSamplerState (state.device, (&(D3D11_SAMPLER_DESC) {
			.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
			.AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
			.AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
			.AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
			.MipLODBias = 0.0f,
			.MaxAnisotropy = 1,
			.ComparisonFunc = D3D11_COMPARISON_ALWAYS,
			.BorderColor = { 0.0f, 0.0f, 0.0f, 0.0f },
			.MinLOD = 0.0f,
			.MaxLOD = 1.0f,
		}), &state.sampler);

		#if COMPILE_SHADER
		ID3D10Blob_Release (vs_blob);
		ID3D10Blob_Release (ps_blob);
		#endif
		EnumDisplayMonitors (NULL, NULL, enum_monitors, 0);
	}

	state.width = width;
	state.height = height;
	return (state.wallpaper_window != NULL);
}

EXPORT void wallpaper_plugin_quit ()
{
	if (NULL != state.wallpaper_window)
	{
		DestroyWindow (state.wallpaper_window);
	}


	if (NULL != state.device)
	{
		ID3D11ShaderResourceView_Release (state.srv);
		CloseHandle (state.shared_handle);
		ID3D11Texture2D_Release (state.shared);

		ID3D11SamplerState_Release (state.sampler);
		ID3D11VertexShader_Release (state.vs);
		ID3D11PixelShader_Release (state.ps);

		ID3D11RenderTargetView_Release (state.rtv);
		ID3D11Texture2D_Release (state.rtt);
		ID3D11DeviceContext_Release (state.ctx);
		ID3D11Device_Release (state.device);
		IDXGISwapChain_Release (state.swapchain);
	}
	state = (wallpaper_plugin_state_t){0};
}

typedef struct
{
	uint32_t x;
	uint32_t y;
	ID3D11Texture2D *texture;
} wallpaper_t;
EXPORT void wallpaper_plugin_upate (int count, wallpaper_t *wallpapers)
{
	if (NULL == state.wallpaper_window)
	{
		return;
	}

	MSG msg;
	while (PeekMessage (&msg, state.wallpaper_window, 0, 0, PM_REMOVE))
	{
		TranslateMessage (&msg);
		DispatchMessageW (&msg);
	}

	for (int32_t i = 0; i < count; ++ i)
	{
		D3D11_TEXTURE2D_DESC desc = {0};
		ID3D11Device *device = NULL;
		ID3D11DeviceContext *ctx = NULL;
		ID3D11Texture2D_GetDevice (wallpapers [i].texture, &device);
		ID3D11Device_GetImmediateContext (device, &ctx);

		ID3D11Texture2D *texture = NULL;
		ID3D11Device_OpenSharedResource (device, state.shared_handle, &IID_ID3D11Texture2D, &texture);
		ID3D11DeviceContext_CopySubresourceRegion (ctx, (ID3D11Resource *) texture, 0, wallpapers [i].x, wallpapers [i].y, 0, (ID3D11Resource *) wallpapers [i].texture, 0, NULL);
		
		ID3D11Texture2D_Release(texture);
		ID3D11DeviceContext_Release(ctx);
		ID3D11Device_Release(device);
	}

	ID3D11DeviceContext_OMSetRenderTargets (state.ctx, 1, &state.rtv, NULL);
	ID3D11DeviceContext_RSSetViewports (state.ctx, 1, (&(D3D11_VIEWPORT){
		.TopLeftX = 0.0f,
		.TopLeftY = 0.0f,
		.Width = (float) state.width,
		.Height = (float) state.height,
		.MinDepth = 0.0f,
		.MaxDepth = 0.0f
	}));

	ID3D11DeviceContext_VSSetShader (state.ctx, state.vs, NULL, 0);

	ID3D11DeviceContext_PSSetSamplers (state.ctx, 0, 1, &state.sampler);
	ID3D11DeviceContext_PSSetShader (state.ctx, state.ps, NULL, 0);
	ID3D11DeviceContext_PSSetShaderResources (state.ctx, 0, 1, &state.srv);

	ID3D11DeviceContext_IASetPrimitiveTopology(state.ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D11DeviceContext_Draw(state.ctx, 6, 0);

	IDXGISwapChain_Present (state.swapchain, 1, 0);
}

EXPORT int wallpaper_plugin_number_of_monitors ()
{
	return state.number_of_monitors;
}


EXPORT RECT wallpaper_plugin_get_monitor_rect (int monitor_index)
{
	RECT rect = {0};
	if (0 <= monitor_index && monitor_index < state.number_of_monitors)
	{
		rect = state.monitors [monitor_index];
	}

	return rect;
}

int main ()
{
	wallpaper_plugin_init ();
	while (1)
	{
		wallpaper_plugin_upate (0, NULL);
	}

	wallpaper_plugin_quit ();
	return (0);
}