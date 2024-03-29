// MemoryFetchPerfInShader.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "MemoryFetchPerfInShader.h"
#include "customtimer.h"

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <math.h>
#include <string>

#define HALFUINT

#define SAFE_RELEASE(object) if((object) != nullptr) (object)->Release()

HINSTANCE g_hInst = nullptr;
HWND g_hWnd = nullptr;
D3D_DRIVER_TYPE g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11Device1* g_pd3dDevice1 = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
ID3D11DeviceContext1* g_pImmediateContext1 = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
IDXGISwapChain1* g_pSwapChain1 = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11UnorderedAccessView* g_pComputeOutput = nullptr;
ID3D11Buffer* g_pContantBuffer = nullptr;
ID3D11ComputeShader* g_pComputeShader = nullptr;

const UINT xDimension = 800;
const UINT yDimension = xDimension;
const UINT NUM_ELEMENTS = xDimension * yDimension;
const UINT tiledSize = 32;

ID3D11Texture2D* g_pMatrixATexture = nullptr;
ID3D11Texture2D* g_pMatrixBTexture = nullptr;

ID3D11ShaderResourceView*   g_pMatrixASRV = nullptr;
ID3D11ShaderResourceView*   g_pMatrixBSRV = nullptr;

#ifdef HALFUINT
UINT16* matrixA = nullptr;
UINT16* matrixB = nullptr;
#else
UINT* matrixA = nullptr;
UINT* matrixB = nullptr;
#endif
unsigned int aPosition = 0;
unsigned int bPosition = 0;

UINT g_width;
UINT g_height;

MandelTimer timer;
LONGLONG timeStamp;
int fps = 0;

typedef struct
{
	UINT randomRow;
	UINT randomCol;
} RandomRowCol;

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
void Render();
HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);


void generateInputData()
{
#ifdef  HALFUINT
	matrixA = new UINT16[NUM_ELEMENTS * 4 * 2];
	matrixB = new UINT16[NUM_ELEMENTS * 4 * 2];
#else
	matrixA = new UINT[NUM_ELEMENTS * 4 * 2];
	matrixB = new UINT[NUM_ELEMENTS * 4 * 2];
#endif //  
	if (matrixA == nullptr || matrixB == nullptr)
	{
		printf("alloc memory failed.\n");
		return;
	}

	for (size_t i = 0; i < NUM_ELEMENTS * 4 * 2; i++)
	{
#ifdef  HALFUINT
		matrixA[i] = static_cast <UINT16>(rand() % 128);
		matrixB[i] = static_cast <UINT16>(rand() % 128);
#else
		matrixA[i] = static_cast <UINT>(rand() % 256);
		matrixB[i] = static_cast <UINT>(rand() % 256);
#endif
	}

	return;
}

void generateRandomPositions()
{
	aPosition = static_cast<unsigned int>(rand() % (NUM_ELEMENTS * 4));
	bPosition = static_cast<unsigned int>(rand() % (NUM_ELEMENTS * 4));

	g_pImmediateContext->UpdateSubresource(g_pMatrixATexture, 0, nullptr, matrixA + aPosition, 0, 0);
	g_pImmediateContext->UpdateSubresource(g_pMatrixBTexture, 0, nullptr, matrixB + bPosition, 0, 0);
	return;
}

unsigned int roundUp(unsigned int numToRound, unsigned int multiple)
{
	if (multiple == 0) return numToRound;

	return ((numToRound + multiple - 1) / multiple) * multiple;
}

HRESULT CreateTexture2D(_In_ ID3D11Device* pDevice, _In_ UINT xDimension, _In_ UINT yDimension, bool readonly,
#ifdef HALFUINT
	_In_ UINT16* initialData,
#else
	_In_ UINT* initialData,
#endif // HALFUINT
	_Outptr_ ID3D11Texture2D** pTexture2D)
{
	D3D11_TEXTURE2D_DESC Tex2DDesc = { 0 };
	Tex2DDesc.Width = xDimension;
	Tex2DDesc.Height = yDimension;
	Tex2DDesc.ArraySize = 1;
	Tex2DDesc.SampleDesc.Count = 1;
	Tex2DDesc.SampleDesc.Quality = 0;
	Tex2DDesc.MipLevels = 1;
#ifdef HALFUINT
	Tex2DDesc.Format = DXGI_FORMAT_R16G16B16A16_UINT;
#else
	Tex2DDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
#endif // HALFUINT
	Tex2DDesc.Usage = D3D11_USAGE_DEFAULT;
	if (readonly == true)
	{
		Tex2DDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	}
	else
	{
		Tex2DDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	}

	D3D11_SUBRESOURCE_DATA TexData = { 0 };
	TexData.pSysMem = initialData;
#ifdef HALFUINT
	TexData.SysMemPitch = xDimension * sizeof(UINT16);
#else
	TexData.SysMemPitch = xDimension * sizeof(UINT);
#endif // HALFUINT

	return pDevice->CreateTexture2D(&Tex2DDesc, &TexData, pTexture2D);
}

HRESULT CreateTexture2DSRV(_In_ ID3D11Device* pDevice, _In_ ID3D11Texture2D* pTexture2D, _Outptr_ ID3D11ShaderResourceView** pSRVOut)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC srv;
#ifdef HALFUINT
	srv.Format = DXGI_FORMAT_R16G16B16A16_UINT;
#else
	srv.Format = DXGI_FORMAT_R32G32B32A32_UINT;
#endif // HALFUINT
	srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv.Texture2D.MostDetailedMip = 0;
	srv.Texture2D.MipLevels = 1;
	return pDevice->CreateShaderResourceView(pTexture2D, &srv, pSRVOut);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitWindow(hInstance, nCmdShow)))
	{
		return FALSE;
	}

	generateInputData();
	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return 0;
	}

	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//generateRandomPositions();
		Render();
	}

	CleanupDevice();
	return (int)msg.wParam;
}

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_MEMORYFETCHPERFINSHADER);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"MandalWindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_MEMORYFETCHPERFINSHADER);
	if (!RegisterClassEx(&wcex))
	{
		return E_FAIL;
	}

	g_hInst = hInstance;
	RECT rc = { 0, 0, 800, 800 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(L"MandalWindowClass", L"Memory fetch uint and uint16",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);

	if (!g_hWnd)
	{
		return E_FAIL;
	}

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		switch (wmId)
		{
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

HRESULT InitDevice()
{
	HRESULT hr = S_OK;
	RECT rect;
	GetClientRect(g_hWnd, &rect);
	g_width = rect.right - rect.left;
	g_height = rect.bottom - rect.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

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

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);

		if (hr == E_INVALIDARG)
		{
			hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		}

		if (SUCCEEDED(hr))
		{
			break;
		}
	}

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

	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	if (dxgiFactory2)
	{
		hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1));
		if (SUCCEEDED(hr))
		{
			(void)g_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1));
		}

		DXGI_SWAP_CHAIN_DESC1 sd = {};
		sd.Width = g_width;
		sd.Height = g_height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS | DXGI_USAGE_SHADER_INPUT;
		sd.BufferCount = 1;

		hr = dxgiFactory2->CreateSwapChainForHwnd(g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1);
		if (SUCCEEDED(hr))
		{
			hr = g_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain));
		}

		dxgiFactory2->Release();
	}
	else
	{
		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 1;
		sd.BufferDesc.Width = g_width;
		sd.BufferDesc.Height = g_height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS | DXGI_USAGE_SHADER_INPUT;
		sd.OutputWindow = g_hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);
	}

	dxgiFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);
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
	if (FAILED(hr))
	{
		return hr;
	}

	hr = g_pd3dDevice->CreateUnorderedAccessView(pBackBuffer, nullptr, &g_pComputeOutput);
	if (FAILED(hr))
	{
		return hr;
	}

	pBackBuffer->Release();

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)g_width;
	vp.Height = (FLOAT)g_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	CreateTexture2D(g_pd3dDevice, xDimension * 2, yDimension * 2, true, matrixA, &g_pMatrixATexture);
	CreateTexture2D(g_pd3dDevice, xDimension * 2, yDimension * 2, true, matrixB, &g_pMatrixBTexture);

	printf("Creating texture2d views...");
	CreateTexture2DSRV(g_pd3dDevice, g_pMatrixATexture, &g_pMatrixASRV);
	CreateTexture2DSRV(g_pd3dDevice, g_pMatrixBTexture, &g_pMatrixBSRV);

	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;
	bufferDesc.ByteWidth = roundUp(sizeof(RandomRowCol), 16);
	g_pd3dDevice->CreateBuffer(&bufferDesc, nullptr, &g_pContantBuffer);

	ID3DBlob* pCSBlob = nullptr;
#ifdef HALFUINT
	hr = CompileShaderFromFile(L"MemoryFetchPerfInShader16.hlsl", "main", "cs_5_0", &pCSBlob);
#else
	hr = CompileShaderFromFile(L"MemoryFetchPerfInShader.hlsl", "main", "cs_5_0", &pCSBlob);
#endif
	if (FAILED(hr))
	{
		return hr;
	}
	hr = g_pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(),
		nullptr, &g_pComputeShader);
	pCSBlob->Release();
	if (FAILED(hr))
	{
		return hr;
	}

	return S_OK;
}

void Render()
{
	static LONGLONG elapsedPrev = 0;
	LONGLONG t = timer.ElapsedMicroseconds();
	LONGLONG dt = (elapsedPrev == 0) ? 1 : (t - elapsedPrev) / 1000000;
	elapsedPrev = t;

	g_pImmediateContext->CSSetShader(g_pComputeShader, NULL, 0);
	ID3D11ShaderResourceView* aRViews[2] = { g_pMatrixASRV, g_pMatrixBSRV };
	g_pImmediateContext->CSSetShaderResources(0, 2, aRViews);

	ID3D11UnorderedAccessView* aUAViews[1] = { g_pComputeOutput };
	g_pImmediateContext->CSSetUnorderedAccessViews(0, 1, aUAViews, (UINT*)(&aUAViews));

	D3D11_MAPPED_SUBRESOURCE rowcolmap;
	g_pImmediateContext->Map(g_pContantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &rowcolmap);

	RandomRowCol rowcol;
	rowcol.randomRow = static_cast <UINT>(rand() % xDimension);
	rowcol.randomCol = static_cast <UINT>(rand() % yDimension);
	*(RandomRowCol *)rowcolmap.pData = rowcol;
	g_pImmediateContext->Unmap(g_pContantBuffer, 0);

	ID3D11Buffer* ppCB[1] = { g_pContantBuffer };
	g_pImmediateContext->CSSetConstantBuffers(0, 1, ppCB);
	g_pImmediateContext->Dispatch(800 / 32, 800 / 32, 1);


	g_pSwapChain->Present(0, 0);

	if ((t = timer.ElapsedMicroseconds()) - timeStamp > 1000000) // every second
	{
		float td = (t - timeStamp) / 1000000.0; // real time interval a bit more than a second
		timeStamp = t;
		std::wstring fpsStr = std::to_wstring(int(fps / td));
#ifdef HALFUINT
		std::wstring title = L"Memory fetch halfuint(16bits) test, its fps: " + fpsStr;
#else
		std::wstring title = L"Memory fetch uint(32bits) test, its fps: " + fpsStr;
#endif
		SetWindowText(g_hWnd, title.c_str());
		fps = 0;
	}
	fps++;
}

HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	dwShaderFlags |= D3DCOMPILE_DEBUG;
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr,
		szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
	if (FAILED(hr))
	{
		return hr;
	}
	if (pErrorBlob)
	{
		pErrorBlob->Release();
	}

	return S_OK;
}

void CleanupDevice()
{
	if (g_pImmediateContext)
	{
		g_pImmediateContext->ClearState();
	}

	SAFE_RELEASE(g_pComputeShader);
	SAFE_RELEASE(g_pContantBuffer);
	SAFE_RELEASE(g_pComputeOutput);
	SAFE_RELEASE(g_pRenderTargetView);
	SAFE_RELEASE(g_pSwapChain1);
	SAFE_RELEASE(g_pSwapChain);
	SAFE_RELEASE(g_pImmediateContext1);
	SAFE_RELEASE(g_pImmediateContext);
	SAFE_RELEASE(g_pd3dDevice1);
	SAFE_RELEASE(g_pd3dDevice);
}