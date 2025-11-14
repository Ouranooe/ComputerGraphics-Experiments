#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d2d1.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stack>
#include <queue>
#include <cstdint>
#include <limits>

#pragma comment(lib, "d2d1.lib")

// ================== 全局常量和枚举 ==================
// 菜单项ID
#define ID_EXP_1 1001
#define ID_EXP_2 1002
#define ID_DRAW_LINE_MIDPOINT 101
#define ID_DRAW_LINE_BRESENHAM 102
#define ID_DRAW_CIRCLE_MIDPOINT 103
#define ID_DRAW_CIRCLE_BRESENHAM 104
#define ID_DRAW_RECTANGLE 105
#define ID_DRAW_POLYGON 106
#define ID_DRAW_BSPLINE 107
#define ID_EDIT_CLEAR 200
#define ID_EDIT_FINISH_POLYGON 201
#define ID_EDIT_FINISH_BSPLINE 202
#define ID_FILL_SCANLINE 300      // 扫描线填充（针对多边形/矩形）
#define ID_FILL_FENCE_MODE 301    // 栅栏填充（自动栅栏线，不用种子点）

// ================== 全局状态 ==================
int currentExperiment = 1; // 1: 实验一, 2: 实验二
HWND g_hwnd = nullptr;

// ================== 实验一==================
ID2D1Factory* pFactory = nullptr;
ID2D1HwndRenderTarget* pRenderTarget = nullptr;
ID2D1SolidColorBrush* pBrush = nullptr;

const float DESIGN_WIDTH = 66.0f;
const float DESIGN_HEIGHT = 46.0f;
const float SCALE = 6.0f;

// ================== 实验二==================
enum DrawMode {
    NONE,
    LINE_MIDPOINT,
    LINE_BRESENHAM,
    CIRCLE_MIDPOINT,
    CIRCLE_BRESENHAM,
    RECTANGLE,
    POLYGON,
    BSPLINE
    // 注意：不再需要 FILL_FENCE 作为“模式”
};

struct Point { int x, y; Point(int x = 0, int y = 0) : x(x), y(y) {} };

struct Shape {
    DrawMode type = NONE;
    std::vector<Point> points;
    COLORREF color = RGB(0, 0, 0);
    bool isFilled = false;
    COLORREF fillColor = RGB(255, 255, 255);
};

std::vector<Shape> shapes;
DrawMode currentMode = NONE;
std::vector<Point> tempPoints;
bool isDrawing = false;

bool g_hasHover = false;
Point g_hoverPoint; // 当前鼠标位置，用作多边形/样条的动态预览

// ====== 栅栏填充结果 ======
struct Span { int y, x0, x1; };
struct FenceFillRegion { std::vector<Span> spans; COLORREF color; };
std::vector<FenceFillRegion> g_fenceFills; // 已完成的栅栏填充区域

// ================== D2D 资源管理  ==================
HRESULT CreateD2DResources(HWND hwnd) {
    if (pRenderTarget) return S_OK;
    RECT rc; GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
    if (SUCCEEDED(hr)) {
        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &pRenderTarget);
        if (SUCCEEDED(hr)) {
            pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBrush);
        }
    }
    return hr;
}

void DiscardD2DResources() {
    if (pBrush) { pBrush->Release(); pBrush = nullptr; }
    if (pRenderTarget) { pRenderTarget->Release(); pRenderTarget = nullptr; }
    if (pFactory) { pFactory->Release(); pFactory = nullptr; }
}

// ================== 实验切换与状态重置 ==================
void SwitchExperiment(int exp) {
    if (currentExperiment == exp) return;
    currentExperiment = exp;
    DiscardD2DResources();
    shapes.clear();
    g_fenceFills.clear();
    currentMode = NONE;
    tempPoints.clear();
    isDrawing = false;
    g_hasHover = false;
    InvalidateRect(g_hwnd, nullptr, TRUE);
}

void ClearCanvas() {
    shapes.clear();
    g_fenceFills.clear();
    tempPoints.clear();
    isDrawing = false;
    g_hasHover = false;
    InvalidateRect(g_hwnd, nullptr, TRUE);
}

// ================== 实验一渲染  ==================
void RenderExperiment1(HWND hwnd) {
    HRESULT hr = CreateD2DResources(hwnd);
    if (FAILED(hr)) return;

    PAINTSTRUCT ps; BeginPaint(hwnd, &ps);
    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    D2D1_SIZE_F rtSize = pRenderTarget->GetSize();
    float scaledWidth = DESIGN_WIDTH * SCALE;
    float scaledHeight = DESIGN_HEIGHT * SCALE;
    float offsetX = (rtSize.width - scaledWidth) / 2.0f;
    float offsetY = (rtSize.height - scaledHeight) / 2.0f;

    auto ToScreenX = [&](float x) { return offsetX + x * SCALE; };
    auto ToScreenY = [&](float y) { return offsetY + y * SCALE; };

    D2D1_ROUNDED_RECT outerRect = D2D1::RoundedRect(
        D2D1::RectF(ToScreenX(0), ToScreenY(0), ToScreenX(66), ToScreenY(46)),
        7.0f * SCALE, 7.0f * SCALE
    );
    pRenderTarget->DrawRoundedRectangle(&outerRect, pBrush, 0.8f * SCALE);

    D2D1_ROUNDED_RECT innerRect = D2D1::RoundedRect(
        D2D1::RectF(ToScreenX(11.5), ToScreenY(8), ToScreenX(54.5), ToScreenY(38)),
        3.0f * SCALE, 3.0f * SCALE
    );
    pRenderTarget->DrawRoundedRectangle(&innerRect, pBrush, 0.8f * SCALE);

    struct Hole { float x, y; };
    Hole holes[] = { {6.5, 7.5}, {59.5, 7.5}, {6.5, 38.5}, {59.5, 38.5} };
    for (const auto& h : holes) {
        D2D1_ELLIPSE hole = D2D1::Ellipse(
            D2D1::Point2F(ToScreenX(h.x), ToScreenY(h.y)),
            3.5f * SCALE, 3.5f * SCALE
        );
        pRenderTarget->DrawEllipse(hole, pBrush, 0.8f * SCALE);
    }

    hr = pRenderTarget->EndDraw();
    if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET) { DiscardD2DResources(); }
    EndPaint(hwnd, &ps);
}

// ================== GDI 基础绘图算法 ==================
void DrawLineMidpoint(HDC hdc, int x0, int y0, int x1, int y1, COLORREF color) {
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }

    int dx = x1 - x0;
    int dy = abs(y1 - y0);
    int d = 2 * dy - dx;
    int dE = 2 * dy;
    int dNE = 2 * (dy - dx);
    int y = y0;
    int ystep = (y0 < y1) ? 1 : -1;

    for (int x = x0; x <= x1; x++) {
        if (steep) SetPixel(hdc, y, x, color);
        else SetPixel(hdc, x, y, color);
        if (d <= 0) d += dE; else { d += dNE; y += ystep; }
    }
}

void DrawLineBresenham(HDC hdc, int x0, int y0, int x1, int y1, COLORREF color) {
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }
    int dx = x1 - x0, dy = abs(y1 - y0), error = dx / 2, y = y0;
    int ystep = (y0 < y1) ? 1 : -1;
    for (int x = x0; x <= x1; x++) {
        SetPixel(hdc, steep ? y : x, steep ? x : y, color);
        error -= dy; if (error < 0) { y += ystep; error += dx; }
    }
}

void DrawCircleMidpoint(HDC hdc, int xc, int yc, int r, COLORREF color) {
    if (r <= 0) return;
    int x = 0, y = r, d = 1 - r;
    auto plot8 = [&](int xo, int yo) {
        SetPixel(hdc, xc + xo, yc + yo, color); SetPixel(hdc, xc - xo, yc + yo, color);
        SetPixel(hdc, xc + xo, yc - yo, color); SetPixel(hdc, xc - xo, yc - yo, color);
        SetPixel(hdc, xc + yo, yc + xo, color); SetPixel(hdc, xc - yo, yc + xo, color);
        SetPixel(hdc, xc + yo, yc - xo, color); SetPixel(hdc, xc - yo, yc - xo, color);
        };
    plot8(x, y);
    while (x < y) {
        if (d < 0) d += 2 * x + 3; else { d += 2 * (x - y) + 5; y--; }
        x++; plot8(x, y);
    }
}

void DrawCircleBresenham(HDC hdc, int xc, int yc, int r, COLORREF color) {
    if (r <= 0) return;
    int x = 0, y = r; int d = 3 - 2 * r;
    auto plot8 = [&](int xo, int yo) {
        SetPixel(hdc, xc + xo, yc + yo, color); SetPixel(hdc, xc - xo, yc + yo, color);
        SetPixel(hdc, xc + xo, yc - yo, color); SetPixel(hdc, xc - xo, yc - yo, color);
        SetPixel(hdc, xc + yo, yc + xo, color); SetPixel(hdc, xc - yo, yc + xo, color);
        SetPixel(hdc, xc + yo, yc - xo, color); SetPixel(hdc, xc - yo, yc - xo, color);
        };
    plot8(x, y);
    while (x < y) {
        if (d < 0) d += 4 * x + 6; else { d += 4 * (x - y) + 10; y--; }
        x++; plot8(x, y);
    }
}

// ================== B样条曲线算法 ==================
double B0(double t) { return (1 - t) * (1 - t) * (1 - t) / 6.0; }
double B1(double t) { return (3 * t * t * t - 6 * t * t + 4) / 6.0; }
double B2(double t) { return (-3 * t * t * t + 3 * t * t + 3 * t + 1) / 6.0; }
double B3(double t) { return t * t * t / 6.0; }

void DrawBSplineSegment(HDC hdc, const Point& p0, const Point& p1,
    const Point& p2, const Point& p3, COLORREF /*color*/) {
    const int steps = 50;

    bool hasPrev = false;

    for (int i = 0; i <= steps; ++i) {
        double t = static_cast<double>(i) / steps;
        double x = B0(t) * p0.x + B1(t) * p1.x + B2(t) * p2.x + B3(t) * p3.x;
        double y = B0(t) * p0.y + B1(t) * p1.y + B2(t) * p2.y + B3(t) * p3.y;

        int ix = static_cast<int>(x + 0.5);
        int iy = static_cast<int>(y + 0.5);

        if (!hasPrev) {
            MoveToEx(hdc, ix, iy, nullptr);
            hasPrev = true;
        }
        else {
            LineTo(hdc, ix, iy);
        }
    }
}

void DrawBSpline(HDC hdc, const std::vector<Point>& ctrlPts, COLORREF color) {
    if (ctrlPts.size() < 4) return;
    for (size_t i = 0; i + 3 < ctrlPts.size(); ++i) {
        DrawBSplineSegment(hdc, ctrlPts[i], ctrlPts[i + 1], ctrlPts[i + 2], ctrlPts[i + 3], color);
    }
}

// ================== 扫描线填充（多边形） ==================
struct Edge { double x, invSlope; int yMax; };

static void FillPolygonScanlineEx(HDC hdc, const std::vector<Point>& pts, COLORREF color) {
    if (pts.size() < 3) return;
    int n = static_cast<int>(pts.size());

    int ymin = INT_MAX, ymax = INT_MIN;
    for (const auto& p : pts) { ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y); }
    if (ymin >= ymax) return;

    std::vector<std::vector<Edge>> ET(ymax - ymin + 1);

    auto addEdge = [&](Point a, Point b) {
        if (a.y == b.y) return; // 跳过水平边
        if (a.y > b.y) std::swap(a, b);
        Edge e; e.x = a.x; e.invSlope = static_cast<double>(b.x - a.x) / static_cast<double>(b.y - a.y);
        e.yMax = b.y; // 上端y不参与扫描线填充
        ET[a.y - ymin].push_back(e);
        };

    for (int i = 0; i < n; ++i) addEdge(pts[i], pts[(i + 1) % n]);

    std::vector<Edge> AET;
    HPEN hPen = CreatePen(PS_SOLID, 1, color); HGDIOBJ oldPen = SelectObject(hdc, hPen);

    for (int y = ymin; y < ymax; ++y) {
        if (!ET[y - ymin].empty()) AET.insert(AET.end(), ET[y - ymin].begin(), ET[y - ymin].end());
        AET.erase(std::remove_if(AET.begin(), AET.end(), [&](const Edge& e) { return e.yMax <= y; }), AET.end());
        std::sort(AET.begin(), AET.end(), [](const Edge& a, const Edge& b) { return a.x < b.x; });

        for (size_t i = 0; i + 1 < AET.size(); i += 2) {
            int x0 = static_cast<int>(std::ceil(AET[i].x));
            int x1 = static_cast<int>(std::floor(AET[i + 1].x));
            if (x0 <= x1) { MoveToEx(hdc, x0, y, nullptr); LineTo(hdc, x1 + 1, y); }
        }
        for (auto& e : AET) e.x += e.invSlope;
    }

    SelectObject(hdc, oldPen); DeleteObject(hPen);
}

// ================== 圆的扫描线填充（用于 isFilled 圆） ==================
static void FillCircleScanline(HDC hdc, const Point& center, int r, COLORREF color) {
    if (r <= 0) return;
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);

    for (int dy = -r; dy <= r; ++dy) {
        int y = center.y + dy;
        int inside = r * r - dy * dy;
        if (inside < 0) continue;
        int dx = static_cast<int>(std::floor(std::sqrt(static_cast<double>(inside))));
        int x0 = center.x - dx;
        int x1 = center.x + dx;
        MoveToEx(hdc, x0, y, nullptr);
        LineTo(hdc, x1 + 1, y);
    }

    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

// ================== 栅栏填充相关工具 ==================
static inline bool ColorEqualRGB(uint32_t pix, COLORREF rgb) {
    BYTE r = GetRValue(rgb), g = GetGValue(rgb), b = GetBValue(rgb);
    BYTE pr = (BYTE)(pix & 0xFF); BYTE pg = (BYTE)((pix >> 8) & 0xFF); BYTE pb = (BYTE)((pix >> 16) & 0xFF);
    return pr == r && pg == g && pb == b;
}

static void DrawFenceSpans(HDC dc, const std::vector<Span>& spans, COLORREF color) {
    if (spans.empty()) return;
    HPEN pen = CreatePen(PS_SOLID, 1, color); HGDIOBJ oldPen = SelectObject(dc, pen);
    for (const auto& s : spans) {
        MoveToEx(dc, s.x0, s.y, nullptr);
        LineTo(dc, s.x1 + 1, s.y);
    }
    SelectObject(dc, oldPen); DeleteObject(pen);
}

// 在内存DC中只画一个图形的黑色边界，白底，用于栅栏填充
static void DrawSingleShapeBoundary(HDC memDC, const RECT& rc, const Shape& shape, COLORREF fenceColor) {
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &rc, hBrush);
    DeleteObject(hBrush);

    HPEN hPen = CreatePen(PS_SOLID, 1, fenceColor);
    HGDIOBJ oldPen = SelectObject(memDC, hPen);
    HGDIOBJ oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));

    switch (shape.type) {
    case RECTANGLE:
        if (shape.points.size() == 2) {
            Rectangle(memDC, shape.points[0].x, shape.points[0].y,
                shape.points[1].x, shape.points[1].y);
        }
        break;
    case POLYGON:
        if (shape.points.size() >= 2) {
            for (size_t i = 0; i < shape.points.size(); ++i) {
                const Point& a = shape.points[i];
                const Point& b = shape.points[(i + 1) % shape.points.size()];
                DrawLineBresenham(memDC, a.x, a.y, b.x, b.y, fenceColor);
            }
        }
        break;
    case CIRCLE_MIDPOINT:
    case CIRCLE_BRESENHAM:
        if (shape.points.size() == 2) {
            int r = static_cast<int>(std::sqrt(
                std::pow(shape.points[1].x - shape.points[0].x, 2.0) +
                std::pow(shape.points[1].y - shape.points[0].y, 2.0)));
            DrawCircleBresenham(memDC, shape.points[0].x,
                shape.points[0].y, r, fenceColor);
        }
        break;
    default:
        break;
    }

    SelectObject(memDC, oldBrush);
    SelectObject(memDC, oldPen);
    DeleteObject(hPen);
}

// 为单个图形生成快照（只画边界）
static void SnapshotShapeScene(const Shape& shape,
    std::vector<uint32_t>& out, int& w, int& h) {
    RECT rc; GetClientRect(g_hwnd, &rc);
    w = rc.right - rc.left; h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr; HDC hdc = GetDC(g_hwnd);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP dib = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(memDC, dib);

    COLORREF fenceColor = RGB(0, 0, 0);
    DrawSingleShapeBoundary(memDC, rc, shape, fenceColor);

    out.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    memcpy(out.data(), bits, out.size() * sizeof(uint32_t));

    SelectObject(memDC, oldBmp);
    DeleteObject(dib); DeleteDC(memDC); ReleaseDC(g_hwnd, hdc);
}

// 计算图形包围盒，用于限制扫描范围
static void GetShapeBoundingBox(const Shape& s, int& minX, int& minY, int& maxX, int& maxY) {
    minX = INT_MAX; minY = INT_MAX;
    maxX = INT_MIN; maxY = INT_MIN;

    if (s.type == RECTANGLE && s.points.size() == 2) {
        int x0 = s.points[0].x, y0 = s.points[0].y;
        int x1 = s.points[1].x, y1 = s.points[1].y;
        minX = std::min(x0, x1); maxX = std::max(x0, x1);
        minY = std::min(y0, y1); maxY = std::max(y0, y1);
    }
    else if (s.type == POLYGON && s.points.size() >= 3) {
        for (const auto& p : s.points) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
    }
    else if ((s.type == CIRCLE_MIDPOINT || s.type == CIRCLE_BRESENHAM) && s.points.size() == 2) {
        int cx = s.points[0].x;
        int cy = s.points[0].y;
        int dx = s.points[1].x - cx;
        int dy = s.points[1].y - cy;
        int r = static_cast<int>(std::sqrt(dx * dx + dy * dy));
        minX = cx - r; maxX = cx + r;
        minY = cy - r; maxY = cy + r;
    }
    else {
        minX = minY = 0; maxX = maxY = -1; // 无效
    }
}

// 栅栏填充核心：不用种子点，用“栅栏线”（扫描线+边界）填满一个图形
static void FenceFillShape(const Shape& shape) {
    std::vector<uint32_t> snapshot;
    int W = 0, H = 0;
    SnapshotShapeScene(shape, snapshot, W, H);
    if (W <= 0 || H <= 0 || snapshot.empty()) return;

    int minX, minY, maxX, maxY;
    GetShapeBoundingBox(shape, minX, minY, maxX, maxY);
    if (minX > maxX || minY > maxY) return;

    // 限制在窗口范围内
    minX = std::max(minX, 0); maxX = std::min(maxX, W - 1);
    minY = std::max(minY, 0); maxY = std::min(maxY, H - 1);
    if (minX > maxX || minY > maxY) return;

    COLORREF fenceColor = RGB(0, 0, 0);
    COLORREF fillColor = RGB(255, 200, 200);

    std::vector<Span> spans;

    for (int y = minY; y <= maxY; ++y) {
        bool inside = false;
        int fillStart = -1;

        int x = minX;
        while (x <= maxX) {
            uint32_t pix = snapshot[static_cast<size_t>(y) * W + x];
            bool isFence = ColorEqualRGB(pix, fenceColor);

            if (isFence) {
                // 跳过连续的栅栏像素，避免多次翻转
                int runStart = x;
                while (x <= maxX) {
                    uint32_t pix2 = snapshot[static_cast<size_t>(y) * W + x];
                    if (!ColorEqualRGB(pix2, fenceColor)) break;
                    x++;
                }
                int runEnd = x - 1;

                if (!inside) {
                    // 外 -> 内 ：填充区从栅栏之后开始
                    inside = true;
                    fillStart = runEnd + 1;
                }
                else {
                    // 内 -> 外 ：一个填充区结束
                    int fillEnd = runStart - 1;
                    if (fillStart <= fillEnd) {
                        spans.push_back({ y, fillStart, fillEnd });
                    }
                    inside = false;
                    fillStart = -1;
                }
            }
            else {
                x++;
            }
        }
        // 如果这一行以“内”状态结束，可以选择丢弃（说明边界不闭合）或忽略
    }

    if (!spans.empty()) {
        g_fenceFills.push_back({ spans, fillColor });
        InvalidateRect(g_hwnd, nullptr, FALSE);
    }
}

// 对最后一个可栅栏填充的图形执行栅栏填充
static void FenceFillLastShape() {
    if (shapes.empty()) return;
    for (int i = static_cast<int>(shapes.size()) - 1; i >= 0; --i) {
        if (shapes[i].type == RECTANGLE ||
            shapes[i].type == POLYGON ||
            shapes[i].type == CIRCLE_MIDPOINT ||
            shapes[i].type == CIRCLE_BRESENHAM) {
            FenceFillShape(shapes[i]);
            return;
        }
    }
    MessageBox(g_hwnd, TEXT("没有可以进行栅栏填充的图形（支持：矩形、多边形、圆）。"),
        TEXT("栅栏填充"), MB_OK | MB_ICONINFORMATION);
}

// ================== 绘制所有图形 ==================
static void DrawAllShapesToDC(HDC memDC, const RECT& rc, bool includeFenceFills, bool includeTempPreview) {
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &rc, hBrush); DeleteObject(hBrush);

    // 先填充图形，后画边界
    for (const auto& shape : shapes) {
        if (shape.isFilled) {
            if (shape.type == POLYGON && shape.points.size() >= 3) {
                FillPolygonScanlineEx(memDC, shape.points, shape.fillColor);
            }
            else if (shape.type == RECTANGLE && shape.points.size() == 2) {
                std::vector<Point> rect_pts = {
                    shape.points[0], Point(shape.points[1].x, shape.points[0].y),
                    shape.points[1], Point(shape.points[0].x, shape.points[1].y)
                };
                FillPolygonScanlineEx(memDC, rect_pts, shape.fillColor);
            }
            else if ((shape.type == CIRCLE_MIDPOINT || shape.type == CIRCLE_BRESENHAM) &&
                shape.points.size() == 2) {
                int r = static_cast<int>(std::sqrt(
                    std::pow(shape.points[1].x - shape.points[0].x, 2.0) +
                    std::pow(shape.points[1].y - shape.points[0].y, 2.0)));
                FillCircleScanline(memDC, shape.points[0], r, shape.fillColor);
            }
        }
    }

    // 画边界
    for (const auto& shape : shapes) {
        switch (shape.type) {
        case LINE_MIDPOINT:
            if (shape.points.size() == 2)
                DrawLineMidpoint(memDC, shape.points[0].x, shape.points[0].y,
                    shape.points[1].x, shape.points[1].y, shape.color);
            break;
        case LINE_BRESENHAM:
            if (shape.points.size() == 2)
                DrawLineBresenham(memDC, shape.points[0].x, shape.points[0].y,
                    shape.points[1].x, shape.points[1].y, shape.color);
            break;
        case CIRCLE_MIDPOINT:
            if (shape.points.size() == 2) {
                int r = static_cast<int>(std::sqrt(
                    std::pow(shape.points[1].x - shape.points[0].x, 2.0) +
                    std::pow(shape.points[1].y - shape.points[0].y, 2.0)));
                DrawCircleMidpoint(memDC, shape.points[0].x,
                    shape.points[0].y, r, shape.color);
            }
            break;
        case CIRCLE_BRESENHAM:
            if (shape.points.size() == 2) {
                int r = static_cast<int>(std::sqrt(
                    std::pow(shape.points[1].x - shape.points[0].x, 2.0) +
                    std::pow(shape.points[1].y - shape.points[0].y, 2.0)));
                DrawCircleBresenham(memDC, shape.points[0].x,
                    shape.points[0].y, r, shape.color);
            }
            break;
        case RECTANGLE:
            if (shape.points.size() == 2) {
                HPEN hPen = CreatePen(PS_SOLID, 1, shape.color);
                HGDIOBJ oldPen = SelectObject(memDC, hPen);
                HGDIOBJ oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));
                Rectangle(memDC, shape.points[0].x, shape.points[0].y,
                    shape.points[1].x, shape.points[1].y);
                SelectObject(memDC, oldBrush); SelectObject(memDC, oldPen); DeleteObject(hPen);
            }
            break;
        case POLYGON:
            if (shape.points.size() >= 2) {
                for (size_t i = 0; i < shape.points.size(); ++i) {
                    const Point& a = shape.points[i];
                    const Point& b = shape.points[(i + 1) % shape.points.size()];
                    DrawLineBresenham(memDC, a.x, a.y, b.x, b.y, shape.color);
                }
            }
            break;
        case BSPLINE:
        {
            HPEN hPen = CreatePen(PS_SOLID, 1, shape.color);
            HGDIOBJ oldPen = SelectObject(memDC, hPen);
            DrawBSpline(memDC, shape.points, shape.color);
            SelectObject(memDC, oldPen);
            DeleteObject(hPen);
        }
        break;
        default: break;
        }
    }

    // 栅栏填充结果
    if (includeFenceFills) {
        for (const auto& reg : g_fenceFills)
            DrawFenceSpans(memDC, reg.spans, reg.color);
    }

    // 临时预览
    if (includeTempPreview && isDrawing && !tempPoints.empty()) {
        COLORREF tempColor = RGB(255, 0, 0);
        HPEN hTempPen = CreatePen(PS_DOT, 1, tempColor);
        HGDIOBJ oldPen = SelectObject(memDC, hTempPen);
        HGDIOBJ oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));

        if ((currentMode == LINE_MIDPOINT || currentMode == LINE_BRESENHAM) && tempPoints.size() == 2) {
            if (currentMode == LINE_MIDPOINT)
                DrawLineMidpoint(memDC, tempPoints[0].x, tempPoints[0].y,
                    tempPoints[1].x, tempPoints[1].y, tempColor);
            else
                DrawLineBresenham(memDC, tempPoints[0].x, tempPoints[0].y,
                    tempPoints[1].x, tempPoints[1].y, tempColor);
        }
        else if ((currentMode == CIRCLE_MIDPOINT || currentMode == CIRCLE_BRESENHAM) && tempPoints.size() == 2) {
            int r = static_cast<int>(std::sqrt(
                std::pow(tempPoints[1].x - tempPoints[0].x, 2.0) +
                std::pow(tempPoints[1].y - tempPoints[0].y, 2.0)));
            if (currentMode == CIRCLE_MIDPOINT)
                DrawCircleMidpoint(memDC, tempPoints[0].x, tempPoints[0].y, r, tempColor);
            else
                DrawCircleBresenham(memDC, tempPoints[0].x, tempPoints[0].y, r, tempColor);
        }
        else if (currentMode == RECTANGLE && tempPoints.size() == 2) {
            Rectangle(memDC, tempPoints[0].x, tempPoints[0].y,
                tempPoints[1].x, tempPoints[1].y);
        }
        else if (currentMode == POLYGON) {
            for (size_t i = 0; i + 1 < tempPoints.size(); ++i)
                DrawLineBresenham(memDC, tempPoints[i].x, tempPoints[i].y,
                    tempPoints[i + 1].x, tempPoints[i + 1].y, RGB(128, 128, 128));
            if (g_hasHover && !tempPoints.empty()) {
                DrawLineBresenham(memDC, tempPoints.back().x, tempPoints.back().y,
                    g_hoverPoint.x, g_hoverPoint.y, tempColor);
            }
        }
        else if (currentMode == BSPLINE) {
            if (tempPoints.size() >= 1) {
                std::vector<Point> preview = tempPoints;
                if (g_hasHover) preview.push_back(g_hoverPoint);
                for (size_t i = 0; i + 1 < preview.size(); ++i)
                    DrawLineBresenham(memDC, preview[i].x, preview[i].y,
                        preview[i + 1].x, preview[i + 1].y, RGB(160, 160, 160));
                DrawBSpline(memDC, preview, tempColor);
            }
        }

        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(hTempPen);
    }
}

// ================== 实验二渲染 ==================
void RenderExperiment2(HWND hwnd) {
    PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc,
        rc.right - rc.left,
        rc.bottom - rc.top);
    HGDIOBJ oldBmp = SelectObject(memDC, memBitmap);

    DrawAllShapesToDC(memDC, rc, /*includeFenceFills=*/true, /*includeTempPreview=*/true);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp); DeleteObject(memBitmap); DeleteDC(memDC);
    EndPaint(hwnd, &ps);
}

// ================== 主窗口过程 ==================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
    {
        g_hwnd = hwnd;
        HMENU hMenuBar = CreateMenu();
        HMENU hExpMenu = CreatePopupMenu();
        AppendMenu(hExpMenu, MF_STRING, ID_EXP_1, TEXT("实验一"));
        AppendMenu(hExpMenu, MF_STRING, ID_EXP_2, TEXT("实验二"));
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hExpMenu, TEXT("实验切换"));

        HMENU hDrawMenu = CreatePopupMenu();
        AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_LINE_MIDPOINT, TEXT("直线 (中点法)"));
        AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_LINE_BRESENHAM, TEXT("直线 (Bresenham)"));
        AppendMenu(hDrawMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_CIRCLE_MIDPOINT, TEXT("圆 (中点法)"));
        AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_CIRCLE_BRESENHAM, TEXT("圆 (Bresenham)"));
        AppendMenu(hDrawMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_RECTANGLE, TEXT("矩形"));
        AppendMenu(hDrawMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_POLYGON, TEXT("多边形 (左键添加点)"));
        AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_BSPLINE, TEXT("B样条曲线 (左键添加点)"));
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hDrawMenu, TEXT("绘图"));

        HMENU hFillMenu = CreatePopupMenu();
        AppendMenu(hFillMenu, MF_STRING, ID_FILL_SCANLINE, TEXT("扫描线填充 (最后一个多边形/矩形)"));
        AppendMenu(hFillMenu, MF_STRING, ID_FILL_FENCE_MODE, TEXT("栅栏填充 (最后一个图形)"));
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFillMenu, TEXT("填充"));

        HMENU hEditMenu = CreatePopupMenu();
        AppendMenu(hEditMenu, MF_STRING, ID_EDIT_FINISH_POLYGON, TEXT("完成多边形"));
        AppendMenu(hEditMenu, MF_STRING, ID_EDIT_FINISH_BSPLINE, TEXT("完成B样条曲线"));
        AppendMenu(hEditMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hEditMenu, MF_STRING, ID_EDIT_CLEAR, TEXT("清空画布"));
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hEditMenu, TEXT("编辑"));

        SetMenu(hwnd, hMenuBar);
        SwitchExperiment(1);
        return 0;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id >= ID_DRAW_LINE_MIDPOINT && id <= ID_DRAW_BSPLINE) {
            isDrawing = false; tempPoints.clear(); g_hasHover = false;
        }

        switch (id) {
        case ID_EXP_1: SwitchExperiment(1); break;
        case ID_EXP_2: SwitchExperiment(2); break;

        case ID_DRAW_LINE_MIDPOINT:  currentMode = LINE_MIDPOINT;  break;
        case ID_DRAW_LINE_BRESENHAM: currentMode = LINE_BRESENHAM; break;
        case ID_DRAW_CIRCLE_MIDPOINT: currentMode = CIRCLE_MIDPOINT; break;
        case ID_DRAW_CIRCLE_BRESENHAM: currentMode = CIRCLE_BRESENHAM; break;
        case ID_DRAW_RECTANGLE: currentMode = RECTANGLE; break;
        case ID_DRAW_POLYGON:  currentMode = POLYGON;  break;
        case ID_DRAW_BSPLINE:  currentMode = BSPLINE;  break;

        case ID_FILL_FENCE_MODE:
            if (currentExperiment == 2) {
                FenceFillLastShape();
            }
            break;

        case ID_FILL_SCANLINE:
            if (currentExperiment == 2) {
                for (int i = static_cast<int>(shapes.size()) - 1; i >= 0; --i) {
                    if (shapes[i].type == POLYGON || shapes[i].type == RECTANGLE) {
                        shapes[i].isFilled = true; shapes[i].fillColor = RGB(200, 200, 255);
                        InvalidateRect(hwnd, nullptr, FALSE);
                        break;
                    }
                }
            }
            break;

        case ID_EDIT_CLEAR:
            if (currentExperiment == 2) ClearCanvas();
            break;

        case ID_EDIT_FINISH_POLYGON:
            if (currentExperiment == 2 && currentMode == POLYGON && tempPoints.size() >= 3) {
                shapes.push_back({ POLYGON, tempPoints, RGB(0, 0, 0) });
                tempPoints.clear(); isDrawing = false; g_hasHover = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            break;
        case ID_EDIT_FINISH_BSPLINE:
            if (currentExperiment == 2 && currentMode == BSPLINE && tempPoints.size() >= 4) {
                shapes.push_back({ BSPLINE, tempPoints, RGB(0, 0, 200) });
                tempPoints.clear(); isDrawing = false; g_hasHover = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            break;
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
        if (currentExperiment == 2) {
            Point p(LOWORD(lParam), HIWORD(lParam));
            if (currentMode == POLYGON || currentMode == BSPLINE) {
                tempPoints.push_back(p); isDrawing = true;
                g_hasHover = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            else if (currentMode != NONE) {
                isDrawing = true; tempPoints = { p, p }; SetCapture(hwnd);
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (currentExperiment == 2) {
            int mx = LOWORD(lParam), my = HIWORD(lParam);
            if (isDrawing && (currentMode == LINE_MIDPOINT || currentMode == LINE_BRESENHAM
                || currentMode == CIRCLE_MIDPOINT || currentMode == CIRCLE_BRESENHAM || currentMode == RECTANGLE)) {
                if (!tempPoints.empty()) { tempPoints.back() = Point(mx, my); InvalidateRect(hwnd, nullptr, FALSE); }
            }
            else if ((currentMode == POLYGON || currentMode == BSPLINE) && !tempPoints.empty()) {
                g_hasHover = true;
                g_hoverPoint = Point(mx, my);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_LBUTTONUP:
        if (currentExperiment == 2 && isDrawing) {
            ReleaseCapture();
            if (currentMode == LINE_MIDPOINT || currentMode == LINE_BRESENHAM
                || currentMode == CIRCLE_MIDPOINT || currentMode == CIRCLE_BRESENHAM || currentMode == RECTANGLE) {
                if (tempPoints.size() == 2) shapes.push_back({ currentMode, tempPoints, RGB(0, 0, 0) });
                isDrawing = false; tempPoints.clear(); g_hasHover = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_PAINT:
        if (currentExperiment == 1) RenderExperiment1(hwnd); else RenderExperiment2(hwnd);
        return 0;

    case WM_SIZE:
        if (currentExperiment == 1 && pRenderTarget)
            pRenderTarget->Resize(D2D1::SizeU(LOWORD(lParam), HIWORD(lParam)));
        else InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_DESTROY:
        DiscardD2DResources();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ================== WinMain ==================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const TCHAR CLASS_NAME[] = TEXT("AdvancedGraphicsApp");
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInstance; wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, TEXT("2023112569-高年平"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700, nullptr, nullptr, hInstance, nullptr);

    if (hwnd == nullptr) return 0;
    ShowWindow(hwnd, nCmdShow); UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
