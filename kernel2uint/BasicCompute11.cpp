//--------------------------------------------------------------------------------------
// File: BasicCompute11.cpp
//
// Demonstrates the basics to get DirectX 11 Compute Shader (aka DirectCompute) up and
// running by implementing Array A + Array B
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <stdio.h>
#include <crtdbg.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <time.h>
#include <float.h>
#include <math.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

// The number of texls
const UINT xDimension = 2560;
const UINT yDimension = 2560;
const UINT NUM_ELEMENTS = xDimension * yDimension;
const UINT tiledSize = 32;


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
HRESULT CreateComputeDevice( _Outptr_ ID3D11Device** ppDeviceOut, _Outptr_ ID3D11DeviceContext** ppContextOut, _In_ bool bForceRef );
HRESULT CreateComputeShader( _In_z_ LPCWSTR pSrcFile, _In_z_ LPCSTR pFunctionName, 
                             _In_ ID3D11Device* pDevice, _Outptr_ ID3D11ComputeShader** ppShaderOut );
HRESULT CreateTexture2D(_In_ ID3D11Device* pDevice, _In_ UINT xDimension, _In_ UINT yDimension, bool readonly, _In_ UINT32* initialData, _Outptr_ ID3D11Texture2D** pTexture2D);
HRESULT CreateTexture2DSRV( _In_ ID3D11Device* pDevice, _In_ ID3D11Texture2D* pBuffer, _Outptr_ ID3D11ShaderResourceView** pSRVOut );
HRESULT CreateTexture2DUAV( _In_ ID3D11Device* pDevice, _In_ ID3D11Texture2D* pBuffer, _Outptr_ ID3D11UnorderedAccessView** pUAVOut );
void RunComputeShader( _In_ ID3D11DeviceContext* pd3dImmediateContext,
                       _In_ ID3D11ComputeShader* pComputeShader,
                       _In_ UINT nNumViews, _In_reads_(nNumViews) ID3D11ShaderResourceView** pShaderResourceViews, 
                       _In_opt_ ID3D11Buffer* pCBCS, _In_reads_opt_(dwNumDataBytes) void* pCSData, _In_ DWORD dwNumDataBytes,
                       _In_ ID3D11UnorderedAccessView* pUnorderedAccessView,
                       _In_ UINT X, _In_ UINT Y, _In_ UINT Z );
ID3D11Texture2D* CreateAndCopyToDebugTex2D(_In_ ID3D11Device* pDevice, _In_ ID3D11DeviceContext* pd3dImmediateContext, _In_ ID3D11Texture2D* pTexture2D);
HRESULT FindDXSDKShaderFileCch( _Out_writes_(cchDest) WCHAR* strDestPath,
                                _In_ int cchDest, 
                                _In_z_ LPCWSTR strFilename );

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3D11Device*               g_pDevice = nullptr;
ID3D11DeviceContext*        g_pContext = nullptr;
ID3D11ComputeShader*        g_pCS = nullptr;

ID3D11Texture2D* g_pMatrixATexture = nullptr;
ID3D11Texture2D* g_pMatrixBTexture = nullptr;
ID3D11Texture2D* g_pMatrixCTexture = nullptr;

ID3D11ShaderResourceView*   g_pMatrixASRV = nullptr;
ID3D11ShaderResourceView*   g_pMatrixBSRV = nullptr;
ID3D11UnorderedAccessView*  g_pMatrixCUAV = nullptr;

UINT32* matrixA = nullptr;
UINT32* matrixB = nullptr;
UINT32* matrixC = nullptr;
UINT32* matrixReference = nullptr;

clock_t tStart = 0;

//--------------------------------------------------------------------------------------
// Entry point to the program
//--------------------------------------------------------------------------------------
int __cdecl main()
{
	// Enable run-time memory check for debug builds.
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif    

	printf("Creating device...");
	if (FAILED(CreateComputeDevice(&g_pDevice, &g_pContext, false)))
		return 1;
	printf("done\n");

	printf("Creating Compute Shader...");
	if (FAILED(CreateComputeShader(L"BasicCompute11.hlsl", "CSMain", g_pDevice, &g_pCS)))
		return 1;
	printf("done\n");
	D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT precisionSupport;
	HRESULT res = g_pDevice->CheckFeatureSupport(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT,
		&precisionSupport, sizeof(D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT));
	if (res == S_OK)
	{
		printf("PixelShaderMinPrecision: %d, AllOtherShaderStagesMinPrecision: %d\n",
			precisionSupport.PixelShaderMinPrecision, precisionSupport.AllOtherShaderStagesMinPrecision);
	}

	printf("Creating texture data and filling them with initial data...");
	matrixA = new UINT32[NUM_ELEMENTS];
	matrixB = new UINT32[NUM_ELEMENTS];
	matrixC = new UINT32[NUM_ELEMENTS];
	matrixReference = new UINT32[NUM_ELEMENTS];
	if (matrixA == nullptr || matrixB == nullptr || matrixC == nullptr || matrixReference == nullptr)
	{
		printf("alloc memory failed\n");
		return 1;
	}
    for ( int i = 0; i < NUM_ELEMENTS; ++i ) 
    {
		matrixA[i] = static_cast <UINT32>(rand() % 10);
		matrixB[i] = static_cast <UINT32>(rand() % 10);
		matrixC[i] = 0;
    }

	for (int m = 0; m < yDimension; m++)
	{
		for (int n = 0; n < xDimension; n++)
		{
			UINT32 result = 0;
			for (int k = 0; k < xDimension; k++)
			{
				result += matrixA[m * yDimension + k] * matrixB[k * yDimension + n];
			}
			matrixReference[m*yDimension + n] = result;
		}
	}

	CreateTexture2D(g_pDevice, xDimension, yDimension, true, matrixA, &g_pMatrixATexture);
	CreateTexture2D(g_pDevice, xDimension, yDimension, true, matrixB, &g_pMatrixBTexture);
	CreateTexture2D(g_pDevice, xDimension, yDimension, false, matrixC, &g_pMatrixCTexture);

	printf("data size: %d\n", sizeof(UINT32));
    printf( "done\n" );

    printf( "Creating texture2d views..." );
	CreateTexture2DSRV(g_pDevice, g_pMatrixATexture, &g_pMatrixASRV);
	CreateTexture2DSRV(g_pDevice, g_pMatrixBTexture, &g_pMatrixBSRV);
	CreateTexture2DUAV(g_pDevice, g_pMatrixCTexture, &g_pMatrixCUAV);

    printf( "done\n" );

    printf( "Running Compute Shader..." );
	tStart = clock();
    ID3D11ShaderResourceView* aRViews[2] = { g_pMatrixASRV, g_pMatrixBSRV };
    RunComputeShader( g_pContext, g_pCS, 2, aRViews, nullptr, nullptr, 0, g_pMatrixCUAV, xDimension / tiledSize, yDimension / tiledSize, 1);
    printf( "done\n" );

    // Read back the result from GPU, verify its correctness against result computed by CPU
    {
		ID3D11Texture2D* debugTex = CreateAndCopyToDebugTex2D( g_pDevice, g_pContext, g_pMatrixCTexture);
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        float *p;
        g_pContext->Map(debugTex, 0, D3D11_MAP_READ, 0, &MappedResource );

        // Set a break point here and put down the expression "p, 1024" in your watch window to see what has been written out by our CS
        // This is also a common trick to debug CS programs.
        p = (float*)MappedResource.pData;
		printf("RowPitch: %d, DepthPitch: %d\n", MappedResource.RowPitch, MappedResource.DepthPitch);

        // Verify that if Compute Shader has done right
        printf( "Verifying against CPU result...\n");
        bool bSuccess = true;
		for (int m = 0; m < yDimension; m++)
		{
			int k = m * MappedResource.RowPitch / sizeof(float);
			for (int n = 0; n < yDimension; n++)
			{
				if (fabs(p[k + n] - matrixReference[m*yDimension + n]) > 1)
				{
					//printf("failure %f, %f\n", p[k + n], matrixReference[m*yDimension + n]);
					bSuccess = false;

				    //break;
				}
			}
		}
        if ( bSuccess )
            printf( "succeeded\n" );

        g_pContext->Unmap(debugTex, 0 );

        SAFE_RELEASE(debugTex);
    }
	printf("Time taken: %.2fms\n", (double)(clock() - tStart));
    
    printf( "Cleaning up...\n" );
    SAFE_RELEASE(g_pMatrixASRV);
    SAFE_RELEASE(g_pMatrixBSRV);
    SAFE_RELEASE(g_pMatrixCUAV);
    SAFE_RELEASE(g_pMatrixATexture);
    SAFE_RELEASE(g_pMatrixBTexture);
    SAFE_RELEASE(g_pMatrixCTexture);
    SAFE_RELEASE( g_pCS );
    SAFE_RELEASE( g_pContext );
    SAFE_RELEASE( g_pDevice );
	delete[] matrixA;
	delete[] matrixB;
	delete[] matrixC;
	delete[] matrixReference;

    return 0;
}


//--------------------------------------------------------------------------------------
// Create the D3D device and device context suitable for running Compute Shaders(CS)
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT CreateComputeDevice( ID3D11Device** ppDeviceOut, ID3D11DeviceContext** ppContextOut, bool bForceRef )
{    
    *ppDeviceOut = nullptr;
    *ppContextOut = nullptr;
    
    HRESULT hr = S_OK;

    UINT uCreationFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef _DEBUG
    uCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL flOut;
    static const D3D_FEATURE_LEVEL flvl[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    
    bool bNeedRefDevice = false;
    if ( !bForceRef )
    {
        hr = D3D11CreateDevice( nullptr,                        // Use default graphics card
                                D3D_DRIVER_TYPE_HARDWARE,    // Try to create a hardware accelerated device
                                nullptr,                        // Do not use external software rasterizer module
                                uCreationFlags,              // Device creation flags
                                flvl,
                                sizeof(flvl) / sizeof(D3D_FEATURE_LEVEL),
                                D3D11_SDK_VERSION,           // SDK version
                                ppDeviceOut,                 // Device out
                                &flOut,                      // Actual feature level created
                                ppContextOut );              // Context out
        
        if ( SUCCEEDED( hr ) )
        {
            // A hardware accelerated device has been created, so check for Compute Shader support

            // If we have a device >= D3D_FEATURE_LEVEL_11_0 created, full CS5.0 support is guaranteed, no need for further checks
            if ( flOut < D3D_FEATURE_LEVEL_11_0 )            
            {
                // Otherwise, we need further check whether this device support CS4.x (Compute on 10)
                D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts;
                (*ppDeviceOut)->CheckFeatureSupport( D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts) );
                if ( !hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x )
                {
                    bNeedRefDevice = true;
                    printf( "No hardware Compute Shader capable device found, trying to create ref device.\n" );
                }
            }
        }
    }
    
    if ( bForceRef || FAILED(hr) || bNeedRefDevice )
    {
        // Either because of failure on creating a hardware device or hardware lacking CS capability, we create a ref device here

        SAFE_RELEASE( *ppDeviceOut );
        SAFE_RELEASE( *ppContextOut );
        
        hr = D3D11CreateDevice( nullptr,                        // Use default graphics card
                                D3D_DRIVER_TYPE_REFERENCE,   // Try to create a hardware accelerated device
                                nullptr,                        // Do not use external software rasterizer module
                                uCreationFlags,              // Device creation flags
                                flvl,
                                sizeof(flvl) / sizeof(D3D_FEATURE_LEVEL),
                                D3D11_SDK_VERSION,           // SDK version
                                ppDeviceOut,                 // Device out
                                &flOut,                      // Actual feature level created
                                ppContextOut );              // Context out
        if ( FAILED(hr) )
        {
            printf( "Reference rasterizer device create failure\n" );
            return hr;
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Compile and create the CS
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT CreateComputeShader( LPCWSTR pSrcFile, LPCSTR pFunctionName, 
                             ID3D11Device* pDevice, ID3D11ComputeShader** ppShaderOut )
{
    if ( !pDevice || !ppShaderOut )
        return E_INVALIDARG;

    // Finds the correct path for the shader file.
    // This is only required for this sample to be run correctly from within the Sample Browser,
    // in your own projects, these lines could be removed safely
    WCHAR str[MAX_PATH];
    HRESULT hr = FindDXSDKShaderFileCch( str, MAX_PATH, pSrcFile );
    if ( FAILED(hr) )
        return hr;
    
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

    const D3D_SHADER_MACRO defines[] = 
    {
#ifdef USE_STRUCTURED_BUFFERS
        "USE_STRUCTURED_BUFFERS", "1",
#endif

#ifdef TEST_DOUBLE
        "TEST_DOUBLE", "1",
#endif
        nullptr, nullptr
    };

    // We generally prefer to use the higher CS shader profile when possible as CS 5.0 is better performance on 11-class hardware
    LPCSTR pProfile = ( pDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ) ? "cs_5_0" : "cs_4_0";

    ID3DBlob* pErrorBlob = nullptr;
    ID3DBlob* pBlob = nullptr;
    hr = D3DCompileFromFile( str, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, pFunctionName, pProfile, 
                             dwShaderFlags, 0, &pBlob, &pErrorBlob );
    if ( FAILED(hr) )
    {
        if ( pErrorBlob )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );

        SAFE_RELEASE( pErrorBlob );
        SAFE_RELEASE( pBlob );    

        return hr;
    }    

    hr = pDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, ppShaderOut );

    SAFE_RELEASE( pErrorBlob );
    SAFE_RELEASE( pBlob );

#if defined(_DEBUG) || defined(PROFILE)
    if ( SUCCEEDED(hr) )
    {
        (*ppShaderOut)->SetPrivateData( WKPDID_D3DDebugObjectName, lstrlenA(pFunctionName), pFunctionName );
    }
#endif

    return hr;
}

_Use_decl_annotations_
HRESULT CreateTexture2D(_In_ ID3D11Device* pDevice, _In_ UINT xDimension, _In_ UINT yDimension, bool readonly, _In_ UINT32* initialData, _Outptr_ ID3D11Texture2D** pTexture2D)
{
	D3D11_TEXTURE2D_DESC Tex2DDesc = { 0 };
	Tex2DDesc.Width = xDimension;
	Tex2DDesc.Height = yDimension;
	Tex2DDesc.ArraySize = 1;
	Tex2DDesc.SampleDesc.Count = 1;
	Tex2DDesc.SampleDesc.Quality = 0;
	Tex2DDesc.MipLevels = 1;
	Tex2DDesc.Format = DXGI_FORMAT_R32_UINT;
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
	TexData.SysMemPitch = xDimension * sizeof(UINT32);

	return pDevice->CreateTexture2D(&Tex2DDesc, &TexData, pTexture2D);
}

_Use_decl_annotations_
HRESULT CreateTexture2DSRV(_In_ ID3D11Device* pDevice, _In_ ID3D11Texture2D* pTexture2D, _Outptr_ ID3D11ShaderResourceView** pSRVOut)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC srv;
	srv.Format = DXGI_FORMAT_R32_UINT;
	srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv.Texture2D.MostDetailedMip = 0;
	srv.Texture2D.MipLevels = 1;
	return pDevice->CreateShaderResourceView(pTexture2D, &srv, pSRVOut);
}

_Use_decl_annotations_
HRESULT CreateTexture2DUAV(_In_ ID3D11Device* pDevice, _In_ ID3D11Texture2D* pTexture2D, _Outptr_ ID3D11UnorderedAccessView** pUAVOut)
{
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav;
	uav.Format = DXGI_FORMAT_R32_UINT;
	uav.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uav.Texture2D.MipSlice = 0;
	return pDevice->CreateUnorderedAccessView(pTexture2D, &uav, pUAVOut);
}

//--------------------------------------------------------------------------------------
// Create a CPU accessible buffer and download the content of a GPU buffer into it
// This function is very useful for debugging CS programs
//-------------------------------------------------------------------------------------- 
_Use_decl_annotations_
ID3D11Texture2D* CreateAndCopyToDebugTex2D( ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Texture2D* pTexture2D )
{
	ID3D11Texture2D* debugTex = nullptr;

	D3D11_TEXTURE2D_DESC desc = {};
	pTexture2D->GetDesc(&desc);
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	if (SUCCEEDED(pDevice->CreateTexture2D(&desc, nullptr, &debugTex)))
	{
		pd3dImmediateContext->CopyResource(debugTex, pTexture2D);
	}
	return debugTex;
}

//--------------------------------------------------------------------------------------
// Run CS
//-------------------------------------------------------------------------------------- 
_Use_decl_annotations_
void RunComputeShader( ID3D11DeviceContext* pd3dImmediateContext,
                      ID3D11ComputeShader* pComputeShader,
                      UINT nNumViews, ID3D11ShaderResourceView** pShaderResourceViews, 
                      ID3D11Buffer* pCBCS, void* pCSData, DWORD dwNumDataBytes,
                      ID3D11UnorderedAccessView* pUnorderedAccessView,
                      UINT X, UINT Y, UINT Z )
{
    pd3dImmediateContext->CSSetShader( pComputeShader, nullptr, 0 );
    pd3dImmediateContext->CSSetShaderResources( 0, nNumViews, pShaderResourceViews );
    pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &pUnorderedAccessView, nullptr );
    if ( pCBCS && pCSData )
    {
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dImmediateContext->Map( pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        memcpy( MappedResource.pData, pCSData, dwNumDataBytes );
        pd3dImmediateContext->Unmap( pCBCS, 0 );
        ID3D11Buffer* ppCB[1] = { pCBCS };
        pd3dImmediateContext->CSSetConstantBuffers( 0, 1, ppCB );
    }

    pd3dImmediateContext->Dispatch( X, Y, Z );

    pd3dImmediateContext->CSSetShader( nullptr, nullptr, 0 );

    ID3D11UnorderedAccessView* ppUAViewnullptr[1] = { nullptr };
    pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewnullptr, nullptr );

    ID3D11ShaderResourceView* ppSRVnullptr[2] = { nullptr, nullptr };
    pd3dImmediateContext->CSSetShaderResources( 0, 2, ppSRVnullptr );

    ID3D11Buffer* ppCBnullptr[1] = { nullptr };
    pd3dImmediateContext->CSSetConstantBuffers( 0, 1, ppCBnullptr );
}

//--------------------------------------------------------------------------------------
// Tries to find the location of the shader file
// This is a trimmed down version of DXUTFindDXSDKMediaFileCch.
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT FindDXSDKShaderFileCch( WCHAR* strDestPath,
                                int cchDest, 
                                LPCWSTR strFilename )
{
    if( !strFilename || strFilename[0] == 0 || !strDestPath || cchDest < 10 )
        return E_INVALIDARG;

    // Get the exe name, and exe path
    WCHAR strExePath[MAX_PATH] =
    {
        0
    };
    WCHAR strExeName[MAX_PATH] =
    {
        0
    };
    WCHAR* strLastSlash = nullptr;
    GetModuleFileName( nullptr, strExePath, MAX_PATH );
    strExePath[MAX_PATH - 1] = 0;
    strLastSlash = wcsrchr( strExePath, TEXT( '\\' ) );
    if( strLastSlash )
    {
        wcscpy_s( strExeName, MAX_PATH, &strLastSlash[1] );

        // Chop the exe name from the exe path
        *strLastSlash = 0;

        // Chop the .exe from the exe name
        strLastSlash = wcsrchr( strExeName, TEXT( '.' ) );
        if( strLastSlash )
            *strLastSlash = 0;
    }

    // Search in directories:
    //      .\
    //      %EXE_DIR%\..\..\%EXE_NAME%

    wcscpy_s( strDestPath, cchDest, strFilename );
    if( GetFileAttributes( strDestPath ) != 0xFFFFFFFF )
        return S_OK;

    swprintf_s( strDestPath, cchDest, L"%s\\..\\..\\%s\\%s", strExePath, strExeName, strFilename );
    if( GetFileAttributes( strDestPath ) != 0xFFFFFFFF )
        return S_OK;    

    // On failure, return the file as the path but also return an error code
    wcscpy_s( strDestPath, cchDest, strFilename );

    return E_FAIL;
}