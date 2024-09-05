//
// Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//--------------------------------------------------------------------------------------
// File: Sample.cpp
//--------------------------------------------------------------------------------------

#include "DXUT.h"
#include "DXUTCamera.h"
#include "DXUTGui.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"
#include "../../ffx_antilag2_dx11.h"

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds


//--------------------------------------------------------------------------------------
// Modified D3DSettingsDlg which will move the dialog to the main display instead of  
// showing the whole screen dialog.
//--------------------------------------------------------------------------------------
class CEF_D3DSettingsDlg: public CD3DSettingsDlg
{
public:
    HRESULT             OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
};
//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CFirstPersonCamera                  g_Camera;					// A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager;	// manager for shared resources of dialogs
CEF_D3DSettingsDlg                  g_D3DSettingsDlg;			// modified device settings dialog which will always shows on the main display
CDXUTDialog                         g_HUD;						// manages the 3D UI
CDXUTDialog                         g_SampleUI;					// dialog for sample specific controls
CDXUTTextHelper*                    g_pTxtHelper = nullptr;
UINT                                g_iWidth;
UINT                                g_iHeight;

#define NUM_MICROSCOPE_INSTANCES 6

CDXUTSDKMesh                        g_CityMesh;
CDXUTSDKMesh                        g_HeavyMesh;
CDXUTSDKMesh                        g_ColumnMesh;

ID3D11Buffer*						g_pConstantBuffer = nullptr;
ID3D11InputLayout*                  g_pVertexLayout = nullptr;
ID3D11SamplerState*					g_pSampleLinear = nullptr;
// Scene Shaders
ID3D11VertexShader*					g_pSceneVS = nullptr;
ID3D11PixelShader*					g_pScenePS = nullptr;

RECT								g_MainDisplayRect;
DirectX::XMMATRIX					g_SingleCameraProjM;

AMD::AntiLag2DX11::Context          g_AntiLagContext = {};
bool                                g_AntiLagAvailable = false;

bool                                g_AntiLagEnabled = false;
CDXUTCheckBox*                      g_AntiLagEnabledCheckBox = nullptr;

bool                                g_AntiLagLimiterEnabled = false;
CDXUTCheckBox*                      g_AntiLagLimiterCheckBox = nullptr;
CDXUTSlider*                        g_AntiLagLimiterSlider = nullptr;
int                                 g_AntiLagLimiterValue = 0;
CDXUTStatic*                        g_AntiLagLimiterText = nullptr;

bool                                g_AntiLagTestingMode = false;


int MapLimiterSliderToFPS( int sliderValue )
{
    // Map to 50fps+ range
    return sliderValue == 0 ? 0 : sliderValue + 49;
}

//--------------------------------------------------------------------------------------
// Retrieve the main display info and place the dialog on main display 
// whenever the resolution is changed.
//--------------------------------------------------------------------------------------
HRESULT CEF_D3DSettingsDlg::OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    m_Dialog.SetLocation( g_MainDisplayRect.left, g_MainDisplayRect.top );
    m_Dialog.SetSize( g_MainDisplayRect.right-g_MainDisplayRect.left, g_MainDisplayRect.bottom-g_MainDisplayRect.top );	
    m_Dialog.SetBackgroundColors( D3DCOLOR_ARGB( 255, 98, 138, 206 ),
                                  D3DCOLOR_ARGB( 255, 54, 105, 192 ),
                                  D3DCOLOR_ARGB( 255, 54, 105, 192 ),
                                  D3DCOLOR_ARGB( 255, 10,  73, 179 ) );

    m_RevertModeDialog.SetLocation( g_MainDisplayRect.left, g_MainDisplayRect.top );
    m_RevertModeDialog.SetSize( g_MainDisplayRect.right-g_MainDisplayRect.left, g_MainDisplayRect.bottom-g_MainDisplayRect.top );
    m_RevertModeDialog.SetBackgroundColors( D3DCOLOR_ARGB( 255, 98, 138, 206 ),
                                            D3DCOLOR_ARGB( 255, 54, 105, 192 ),
                                            D3DCOLOR_ARGB( 255, 54, 105, 192 ),
                                            D3DCOLOR_ARGB( 255, 10,  73, 179 ) );

    return S_OK;
}
//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
enum
{
    IDC_TOGGLEFULLSCREEN = 1,
    IDC_CHANGEDEVICE,
    IDC_ANTILAG_ENABLED,
    IDC_ANTILAG_LIMITER_ENABLED,
    IDC_ANTILAG_LIMITER_SLIDER,
    IDC_ANTILAG_LIMITER_TEXT,
    IDC_ANTILAG_HELPTEXT,
};


static const wchar_t* gHelpText0 = L"Press M to enable FLM testing mode.\nThis locks the mouse to the camera";
static const wchar_t* gHelpText1 = L"Press M to disable FLM testing mode.";


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

// Direct3D 11 callbacks
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
HRESULT CALLBACK OnD3D11SwapChainResized( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );

void CALLBACK PreMessagePump( void* pUserContext );

void InitApp();
void RenderText();

//--------------------------------------------------------------------------------------
// Helper function to compile an hlsl shader from file, 
// its binary compiled code is returned
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, 
                               LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D10_SHADER_MACRO* pDefines )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    // open the file
    HANDLE hFile = CreateFile( str, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        return E_FAIL;

    // Get the file size
    LARGE_INTEGER FileSize;
    GetFileSizeEx( hFile, &FileSize );

    // create enough space for the file data
    BYTE* pFileData = new BYTE[ FileSize.LowPart ];
    if( !pFileData )
        return E_OUTOFMEMORY;

    // read the data in
    DWORD BytesRead;
    if( !ReadFile( hFile, pFileData, FileSize.LowPart, &BytesRead, NULL ) )
        return E_FAIL; 

    CloseHandle( hFile );

    // Compile the shader
    char pFilePathName[MAX_PATH];        
    WideCharToMultiByte(CP_ACP, 0, str, -1, pFilePathName, MAX_PATH, NULL, NULL);
    ID3DBlob* pErrorBlob;
    hr = D3DCompile( pFileData, FileSize.LowPart, pFilePathName, pDefines, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, 0, ppBlobOut, &pErrorBlob );

    delete []pFileData;

    if( FAILED(hr) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
   _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device 
    // that is available on the system depending on which D3D callbacks are set below
    DXUTSetIsInGammaCorrectMode( false );

    // Set DXUT callbacks
    DXUTSetCallbackPreMessagePump( PreMessagePump );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11SwapChainResized );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );

    InitApp();
    DXUTInit( true, true ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"Anti-Lag 2.0 DX11 Sample v1.0" );

    int width = 1920;
    int height = 1080;
    bool windowed = true;

    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, windowed, width, height );

    DXUTMainLoop(); // Enter into the DXUT render loop	

    return DXUTGetExitCode();
}
//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );
    g_SampleUI.GetFont( 0 );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 260, iY, 150, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 260, iY += 24, 150, 22, VK_F2 );

    g_HUD.AddCheckBox( IDC_ANTILAG_ENABLED, L"Toggle Anti-Lag 2.0", 5, iY += 24, 250, 22, g_AntiLagEnabled, 0, false, &g_AntiLagEnabledCheckBox );
    g_HUD.AddCheckBox( IDC_ANTILAG_LIMITER_ENABLED, L"Anti-Lag 2.0 Framerate Limiter", 5, iY += 24, 250, 22, g_AntiLagLimiterEnabled, 0, false, &g_AntiLagLimiterCheckBox );
    g_HUD.AddSlider( IDC_ANTILAG_LIMITER_SLIDER, 5, iY += 24, 250, 22, 0, 251, g_AntiLagLimiterValue, false, &g_AntiLagLimiterSlider );
    g_HUD.AddStatic( IDC_ANTILAG_LIMITER_TEXT, L"", 265, iY, 50, 22, false, &g_AntiLagLimiterText );
    g_HUD.AddStatic( IDC_ANTILAG_HELPTEXT, g_AntiLagTestingMode ? gHelpText1 : gHelpText0, 5, iY += 24, 250, 22 );

    g_SampleUI.SetCallback( OnGUIEvent );
}


void CALLBACK PreMessagePump( void* pUserContext )
{
    if ( g_AntiLagAvailable )
    {
        AMD::AntiLag2DX11::Update( &g_AntiLagContext, g_AntiLagEnabled, g_AntiLagLimiterValue );
    }
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    pDeviceSettings->d3d11.SyncInterval = 0;
    g_D3DSettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_D3D11_PRESENT_INTERVAL )->SetEnabled( false );

    return true;
}
//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    g_Camera.SetRotateButtons( true, false, false, g_AntiLagTestingMode );

     // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}
//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}
//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if ( bKeyDown && nChar == 'M' )
    {
        g_AntiLagTestingMode ^= 1;
        g_HUD.GetStatic( IDC_ANTILAG_HELPTEXT )->SetText( g_AntiLagTestingMode ? gHelpText1 : gHelpText0 );
    }
}


void UpdateSlider()
{
    g_AntiLagLimiterValue = MapLimiterSliderToFPS( g_AntiLagLimiterSlider->GetValue() );

    if ( g_AntiLagLimiterValue > 0 )
    {
        wchar_t valueString[64] = {};
        swprintf_s( valueString, _countof( valueString ), L"%d fps", g_AntiLagLimiterValue );
        g_AntiLagLimiterText->SetText( valueString );
    }
    else
    {
        g_AntiLagLimiterText->SetText( L"Off" );
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); 
            break;
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); 
            break;
        case IDC_ANTILAG_ENABLED:
            g_AntiLagEnabled = g_AntiLagEnabledCheckBox->GetChecked();
            g_AntiLagLimiterCheckBox->SetEnabled( g_AntiLagEnabled );
            break;

        case IDC_ANTILAG_LIMITER_ENABLED:
            g_AntiLagLimiterEnabled = g_AntiLagLimiterCheckBox->GetChecked();
            g_AntiLagLimiterSlider->SetEnabled( g_AntiLagLimiterEnabled );
            if ( !g_AntiLagLimiterEnabled )
            {
                g_AntiLagLimiterSlider->SetValue( 0 );
            }
            UpdateSlider();
            break;

        case IDC_ANTILAG_LIMITER_SLIDER:
            UpdateSlider();
            break;
    }
}
//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}
//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

     // Get device context
    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );

     g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    ID3DBlob* pBlob = NULL;

    // Main scene VS
    V_RETURN( CompileShaderFromFile( L"..\\src\\Shaders\\Sample.hlsl", "VSScenemain", "vs_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSceneVS ) );
    // Define our scene vertex data layout
    const D3D11_INPUT_ELEMENT_DESC SceneLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( pd3dDevice->CreateInputLayout( SceneLayout, ARRAYSIZE( SceneLayout ), pBlob->GetBufferPointer(),
                                                pBlob->GetBufferSize(), &g_pVertexLayout ) );
    SAFE_RELEASE( pBlob );

    // Main scene PS
    V_RETURN( CompileShaderFromFile( L"..\\src\\Shaders\\Sample.hlsl", "PSScenemain", "ps_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pScenePS ) );
    SAFE_RELEASE( pBlob );

    // Setup constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.ByteWidth = sizeof( DirectX::XMMATRIX );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pConstantBuffer ) );

    // Create sampler states 
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSampleLinear ) )

    // Load the Meshes
    g_CityMesh.Create( pd3dDevice, L"media\\MicroscopeCity\\occcity.sdkmesh", false );
    g_HeavyMesh.Create( pd3dDevice, L"media\\MicroscopeCity\\scanner.sdkmesh", false );
    g_ColumnMesh.Create( pd3dDevice, L"media\\MicroscopeCity\\column.sdkmesh", false );

    // Setup the camera's view parameters
    DirectX::XMVECTOR vecEye = DirectX::XMVectorSet( 0.0f, 0.5f, -1.3f, 0.0f );
    DirectX::XMVECTOR vecAt = DirectX::XMVectorSet( 0.0f, 0.5f,  0.0f, 0.0f );
    g_Camera.SetViewParams( vecEye, vecAt );
    g_Camera.SetRotateButtons(true, false, false);
    g_Camera.SetEnableYAxisMovement( false );

    if ( AMD::AntiLag2DX11::Initialize( &g_AntiLagContext ) == S_OK )
    {
        g_AntiLagAvailable = true;
        g_AntiLagEnabled = true;
    }

    g_AntiLagEnabledCheckBox->SetEnabled( g_AntiLagAvailable );
    g_AntiLagEnabledCheckBox->SetChecked( g_AntiLagEnabled );

    g_AntiLagLimiterCheckBox->SetEnabled( g_AntiLagAvailable && g_AntiLagEnabled );
    g_AntiLagLimiterSlider->SetEnabled( false );

    return S_OK;
}
//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11SwapChainResized( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr = S_OK;

    g_iWidth = pBackBufferSurfaceDesc->Width;
    g_iHeight = pBackBufferSurfaceDesc->Height;

    g_MainDisplayRect.left	 = 0;
    g_MainDisplayRect.right  = g_iWidth;
    g_MainDisplayRect.top	 = 0;
    g_MainDisplayRect.bottom = g_iHeight;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    float YFOV = DirectX::XM_PI/4.0f;

    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    // The projection matrix for the camera of single camera mode.
    g_SingleCameraProjM = DirectX::XMMatrixPerspectiveFovLH( YFOV, fAspectRatio, 0.01f, 500.0f );
    
    // Locate the HUD and UI based on the area of main display.
    g_HUD.SetLocation( g_MainDisplayRect.right - 500, g_MainDisplayRect.top );
    g_HUD.SetSize( 500, 170 );
    g_SampleUI.SetLocation( g_MainDisplayRect.right - 170, g_MainDisplayRect.top + 300 );
    g_SampleUI.SetSize( 170, 170 );

    return hr;
}
//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK RenderScene( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, DirectX::XMMATRIX& vpm )
{
    DirectX::XMMATRIX mWorldViewProj = vpm;

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dImmediateContext->Map( g_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    DirectX::XMMATRIX* pWVPM = ( DirectX::XMMATRIX* )MappedResource.pData;
    *pWVPM = DirectX::XMMatrixTranspose( mWorldViewProj );
    pd3dImmediateContext->Unmap( g_pConstantBuffer, 0 );

    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pConstantBuffer );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSampleLinear );
    pd3dImmediateContext->VSSetShader( g_pSceneVS, NULL, 0 );
    pd3dImmediateContext->PSSetShader( g_pScenePS, NULL, 0 );

    // Render the city
    g_CityMesh.Render( pd3dImmediateContext, 0 );
    g_ColumnMesh.Render( pd3dImmediateContext, 0 );
    for( int i = 0; i < NUM_MICROSCOPE_INSTANCES; i++ )
    {
        DirectX::XMMATRIX mMatRot = DirectX::XMMatrixRotationY( i * ( DirectX::XM_PI / 3.0f ) );
        DirectX::XMMATRIX mWVP = mMatRot * mWorldViewProj;
        pd3dImmediateContext->Map( g_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        DirectX::XMMATRIX* pWVPMInstance = ( DirectX::XMMATRIX* )MappedResource.pData;
        *pWVPMInstance = DirectX::XMMatrixTranspose( mWVP );
        pd3dImmediateContext->Unmap( g_pConstantBuffer, 0 );

        for( int j = 0; j < 100; j++ )
        {
            g_HeavyMesh.Render( pd3dImmediateContext, 0 );
        }
    }
}
//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext )
{
    // Clear the render target
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    float ClearColor[4] = { 0.369f, 0.369f, 0.369f, 0.0f };
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    D3D11_VIEWPORT Viewport;
    DirectX::XMMATRIX ViewM, VPM;

    VPM = g_Camera.GetViewMatrix() * g_SingleCameraProjM;
    RenderScene(pd3dDevice, pd3dImmediateContext, VPM);

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    g_SampleUI.OnRender( fElapsedTime );
    g_HUD.OnRender( fElapsedTime );
    DXUT_EndPerfEvent();
}
//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( g_MainDisplayRect.left, g_MainDisplayRect.top );
    g_pTxtHelper->SetForegroundColor( DirectX::XMVectorSet( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
}
//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}
//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    AMD::AntiLag2DX11::DeInitialize( &g_AntiLagContext );
    g_AntiLagAvailable = false;
    g_AntiLagEnabled = false;

    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    SAFE_DELETE( g_pTxtHelper );

    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pConstantBuffer );
    SAFE_RELEASE( g_pSceneVS );
    SAFE_RELEASE( g_pScenePS );
    SAFE_RELEASE( g_pSampleLinear );

    g_CityMesh.Destroy();
    g_HeavyMesh.Destroy();
    g_ColumnMesh.Destroy();
}