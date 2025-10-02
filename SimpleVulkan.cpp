// SimpleVulkan.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "SimpleVulkan.h"
#include "VulkanRender.h"

#include <chrono>

#define MAX_LOADSTRING 100

// Global Variables:
static std::unique_ptr<VulkanRender> gVulkanRender = nullptr;
HINSTANCE hInst;                                // current instance
static HWND gHwnd = nullptr;

WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int, uint32_t, uint32_t, bool);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

// Frame counter to display fps
uint32_t gFrameCounter = 0;
uint32_t lastFPS = 0;
std::chrono::time_point<std::chrono::high_resolution_clock> gLastTimestamp;

bool resizing = false;

int screenWidth;
int screenHeight;


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_SIMPLEVULKAN, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // FULL SCREEN toggle
    bool fullScreen = false;

    if (fullScreen)
    {
        DEVMODE dmScreenSettings;
        memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
        dmScreenSettings.dmSize = sizeof(dmScreenSettings);
        dmScreenSettings.dmPelsWidth = screenWidth;
        dmScreenSettings.dmPelsHeight = screenHeight;
        dmScreenSettings.dmBitsPerPel = 32;
        dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
        if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
        {
            if (MessageBox(NULL, L"Fullscreen Mode not supported!\n Switch to window mode?", L"Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
            {
//                    settings.fullscreen = false;
            }
            else
            {
//                  return nullptr;
            }
        }
    }
    else
    {
        screenWidth = 1280;
        screenHeight = 720;
    }


    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow, screenWidth, screenHeight, fullScreen))
    {
        return FALSE;
    }

//    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SIMPLEVULKAN));

    gVulkanRender = std::make_unique<VulkanRender>();
    gVulkanRender->Init(hInstance, gHwnd, screenWidth, screenHeight);

    MSG msg;

    gLastTimestamp = std::chrono::high_resolution_clock::now();

    // Main message loop:
    bool quitMessageReceived = false;
    while (!quitMessageReceived) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                quitMessageReceived = true;
                break;
            }
        }

        // MAIN: render Vulkan3D
        if (!IsIconic(gHwnd) && gVulkanRender->IsPrepared())
        {
            gFrameCounter++;

            auto tNow = std::chrono::high_resolution_clock::now();

            float deltaTime = std::chrono::duration_cast<std::chrono::duration<float>>(tNow - gLastTimestamp).count();
            gVulkanRender->RenderFrame(deltaTime);
            gLastTimestamp = tNow;
        }
    }

    gVulkanRender->Finalize();

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
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
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SIMPLEVULKAN));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName = 0;// MAKEINTRESOURCEW(IDC_SIMPLEVULKAN);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SIMPLEVULKAN));

    return RegisterClassExW(&wcex);
}


BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, uint32_t destWidth, uint32_t destHeight, bool fullScreen)
{
   hInst = hInstance; // Store instance handle in our global variable

   DWORD dwExStyle;
   DWORD dwStyle;
   if (fullScreen)
   {
       dwExStyle = WS_EX_APPWINDOW;
       dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
   }
   else
   {
       dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
       dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
   }

   RECT windowRect{
       .left = 0L,
       .top = 0L,
       .right = (long)destWidth,
       .bottom = (long)destHeight
   };
   AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);


   gHwnd = CreateWindowW(szWindowClass, szTitle,
       dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
       CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
       nullptr, nullptr, hInstance, nullptr);

   if (!gHwnd)
   {
      return FALSE;
   }

   ShowWindow(gHwnd, nCmdShow);
   UpdateWindow(gHwnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ENTERSIZEMOVE:
        resizing = true;
        break;
    case WM_EXITSIZEMOVE:
        resizing = false;
        break;
    case WM_SIZE:
        if (gVulkanRender.get() && (gVulkanRender->IsPrepared()) && (wParam != SIZE_MINIMIZED))
        {
            if ((resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
            {
                gVulkanRender->HandleWindowResize(LOWORD(lParam), HIWORD(lParam));
            }
        }
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
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
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        gVulkanRender->ClearPrepared();
        DestroyWindow(hWnd);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
