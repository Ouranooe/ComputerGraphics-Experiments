#include <windows.h>
#include <d2d1.h>
#pragma comment(lib, "d2d1.lib")
#include "CoordinateTransform.h"

ID2D1Factory* pFactory = nullptr;
ID2D1HwndRenderTarget* pRenderTarget = nullptr;
ID2D1SolidColorBrush* pBrush = nullptr;

CoordinateTransform coord;

// 创建渲染目标
HRESULT CreateGraphicsResources(HWND hwnd)
{
    HRESULT hr = S_OK;
    if (pRenderTarget == nullptr)
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &pRenderTarget
        );

        if (SUCCEEDED(hr))
        {
            // 创建画刷
            hr = pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Black), &pBrush);
        }

        if (SUCCEEDED(hr))
        {
            coord.Initialize(pRenderTarget); // 初始化坐标变换
        }
    }
    return hr;
}

void DiscardGraphicsResources()
{
    if (pBrush) { pBrush->Release(); pBrush = nullptr; }
    if (pRenderTarget) { pRenderTarget->Release(); pRenderTarget = nullptr; }
}

void OnPaint(HWND hwnd)
{
    HRESULT hr = CreateGraphicsResources(hwnd);
    if (FAILED(hr)) return;

    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);

    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // ===== 坐标系绘制 =====

    // 原点标记
    float r = 4.0f;
    D2D1_ELLIPSE origin = D2D1::Ellipse(D2D1::Point2F(0, 0), r, r);
    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
    pRenderTarget->FillEllipse(origin, pBrush);

    // 坐标轴线
    D2D1_SIZE_F rtSize = pRenderTarget->GetSize();
    float halfWidth = rtSize.width / 2.0f;
    float halfHeight = rtSize.height / 2.0f;

    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Gray));

    // X轴（水平线）
    pRenderTarget->DrawLine(
        D2D1::Point2F(-halfWidth, 0),
        D2D1::Point2F(halfWidth, 0),
        pBrush, 1.5f);

    // Y轴（垂直线）
    pRenderTarget->DrawLine(
        D2D1::Point2F(0, -halfHeight),
        D2D1::Point2F(0, halfHeight),
        pBrush, 1.5f);

    // X轴箭头
    pRenderTarget->DrawLine(D2D1::Point2F(halfWidth - 10, 5), D2D1::Point2F(halfWidth, 0), pBrush, 1.0f);
    pRenderTarget->DrawLine(D2D1::Point2F(halfWidth - 10, -5), D2D1::Point2F(halfWidth, 0), pBrush, 1.0f);

    // Y轴箭头
    pRenderTarget->DrawLine(D2D1::Point2F(-5, halfHeight - 10), D2D1::Point2F(0, halfHeight), pBrush, 1.0f);
    pRenderTarget->DrawLine(D2D1::Point2F(5, halfHeight - 10), D2D1::Point2F(0, halfHeight), pBrush, 1.0f);

    hr = pRenderTarget->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardGraphicsResources();
    }

    EndPaint(hwnd, &ps);
}

void OnResize(UINT width, UINT height)
{
    if (pRenderTarget)
    {
        pRenderTarget->Resize(D2D1::SizeU(width, height));
        coord.OnResize(); // 更新坐标变换
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
        return 0;

    case WM_PAINT:
        OnPaint(hwnd);
        return 0;

    case WM_SIZE:
        OnResize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_DESTROY:
        DiscardGraphicsResources();
        if (pFactory) { pFactory->Release(); pFactory = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"D2D Coordinate Test";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Direct2D 坐标系演示",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
