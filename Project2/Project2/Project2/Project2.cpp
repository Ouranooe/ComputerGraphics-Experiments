#include <windows.h>
#include <vector>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

 //-----------------------------------------------------------------------------
 // 1. 常量 & 菜单
 //-----------------------------------------------------------------------------
#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

#define ID_DRAW_LINE_MIDPOINT    1001
#define ID_DRAW_LINE_BRESENHAM   1002
#define ID_DRAW_CIRCLE_MIDPOINT  1003
#define ID_DRAW_CIRCLE_BRESENHAM 1004
#define ID_DRAW_RECTANGLE        1005
#define ID_DRAW_POLYGON          1006
#define ID_DRAW_BSPLINE          1007
#define ID_FILL_SCANLINE         1008
#define ID_FILL_FENCE            1009
#define ID_EDIT_FINISH           1010
#define ID_EDIT_CLEAR            1011

//-----------------------------------------------------------------------------
// 2. 数据结构
//-----------------------------------------------------------------------------
enum class DrawMode {
    None,
    DrawLineMidpoint,
    DrawLineBresenham,
    DrawCircleMidpoint,
    DrawCircleBresenham,
    DrawRectangle,
    DrawPolygon,
    DrawBSpline,
    FillScanline,
    FillFence
};

struct Point {
    int x, y;
};

struct Shape {
    DrawMode type;
    std::vector<Point> vertices; // 对于矩形/圆：两个点；多边形：多个点
    COLORREF color;              // 边界颜色
    COLORREF fillColor;          // 填充颜色
    int fillMode;                // 0:无  1:扫描线  2:栅栏
};

// B样条基函数（均匀三次 B 样条）
void BSplineBase(float t, float* b) {
    float t2 = t * t;
    float t3 = t2 * t;
    b[0] = (-t3 + 3 * t2 - 3 * t + 1) / 6.0f;
    b[1] = (3 * t3 - 6 * t2 + 4) / 6.0f;
    b[2] = (-3 * t3 + 3 * t2 + 3 * t + 1) / 6.0f;
    b[3] = t3 / 6.0f;
}

//-----------------------------------------------------------------------------
// 3. 全局变量
//-----------------------------------------------------------------------------
HWND   g_hwnd = nullptr;
HDC    g_hdcMem = nullptr;
HBITMAP g_hbmMem = nullptr;

DrawMode g_currentMode = DrawMode::None;
std::vector<Shape> g_shapes;
std::vector<Point> g_currentPoints;
bool g_isDrawing = false;

COLORREF g_drawColor = RGB(0, 0, 0);
COLORREF g_fillColor = RGB(253, 151, 47);

//-----------------------------------------------------------------------------
// 4. 函数声明
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// 基础绘制
void DrawPixel(HDC, int, int, COLORREF);
void DrawPixelXor(HDC, int, int, COLORREF);          // 供栅栏填充使用（按颜色异或）
void DrawLineMidpoint(HDC, int, int, int, int, COLORREF);
void DrawLineBresenham(HDC, int, int, int, int, COLORREF);
void DrawCircleMidpoint(HDC, int, int, int, COLORREF);
void DrawCircleBresenham(HDC, int, int, int, COLORREF);
void DrawPolyline(HDC, const std::vector<Point>&, COLORREF, bool closed);
void DrawBSpline(HDC, const std::vector<Point>&, COLORREF);
void DrawShapeBorder(HDC, const Shape&);

// 填充相关（普通扫描线）
bool PointInShape(const Shape&, int, int);
void FillPolygonScanline(HDC, const std::vector<Point>&, COLORREF, bool innerOnly);
void FillRectScanline(HDC, const Point&, const Point&, COLORREF, bool innerOnly);
void FillCircleScanline(HDC, const Point&, const Point&, COLORREF, bool innerOnly);
void FillShapeScanline(HDC, const Shape&, COLORREF);

// 栅栏填充
void FenceFillRect(HDC, const Point&, const Point&, COLORREF xorColor);
void FenceFillCircle(HDC, const Point&, const Point&, COLORREF xorColor);
void FenceFillPolygon(HDC, const std::vector<Point>&, COLORREF xorColor);
void FillShapeFence(HDC, const Shape&, COLORREF);

// 交互 & 渲染
void CreateMenuSystem(HWND);
void OnPaint(HWND);
void OnLButtonDown(int, int);
void OnMouseMove(int, int);
void ClearCanvas();
void FinishDrawing();
void RedrawAllShapes(HDC);

//-----------------------------------------------------------------------------
// 5. WinMain
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"ComputerGraphicsLab";
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(
        0, L"ComputerGraphicsLab", L"2023115323侯懿",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInst, NULL
    );
    if (!g_hwnd) return 0;

    CreateMenuSystem(g_hwnd);
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    MSG msg{};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

//-----------------------------------------------------------------------------
// 6. 窗口过程
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HDC hdc = GetDC(hwnd);
        g_hdcMem = CreateCompatibleDC(hdc);
        RECT rc; GetClientRect(hwnd, &rc);
        g_hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        SelectObject(g_hdcMem, g_hbmMem);
        ReleaseDC(hwnd, hdc);
    } break;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case ID_DRAW_LINE_MIDPOINT:
            g_currentMode = DrawMode::DrawLineMidpoint;   g_currentPoints.clear(); g_isDrawing = false; break;
        case ID_DRAW_LINE_BRESENHAM:
            g_currentMode = DrawMode::DrawLineBresenham;  g_currentPoints.clear(); g_isDrawing = false; break;
        case ID_DRAW_CIRCLE_MIDPOINT:
            g_currentMode = DrawMode::DrawCircleMidpoint; g_currentPoints.clear(); g_isDrawing = false; break;
        case ID_DRAW_CIRCLE_BRESENHAM:
            g_currentMode = DrawMode::DrawCircleBresenham;g_currentPoints.clear(); g_isDrawing = false; break;
        case ID_DRAW_RECTANGLE:
            g_currentMode = DrawMode::DrawRectangle;      g_currentPoints.clear(); g_isDrawing = false; break;
        case ID_DRAW_POLYGON:
            g_currentMode = DrawMode::DrawPolygon;        g_currentPoints.clear(); g_isDrawing = true;  break;
        case ID_DRAW_BSPLINE:
            g_currentMode = DrawMode::DrawBSpline;        g_currentPoints.clear(); g_isDrawing = true;  break;
        case ID_FILL_SCANLINE:
            g_currentMode = DrawMode::FillScanline;       break;
        case ID_FILL_FENCE:
            g_currentMode = DrawMode::FillFence;          break;
        case ID_EDIT_FINISH:
            FinishDrawing();                              break;
        case ID_EDIT_CLEAR:
            ClearCanvas();                                break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    } break;

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        OnLButtonDown(x, y);
    } break;

    case WM_MOUSEMOVE: {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        OnMouseMove(x, y);
    } break;

    case WM_SIZE: {
        if (g_hdcMem) {
            DeleteObject(g_hbmMem);
            HDC hdc = GetDC(hwnd);
            RECT rc; GetClientRect(hwnd, &rc);
            g_hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
            SelectObject(g_hdcMem, g_hbmMem);
            ReleaseDC(hwnd, hdc);
            InvalidateRect(hwnd, NULL, FALSE);
        }
    } break;

    case WM_PAINT:
        OnPaint(hwnd); break;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        DeleteObject(g_hbmMem);
        DeleteDC(g_hdcMem);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

//-----------------------------------------------------------------------------
// 7. 基础绘制
//-----------------------------------------------------------------------------
void DrawPixel(HDC hdc, int x, int y, COLORREF c) {
    SetPixel(hdc, x, y, c);
}

// 按颜色异或一个像素：new = old XOR color
void DrawPixelXor(HDC hdc, int x, int y, COLORREF c) {
    COLORREF old = GetPixel(hdc, x, y);
    if (old == CLR_INVALID) {
        // 超出范围等情况，直接画成背景 ^ c (一般用不到)
        old = RGB(255, 255, 255);
    }
    BYTE r = GetRValue(old) ^ GetRValue(c);
    BYTE g = GetGValue(old) ^ GetGValue(c);
    BYTE b = GetBValue(old) ^ GetBValue(c);
    SetPixel(hdc, x, y, RGB(r, g, b));
}

void DrawLineMidpoint(HDC hdc, int x1, int y1, int x2, int y2, COLORREF c) {
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = (x2 >= x1) ? 1 : -1;
    int sy = (y2 >= y1) ? 1 : -1;
    int x = x1, y = y1;
    DrawPixel(hdc, x, y, c);
    if (dx > dy) {
        int d = 2 * dy - dx;
        for (int i = 0; i < dx; ++i) {
            x += sx;
            if (d < 0) d += 2 * dy;
            else { y += sy; d += 2 * (dy - dx); }
            DrawPixel(hdc, x, y, c);
        }
    }
    else {
        int d = 2 * dx - dy;
        for (int i = 0; i < dy; ++i) {
            y += sy;
            if (d < 0) d += 2 * dx;
            else { x += sx; d += 2 * (dx - dy); }
            DrawPixel(hdc, x, y, c);
        }
    }
}

void DrawLineBresenham(HDC hdc, int x1, int y1, int x2, int y2, COLORREF c) {
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = (x2 >= x1) ? 1 : -1;
    int sy = (y2 >= y1) ? 1 : -1;
    int x = x1, y = y1;
    DrawPixel(hdc, x, y, c);
    if (dx > dy) {
        int e = -dx;
        for (int i = 0; i < dx; ++i) {
            x += sx;
            e += 2 * dy;
            if (e >= 0) { y += sy; e -= 2 * dx; }
            DrawPixel(hdc, x, y, c);
        }
    }
    else {
        int e = -dy;
        for (int i = 0; i < dy; ++i) {
            y += sy;
            e += 2 * dx;
            if (e >= 0) { x += sx; e -= 2 * dy; }
            DrawPixel(hdc, x, y, c);
        }
    }
}

void DrawCirclePoints(HDC hdc, int xc, int yc, int x, int y, COLORREF c) {
    DrawPixel(hdc, xc + x, yc + y, c);
    DrawPixel(hdc, xc - x, yc + y, c);
    DrawPixel(hdc, xc + x, yc - y, c);
    DrawPixel(hdc, xc - x, yc - y, c);
    DrawPixel(hdc, xc + y, yc + x, c);
    DrawPixel(hdc, xc - y, yc + x, c);
    DrawPixel(hdc, xc + y, yc - x, c);
    DrawPixel(hdc, xc - y, yc - x, c);
}

void DrawCircleMidpoint(HDC hdc, int xc, int yc, int r, COLORREF c) {
    if (r <= 0) return;
    int x = 0, y = r;
    int d = 1 - r;
    DrawCirclePoints(hdc, xc, yc, x, y, c);
    while (x < y) {
        ++x;
        if (d < 0) d += 2 * x + 1;
        else { --y; d += 2 * (x - y) + 1; }
        DrawCirclePoints(hdc, xc, yc, x, y, c);
    }
}

void DrawCircleBresenham(HDC hdc, int xc, int yc, int r, COLORREF c) {
    if (r <= 0) return;
    int x = 0, y = r;
    int e = 3 - 2 * r;
    DrawCirclePoints(hdc, xc, yc, x, y, c);
    while (x < y) {
        if (e < 0) e += 4 * x + 6;
        else { e += 4 * (x - y) + 10; --y; }
        ++x;
        DrawCirclePoints(hdc, xc, yc, x, y, c);
    }
}

// 折线绘制：closed=true 时首尾相连（用于“完成后的多边形”）
void DrawPolyline(HDC hdc, const std::vector<Point>& v, COLORREF c, bool closed) {
    if (v.size() < 2) return;
    for (size_t i = 0; i + 1 < v.size(); ++i)
        DrawLineMidpoint(hdc, v[i].x, v[i].y, v[i + 1].x, v[i + 1].y, c);
    if (closed)
        DrawLineMidpoint(hdc, v.back().x, v.back().y, v.front().x, v.front().y, c);
}

// 均匀三次 B 样条（逼近型：一般不会经过中间控制点）
void DrawBSpline(HDC hdc, const std::vector<Point>& ctrl, COLORREF c) {
    if (ctrl.size() < 4) return;
    for (size_t i = 0; i + 3 < ctrl.size(); ++i) {
        Point p1 = ctrl[i];
        Point p2 = ctrl[i + 1];
        Point p3 = ctrl[i + 2];
        Point p4 = ctrl[i + 3];

        float b0[4];
        BSplineBase(0.0f, b0);
        Point last;
        last.x = int(b0[0] * p1.x + b0[1] * p2.x + b0[2] * p3.x + b0[3] * p4.x);
        last.y = int(b0[0] * p1.y + b0[1] * p2.y + b0[2] * p3.y + b0[3] * p4.y);

        for (float t = 0.01f; t <= 1.0f; t += 0.01f) {
            float b[4];
            BSplineBase(t, b);
            Point cur;
            cur.x = int(b[0] * p1.x + b[1] * p2.x + b[2] * p3.x + b[3] * p4.x);
            cur.y = int(b[0] * p1.y + b[1] * p2.y + b[2] * p3.y + b[3] * p4.y);
            DrawLineMidpoint(hdc, last.x, last.y, cur.x, cur.y, c);
            last = cur;
        }
    }
}

void DrawShapeBorder(HDC hdc, const Shape& s) {
    const auto& v = s.vertices;
    if (v.size() < 2) return;
    switch (s.type) {
    case DrawMode::DrawLineMidpoint:
        DrawLineMidpoint(hdc, v[0].x, v[0].y, v[1].x, v[1].y, s.color); break;
    case DrawMode::DrawLineBresenham:
        DrawLineBresenham(hdc, v[0].x, v[0].y, v[1].x, v[1].y, s.color); break;
    case DrawMode::DrawCircleMidpoint: {
        int r = int(std::sqrt(double((v[1].x - v[0].x) * (v[1].x - v[0].x) +
            (v[1].y - v[0].y) * (v[1].y - v[0].y))));
        DrawCircleMidpoint(hdc, v[0].x, v[0].y, r, s.color);
    } break;
    case DrawMode::DrawCircleBresenham: {
        int r = int(std::sqrt(double((v[1].x - v[0].x) * (v[1].x - v[0].x) +
            (v[1].y - v[0].y) * (v[1].y - v[0].y))));
        DrawCircleBresenham(hdc, v[0].x, v[0].y, r, s.color);
    } break;
    case DrawMode::DrawRectangle: {
        int x1 = min(v[0].x, v[1].x);
        int x2 = max(v[0].x, v[1].x);
        int y1 = min(v[0].y, v[1].y);
        int y2 = max(v[0].y, v[1].y);
        DrawLineMidpoint(hdc, x1, y1, x2, y1, s.color);
        DrawLineMidpoint(hdc, x2, y1, x2, y2, s.color);
        DrawLineMidpoint(hdc, x2, y2, x1, y2, s.color);
        DrawLineMidpoint(hdc, x1, y2, x1, y1, s.color);
    } break;
    case DrawMode::DrawPolygon:
        DrawPolyline(hdc, v, s.color, true);
        break;
    case DrawMode::DrawBSpline:
        DrawBSpline(hdc, v, s.color);
        break;
    default: break;
    }
}

//-----------------------------------------------------------------------------
// 8. 填充算法
//-----------------------------------------------------------------------------

// 判断点击点是否大致在图形内部（仅用于选中要填充的图形）
bool PointInShape(const Shape& s, int x, int y) {
    const auto& v = s.vertices;
    if (v.size() < 2) return false;

    switch (s.type) {
    case DrawMode::DrawRectangle: {
        int x1 = min(v[0].x, v[1].x);
        int x2 = max(v[0].x, v[1].x);
        int y1 = min(v[0].y, v[1].y);
        int y2 = max(v[0].y, v[1].y);
        return x >= x1 && x <= x2 && y >= y1 && y <= y2;
    }
    case DrawMode::DrawCircleMidpoint:
    case DrawMode::DrawCircleBresenham: {
        int dx = x - v[0].x;
        int dy = y - v[0].y;
        int r2 = (v[1].x - v[0].x) * (v[1].x - v[0].x) +
            (v[1].y - v[0].y) * (v[1].y - v[0].y);
        return dx * dx + dy * dy <= r2;
    }
    case DrawMode::DrawPolygon: {
        int minX = v[0].x, maxX = v[0].x, minY = v[0].y, maxY = v[0].y;
        for (auto& p : v) {
            minX = min(minX, p.x); maxX = max(maxX, p.x);
            minY = min(minY, p.y); maxY = max(maxY, p.y);
        }
        return x >= minX && x <= maxX && y >= minY && y <= maxY;
    }
    default:
        return false;
    }
}

//---------------------- 普通扫描线填充（原来的实现） ----------------------

void FillPolygonScanline(HDC hdc, const std::vector<Point>& v, COLORREF c, bool innerOnly) {
    if (v.size() < 3) return;
    int ymin = v[0].y, ymax = v[0].y;
    for (auto& p : v) { ymin = min(ymin, p.y); ymax = max(ymax, p.y); }

    for (int y = ymin; y <= ymax; ++y) {
        std::vector<float> xs;
        for (size_t i = 0; i < v.size(); ++i) {
            Point p1 = v[i], p2 = v[(i + 1) % v.size()];
            if (p1.y == p2.y) continue;     // 忽略水平边
            int yMin = min(p1.y, p2.y);
            int yMax = max(p1.y, p2.y);
            if (y < yMin || y >= yMax) continue;
            float x = p1.x + (float)(y - p1.y) * (float)(p2.x - p1.x) / (float)(p2.y - p1.y);
            xs.push_back(x);
        }
        if (xs.size() < 2) continue;
        std::sort(xs.begin(), xs.end());
        for (size_t i = 0; i + 1 < xs.size(); i += 2) {
            int x1 = (int)std::ceil(xs[i]);
            int x2 = (int)std::floor(xs[i + 1]);
            if (innerOnly) { x1++; x2--; }
            if (x1 > x2) continue;
            for (int x = x1; x <= x2; ++x)
                DrawPixel(hdc, x, y, c);
        }
    }
}

// 矩形填充：采用闭区间 [x1,x2] × [y1,y2]
void FillRectScanline(HDC hdc, const Point& p1, const Point& p2, COLORREF c, bool innerOnly) {
    int x1 = min(p1.x, p2.x);
    int x2 = max(p1.x, p2.x);
    int y1 = min(p1.y, p2.y);
    int y2 = max(p1.y, p2.y);

    if (innerOnly) { x1++; x2--; y1++; y2--; }
    if (x1 > x2 || y1 > y2) return;

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            DrawPixel(hdc, x, y, c);
}

void FillCircleScanline(HDC hdc, const Point& center, const Point& onCircle, COLORREF c, bool innerOnly) {
    int xc = center.x, yc = center.y;
    int r = int(std::sqrt(double((onCircle.x - xc) * (onCircle.x - xc) +
        (onCircle.y - yc) * (onCircle.y - yc))));
    if (innerOnly && r > 0) r--;
    if (r <= 0) return;

    for (int y = yc - r; y <= yc + r; ++y) {
        int dy = y - yc;
        int t = r * r - dy * dy;
        if (t < 0) continue;
        int dx = int(std::sqrt(double(t)));
        int x1 = xc - dx, x2 = xc + dx;
        for (int x = x1; x <= x2; ++x)
            DrawPixel(hdc, x, y, c);
    }
}

void FillShapeScanline(HDC hdc, const Shape& s, COLORREF c) {
    const auto& v = s.vertices;
    switch (s.type) {
    case DrawMode::DrawRectangle:
        FillRectScanline(hdc, v[0], v[1], c, false); break;
    case DrawMode::DrawCircleMidpoint:
    case DrawMode::DrawCircleBresenham:
        FillCircleScanline(hdc, v[0], v[1], c, false); break;
    case DrawMode::DrawPolygon:
        FillPolygonScanline(hdc, v, c, false);        break;
    default: break;
    }
}

//---------------------- 栅栏填充真正实现（异或） ----------------------
// “栅栏右边的线向左异或，栅栏左边的线向右异或”
// 这里选取一条竖直栅栏线 fenceX，位于图形最左边界的左侧：
//   fenceX = minX - 1
// 因此所有边都在栅栏右侧，我们就对每个交点做“从交点向左到栅栏”的异或。
// 若你想更一般，也可以根据交点在栅栏左/右两侧决定方向。

// 矩形的栅栏填充
void FenceFillRect(HDC hdc, const Point& p1, const Point& p2, COLORREF xorColor) {
    int x1 = min(p1.x, p2.x);
    int x2 = max(p1.x, p2.x);
    int y1 = min(p1.y, p2.y);
    int y2 = max(p1.y, p2.y);

    int fenceX = x1 - 1; // 栅栏在矩形左侧

    for (int y = y1; y <= y2; ++y) {
        int xs[2] = { x1, x2 }; // 当前扫描线与矩形边界的两个交点
        for (int k = 0; k < 2; ++k) {
            int xEnd = xs[k];
            if (xEnd < fenceX) continue; // 按当前选择不太会发生
            for (int x = fenceX; x <= xEnd; ++x) {
                DrawPixelXor(hdc, x, y, xorColor);
            }
        }
    }
}

// 圆的栅栏填充
void FenceFillCircle(HDC hdc, const Point& center, const Point& onCircle, COLORREF xorColor) {
    int xc = center.x, yc = center.y;
    int r = int(std::sqrt(double((onCircle.x - xc) * (onCircle.x - xc) +
        (onCircle.y - yc) * (onCircle.y - yc))));
    if (r <= 0) return;

    int minX = xc - r;
    int maxX = xc + r;
    int fenceX = minX - 1; // 栅栏在圆左侧

    for (int y = yc - r; y <= yc + r; ++y) {
        int dy = y - yc;
        int t = r * r - dy * dy;
        if (t < 0) continue;
        int dx = int(std::sqrt(double(t)));
        int xs[2] = { xc - dx, xc + dx }; // 与当前扫描线交点
        for (int k = 0; k < 2; ++k) {
            int xEnd = xs[k];
            if (xEnd < fenceX) continue;
            for (int x = fenceX; x <= xEnd; ++x) {
                DrawPixelXor(hdc, x, y, xorColor);
            }
        }
    }
}

// 多边形的栅栏填充
void FenceFillPolygon(HDC hdc, const std::vector<Point>& v, COLORREF xorColor) {
    if (v.size() < 3) return;

    int minX = v[0].x, maxX = v[0].x;
    int ymin = v[0].y, ymax = v[0].y;
    for (auto& p : v) {
        minX = min(minX, p.x); maxX = max(maxX, p.x);
        ymin = min(ymin, p.y); ymax = max(ymax, p.y);
    }

    int fenceX = minX - 1; // 栅栏在多边形左侧

    for (int y = ymin; y <= ymax; ++y) {
        std::vector<float> xs;
        // 和扫描线填充一样，求当前扫描线与各边的交点
        for (size_t i = 0; i < v.size(); ++i) {
            Point p1 = v[i], p2 = v[(i + 1) % v.size()];
            if (p1.y == p2.y) continue;     // 忽略水平边
            int yMin = min(p1.y, p2.y);
            int yMax = max(p1.y, p2.y);
            if (y < yMin || y >= yMax) continue;
            float x = p1.x + (float)(y - p1.y) * (float)(p2.x - p1.x) / (float)(p2.y - p1.y);
            xs.push_back(x);
        }
        if (xs.empty()) continue;

        // 交点顺序其实对“从栅栏到交点异或”来说无所谓，但排个序更清晰
        std::sort(xs.begin(), xs.end());

        // 栅栏右边的线向左异或：从交点向左扫到栅栏
        for (size_t i = 0; i < xs.size(); ++i) {
            int xEnd = (int)std::floor(xs[i]); // 交点所在列
            if (xEnd < fenceX) continue;
            for (int x = fenceX; x <= xEnd; ++x) {
                DrawPixelXor(hdc, x, y, xorColor);
            }
        }
    }
}

// 统一的“栅栏填充”入口：根据图元类型调用不同的 fence 填充函数
void FillShapeFence(HDC hdc, const Shape& s, COLORREF fillColor) {
    const auto& v = s.vertices;
    if (v.size() < 2) return;

    // 设背景是白色 (255,255,255)，希望最终填充颜色为 fillColor，
    // 使用异或时，令“笔颜色” xorColor 满足：
    //   255 XOR xorColor = fillColor  => xorColor = 255 XOR fillColor
    COLORREF xorColor = RGB(
        255 ^ GetRValue(fillColor),
        255 ^ GetGValue(fillColor),
        255 ^ GetBValue(fillColor)
    );

    switch (s.type) {
    case DrawMode::DrawRectangle:
        FenceFillRect(hdc, v[0], v[1], xorColor);
        break;
    case DrawMode::DrawCircleMidpoint:
    case DrawMode::DrawCircleBresenham:
        FenceFillCircle(hdc, v[0], v[1], xorColor);
        break;
    case DrawMode::DrawPolygon:
        FenceFillPolygon(hdc, v, xorColor);
        break;
    default:
        break;
    }
}

//-----------------------------------------------------------------------------
// 9. 菜单 & 交互 & 重绘
//-----------------------------------------------------------------------------
void CreateMenuSystem(HWND hwnd) {
    HMENU hMenu = CreateMenu();
    HMENU hDrawMenu = CreateMenu();
    HMENU hFillMenu = CreateMenu();
    HMENU hEditMenu = CreateMenu();

    AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_LINE_MIDPOINT, L"直线 (中点法)");
    AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_LINE_BRESENHAM, L"直线 (Bresenham)");
    AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_CIRCLE_MIDPOINT, L"圆 (中点法)");
    AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_CIRCLE_BRESENHAM, L"圆 (Bresenham)");
    AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_RECTANGLE, L"矩形");
    AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_POLYGON, L"多边形");
    AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_BSPLINE, L"B样条曲线");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hDrawMenu, L"绘图");

    AppendMenu(hFillMenu, MF_STRING, ID_FILL_SCANLINE, L"扫描线填充");
    AppendMenu(hFillMenu, MF_STRING, ID_FILL_FENCE, L"栅栏填充(无种子)");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFillMenu, L"填充");

    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_FINISH, L"完成当前图形");
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_CLEAR, L"清空画布");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, L"编辑");

    SetMenu(hwnd, hMenu);
}

void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);

    // 每次重绘都先清成白色，然后根据 g_shapes 重绘，
    // 这样栅栏填充中的“背景=白色”的假设是成立的
    FillRect(g_hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    RedrawAllShapes(g_hdcMem);

    // 正在绘制多边形时的折线预览（不过闭）
    if (g_currentMode == DrawMode::DrawPolygon && g_currentPoints.size() >= 2) {
        DrawPolyline(g_hdcMem, g_currentPoints, g_drawColor, false);
    }

    // 正在绘制 B 样条时：画已点击控制点 + 控制折线 + B 样条预览
    if (g_currentMode == DrawMode::DrawBSpline && !g_currentPoints.empty()) {
        for (auto& p : g_currentPoints)
            Ellipse(g_hdcMem, p.x - 3, p.y - 3, p.x + 3, p.y + 3);

        if (g_currentPoints.size() > 1)
            DrawPolyline(g_hdcMem, g_currentPoints, RGB(200, 200, 200), false);

        if (g_currentPoints.size() >= 4)
            DrawBSpline(g_hdcMem, g_currentPoints, g_drawColor);
    }

    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, g_hdcMem, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
}

void OnLButtonDown(int x, int y) {
    switch (g_currentMode) {
    case DrawMode::DrawLineMidpoint:
    case DrawMode::DrawLineBresenham:
    case DrawMode::DrawCircleMidpoint:
    case DrawMode::DrawCircleBresenham:
    case DrawMode::DrawRectangle:
        if (!g_isDrawing) {
            g_currentPoints.clear();
            g_currentPoints.push_back({ x, y });
            g_isDrawing = true;
        }
        else {
            g_currentPoints.push_back({ x, y });
            FinishDrawing();
        }
        break;

    case DrawMode::DrawPolygon:
    case DrawMode::DrawBSpline:
        // 连续点，等点击"完成当前图形"才结束
        g_currentPoints.push_back({ x, y });
        InvalidateRect(g_hwnd, NULL, FALSE);
        break;

    case DrawMode::FillScanline:
    case DrawMode::FillFence: {
        for (auto& s : g_shapes) {
            if (PointInShape(s, x, y)) {
                s.fillColor = g_fillColor;
                if (g_currentMode == DrawMode::FillScanline) {
                    s.fillMode = 1;
                    FillShapeScanline(g_hdcMem, s, s.fillColor);
                }
                else {
                    s.fillMode = 2;
                    FillShapeFence(g_hdcMem, s, s.fillColor);  // 真正的栅栏填充
                }
                InvalidateRect(g_hwnd, NULL, FALSE);
                break;
            }
        }
    } break;

    default: break;
    }
}

void OnMouseMove(int x, int y) {
    if (!g_isDrawing || g_currentPoints.empty()) return;

    HDC hdc = GetDC(g_hwnd);
    RECT rc; GetClientRect(g_hwnd, &rc);

    FillRect(g_hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    RedrawAllShapes(g_hdcMem);

    Point p0 = g_currentPoints[0];

    switch (g_currentMode) {
    case DrawMode::DrawLineMidpoint:
        DrawLineMidpoint(g_hdcMem, p0.x, p0.y, x, y, g_drawColor); break;
    case DrawMode::DrawLineBresenham:
        DrawLineBresenham(g_hdcMem, p0.x, p0.y, x, y, g_drawColor); break;

    case DrawMode::DrawCircleMidpoint:
    case DrawMode::DrawCircleBresenham:
    case DrawMode::DrawRectangle:
        break;

    case DrawMode::DrawPolygon: {
        if (g_currentPoints.size() > 1)
            DrawPolyline(g_hdcMem, g_currentPoints, g_drawColor, false);
        Point last = g_currentPoints.back();
        DrawLineMidpoint(g_hdcMem, last.x, last.y, x, y, g_drawColor);
    } break;

    case DrawMode::DrawBSpline: {
        for (auto& p : g_currentPoints)
            Ellipse(g_hdcMem, p.x - 3, p.y - 3, p.x + 3, p.y + 3);

        std::vector<Point> temp = g_currentPoints;
        temp.push_back({ x, y });

        if (temp.size() > 1)
            DrawPolyline(g_hdcMem, temp, RGB(200, 200, 200), false);

        if (temp.size() >= 4)
            DrawBSpline(g_hdcMem, temp, g_drawColor);
    } break;

    default: break;
    }

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, g_hdcMem, 0, 0, SRCCOPY);
    ReleaseDC(g_hwnd, hdc);
}

void ClearCanvas() {
    g_shapes.clear();
    g_currentPoints.clear();
    g_isDrawing = false;
    g_currentMode = DrawMode::None;
    InvalidateRect(g_hwnd, NULL, TRUE);
}

void FinishDrawing() {
    if (g_currentPoints.empty()) { g_isDrawing = false; return; }

    Shape s;
    s.type = g_currentMode;
    s.vertices = g_currentPoints;
    s.color = g_drawColor;
    s.fillColor = g_fillColor;
    s.fillMode = 0;

    if (s.type == DrawMode::DrawPolygon && s.vertices.size() < 3) {
        // 多边形不合法则丢弃
    }
    else if (s.type == DrawMode::DrawBSpline && s.vertices.size() < 4) {
        // B样条不合法则丢弃
    }
    else {
        g_shapes.push_back(s);
    }

    g_currentPoints.clear();
    if (g_currentMode != DrawMode::DrawPolygon &&
        g_currentMode != DrawMode::DrawBSpline) {
        g_isDrawing = false;
    }
    InvalidateRect(g_hwnd, NULL, FALSE);
}

void RedrawAllShapes(HDC hdc) {
    for (const auto& s : g_shapes) {
        if (s.fillMode == 1)
            FillShapeScanline(hdc, s, s.fillColor);
        else if (s.fillMode == 2)
            FillShapeFence(hdc, s, s.fillColor);  // 重新用栅栏填充绘制
        DrawShapeBorder(hdc, s);
    }
}
