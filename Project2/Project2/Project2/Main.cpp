#include "framework.h"
#include <windowsx.h>

#include "GraphicsEngine.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void CreateMenuSystem(HWND hwnd);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASS wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ComputerGraphicsLab";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        return 0;
    }

    HWND hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        L"2023115323侯懿",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        GraphicsEngine::WINDOW_WIDTH,
        GraphicsEngine::WINDOW_HEIGHT,
        nullptr,
        nullptr,
        hInstance,
        nullptr);
    if (!hwnd) {
        return 0;
    }

    CreateMenuSystem(hwnd);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        GraphicsEngine::Initialize(hwnd);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        GraphicsEngine::HandleCommand(id);
    } return 0;

    case WM_LBUTTONDOWN:
        GraphicsEngine::HandleLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        GraphicsEngine::HandleMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEWHEEL:
        GraphicsEngine::HandleMouseWheel(static_cast<short>(GET_WHEEL_DELTA_WPARAM(wParam)));
        return 0;

    case WM_SIZE:
        GraphicsEngine::Resize(hwnd);
        return 0;

    case WM_PAINT:
        GraphicsEngine::OnPaint(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        GraphicsEngine::Shutdown();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

static void CreateMenuSystem(HWND hwnd) {
    HMENU hMenu = CreateMenu();
    HMENU hDrawMenu = CreateMenu();
    HMENU hFillMenu = CreateMenu();
    HMENU hEditMenu = CreateMenu();
    HMENU hTransMenu = CreateMenu();
    HMENU hClipLineMenu = CreateMenu();
    HMENU hClipPolyMenu = CreateMenu();

    AppendMenu(hDrawMenu, MF_STRING, GraphicsEngine::ID_DRAW_LINE_MIDPOINT, L"直线 (中点法)");
    AppendMenu(hDrawMenu, MF_STRING, GraphicsEngine::ID_DRAW_LINE_BRESENHAM, L"直线 (Bresenham)");
    AppendMenu(hDrawMenu, MF_STRING, GraphicsEngine::ID_DRAW_CIRCLE_MIDPOINT, L"圆 (中点法)");
    AppendMenu(hDrawMenu, MF_STRING, GraphicsEngine::ID_DRAW_CIRCLE_BRESENHAM, L"圆 (Bresenham)");
    AppendMenu(hDrawMenu, MF_STRING, GraphicsEngine::ID_DRAW_RECTANGLE, L"矩形");
    AppendMenu(hDrawMenu, MF_STRING, GraphicsEngine::ID_DRAW_POLYGON, L"多边形");
    AppendMenu(hDrawMenu, MF_STRING, GraphicsEngine::ID_DRAW_BSPLINE, L"B样条曲线");
    AppendMenu(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hDrawMenu), L"绘图");

    AppendMenu(hFillMenu, MF_STRING, GraphicsEngine::ID_FILL_SCANLINE, L"扫描线填充");
    AppendMenu(hFillMenu, MF_STRING, GraphicsEngine::ID_FILL_FENCE, L"栅栏填充(无种子)");
    AppendMenu(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFillMenu), L"填充");

    AppendMenu(hTransMenu, MF_STRING, GraphicsEngine::ID_TRANS_TRANSLATE, L"平移");
    AppendMenu(hTransMenu, MF_STRING, GraphicsEngine::ID_TRANS_SCALE, L"缩放(含滚轮)");
    AppendMenu(hTransMenu, MF_STRING, GraphicsEngine::ID_TRANS_ROTATE, L"旋转(绕鼠标点)");
    AppendMenu(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hTransMenu), L"变换");

    AppendMenu(hClipLineMenu, MF_STRING, GraphicsEngine::ID_CLIP_LINE_CS, L"直线裁剪 - Cohen-Sutherland");
    AppendMenu(hClipLineMenu, MF_STRING, GraphicsEngine::ID_CLIP_LINE_MID, L"直线裁剪 - 中点分割");
    AppendMenu(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hClipLineMenu), L"线裁剪");

    AppendMenu(hClipPolyMenu, MF_STRING, GraphicsEngine::ID_CLIP_POLY_SH, L"多边形裁剪 - Sutherland-Hodgman");
    AppendMenu(hClipPolyMenu, MF_STRING, GraphicsEngine::ID_CLIP_POLY_WA, L"多边形裁剪 - Weiler-Atherton");
    AppendMenu(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hClipPolyMenu), L"多边形裁剪");

    AppendMenu(hEditMenu, MF_STRING, GraphicsEngine::ID_EDIT_FINISH, L"完成当前图形");
    AppendMenu(hEditMenu, MF_STRING, GraphicsEngine::ID_EDIT_CLEAR, L"清空画布");
    AppendMenu(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hEditMenu), L"编辑");

    SetMenu(hwnd, hMenu);
}
