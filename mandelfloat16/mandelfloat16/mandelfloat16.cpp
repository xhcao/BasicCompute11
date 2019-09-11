// mandelfloat16.cpp : 定义应用程序的入口点。
//
#include "stdafx.h"
#include "mandelfloat16.h"
#include "Timer.h"

#include <stdio.h>
#include <crtdbg.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <float.h>
#include <math.h>

#define MAX_LOADSTRING 100

#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }

// 全局变量:
HINSTANCE hInst;                                // 当前实例
HWND hWnd;
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

D3D_DRIVER_TYPE g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11ComputeShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;

typedef struct
{
	float a0, b0, da, db;
	float ja0, jb0; // julia set point

	int max_iterations;
	int julia;  // julia or mandelbrot
	unsigned int cycle;

} MandelConstantsNoDoubles;

int g_width, g_height; // size of window
int fps = 0;
int cycle = -110;  // color cycling, with a,z keys
LONGLONG timer = 0;
WCHAR message[256];
Timer time;
// center coordiantes and pixel step size, for mandel and julia
float a0[2], b0[2], da[2], db[2];

bool vsync = false; // redraw synced with screen refresh
bool julia = false; // mandelbrot or julia
bool juliaAnimate = false;

int   mouseButtonState = 0;
float mouseDx = 0;
float mouseDy = 0;

#define MAX_ITERATIONS 1024
int max_iterations = MAX_ITERATIONS;  // can be doubled with m key

ID3D11ComputeShader*        g_pCSMandelJulia_scalarFloat;
ID3D11Buffer*		    				g_pcbFractal;
ID3D11UnorderedAccessView*  g_pComputeOutput;  // compute output

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

HRESULT InitDevice();
void CleanupDevice();
void Render();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 在此处放置代码。

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MANDELFLOAT16, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MANDELFLOAT16));

	if (FAILED(InitDevice()))
	{
		CleanupDevice();
	}

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
			Render();
        }
    }

    return (int) msg.wParam;
}

//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
//***********************************************************************************
void CleanupDevice()
//***********************************************************************************
{
	/*
	  SAFE_RELEASE( g_pFont10 );
	  SAFE_RELEASE( g_pSprite10 );
  */

	SAFE_RELEASE(g_pcbFractal);

	if (g_pd3dDeviceContext) g_pd3dDeviceContext->ClearState();

	if (g_pSwapChain)
	{
		g_pSwapChain->SetFullscreenState(false, NULL); // switch back to full screen else not working ok
		g_pSwapChain->Release();
	}

	if (g_pd3dDeviceContext) g_pd3dDeviceContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
}

//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MANDELFLOAT16));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_MANDELFLOAT16);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;

	// Disable optimizations to further improve shader debugging
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}

//***********************************************************************************
void Resize()
//***********************************************************************************
{

	if (g_pd3dDevice == NULL)
		return;

	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	g_height = height;
	g_width = width;


	SAFE_RELEASE(g_pComputeOutput);  // release first else resize problem

	DXGI_SWAP_CHAIN_DESC sd;
	g_pSwapChain->GetDesc(&sd);
	hr = g_pSwapChain->ResizeBuffers(sd.BufferCount, width, height, sd.BufferDesc.Format, 0);

	ID3D11Texture2D* pTexture;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pTexture);


	// create shader unordered access view on back buffer for compute shader to write into texture
	hr = g_pd3dDevice->CreateUnorderedAccessView(pTexture, NULL, &g_pComputeOutput);


	pTexture->Release();



	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = width;
	vp.Height = height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pd3dDeviceContext->RSSetViewports(1, &vp);

	// mandel
	a0[0] = -0.5f;
	b0[0] = -0;
	da[0] = da[1] = 4.0f / g_width;
	db[0] = db[1] = da[0];//2.0f/g_height;

	// julia
	a0[1] = 0;
	b0[1] = 0;

}

HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};

	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	UINT numFeatureLevels = ARRAYSIZE(featureLevels);
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pd3dDeviceContext);

	if (FAILED(hr))
	{
		return hr;
	}

	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
	{
		return hr;
	}

		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 1;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);

	dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

	dxgiFactory->Release();

	if (FAILED(hr))
	{
		return hr;
	}

	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
	{
		return hr;
	}

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
	{
		return hr;
	}

	g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 0.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pd3dDeviceContext->RSSetViewports(1, &vp);

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.ByteWidth = sizeof(MandelConstantsNoDoubles);
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	hr = g_pd3dDevice->CreateBuffer(&desc, nullptr, &g_pcbFractal);

	if (FAILED(hr))
	{
		return hr;
	}

	DWORD dwShaderFlags = 0;

	ID3DBlob* pCSBlob = nullptr;
	hr = CompileShaderFromFile(L"mandalfloat16.hlsl", "main", "cs_5_0", &pCSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	hr = g_pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
	if (FAILED(hr))
	{
		pCSBlob->Release();
		return hr;
	}

	Resize();

	return S_OK;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此处添加使用 hdc 的任何绘图代码...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

//*************************************************************************
void Render()
//*************************************************************************
{

	// time step for zooming / panning depending on rendering frame rate to make interaction speed independent of frame rate
	static LONGLONG elapsedPrev = 0;
	LONGLONG t = time.ElapsedMicroseconds();
	LONGLONG dt = (elapsedPrev == 0) ? 1 : (t - elapsedPrev) / 1000000;
	elapsedPrev = t;


	// julia or mandelbrot
	int i = (julia && !juliaAnimate) ? 1 : 0;

	// pan
	if (sqrt(mouseDx*mouseDx + mouseDy * mouseDy) > 0.25f)
	{

		float dx = (int)(mouseDx*4.0f);  // truncate to integer
		float dy = (int)(mouseDy*4.0f);

		dx = dx * ceil(dt * 60);  // truncate to integer
		dy = dy * ceil(dt * 60);

		//  pan in pixel steps, so not to have jitter
		a0[i] += da[i] * dx;
		b0[i] += db[i] * dy;

		a0[i] = min(2, max(-2, a0[i]));  // don't move set out of view
		b0[i] = min(2, max(-2, b0[i]));
	}

	// zoom
	if (mouseButtonState & MK_LBUTTON)
	{
		da[i] = da[i] * (1.0f - dt);
		db[i] = db[i] * (1.0f - dt);
	}
	if (mouseButtonState & MK_RBUTTON)
	{
		da[i] = da[i] / (1.0f - dt);
		db[i] = db[i] / (1.0f - dt);
	}


	i = (julia) ? 1 : 0;


	// Fill constant buffer
	D3D11_MAPPED_SUBRESOURCE msr;
	MandelConstantsNoDoubles mc;
	mc.a0 = (float)(a0[i] - da[i] * g_width / 2);
	mc.b0 = (float)(b0[i] - db[i] * g_height / 2);
	mc.da = (float)da[i];
	mc.db = (float)db[i];
	mc.ja0 = (float)a0[0];      // julia point
	mc.jb0 = (float)b0[0];
	mc.cycle = cycle;
	mc.julia = julia;
	mc.max_iterations = max_iterations;
	g_pd3dDeviceContext->Map(g_pcbFractal, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

		
	//*(MandelConstantsNoDoubles *)msr.pData = mc;
	g_pd3dDeviceContext->Unmap(g_pcbFractal, 0);


	// Set compute shader

			g_pd3dDeviceContext->CSSetShader(g_pCSMandelJulia_scalarFloat, NULL, 0);

	// For CS output
	ID3D11UnorderedAccessView* aUAViews[1] = { g_pComputeOutput };
	g_pd3dDeviceContext->CSSetUnorderedAccessViews(0, 1, aUAViews, (UINT*)(&aUAViews));

	// For CS constant buffer
	ID3D11Buffer* ppCB[1] = { g_pcbFractal };
	g_pd3dDeviceContext->CSSetConstantBuffers(0, 1, ppCB);




	// Run the CS
		g_pd3dDeviceContext->Dispatch((g_width + 15) / 16, (g_height + 15) / 16, 1);
	

	// Present our back buffer to our front buffer
	g_pSwapChain->Present(0, 0);


	if ((t = time.ElapsedMicroseconds()) - timer > 1000000) // every second
	{
		LONGLONG td = t - timer; // real time interval a bit more than a second
		timer = t;
		//swprintf(message, L"Mandel  %s %s FPS %.2f",  L"Scalar", L"(floats)", (float)fps / td);
		//SetWindowText(hWnd, message);
		fps = 0;
	}
	fps++;

}
