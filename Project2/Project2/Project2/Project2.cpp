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
#define ID_FILL_SCANLINE 300      // 扫描线填充（点击封闭图形）
#define ID_FILL_FENCE_MODE 301    // 栅栏填充（点击种子点）
#define IDT_FENCE_FILL 5001       // 栅栏填充异步处理的计时器ID

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
    BSPLINE,
    FILL_FENCE,          // 栅栏填充模式
    FILL_SCANLINE_MODE   // 扫描线填充模式（点击封闭图形）
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

// ====== 栅栏填充结果与作业 ======
struct Span { int y, x0, x1; };
struct FenceFillRegion { std::vector<Span> spans; COLORREF color; };
std::vector<FenceFillRegion> g_fenceFills; // 已完成的栅栏填充区域

struct FenceFillJob {
    bool active = false;
    int width = 0, height = 0;
    int seedX = 0, seedY = 0;
    COLORREF fenceColor = RGB(0, 0, 0); // 边界颜色
    COLORREF fillColor = RGB(255, 200, 200); // 填充颜色
    std::vector<uint32_t> snapshot;   // 边界快照
    std::vector<unsigned char> vis;   // 访问标记
    std::vector<Span> partialSpans;   // 逻辑填充结果（所有span）
    std::vector<Point> stack;         // 扫描线种子栈
    bool touchedEdge = false;         // 是否触达窗口边界（判定非封闭）

    // 动画阶段控制
    bool regionReady = false;         // true 表示逻辑填充已完成，只做动画
    int  sweepX = 0;                  // 当前“从右往左”扫描的可见左边界
} g_fillJob;

// 栅栏填充动画速度：每个计时器 tick 向左移动多少像素
int g_fenceFillSweepPixelsPerTick = 3;

// 控制栅栏填充动画速度的函数（不在界面显示）
void SetFenceFillSpeed(int pixelsPerTick) {
    if (pixelsPerTick <= 0) pixelsPerTick = 1;
    g_fenceFillSweepPixelsPerTick = pixelsPerTick;
}

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
void CancelFenceFillJob() {
    if (g_fillJob.active) {
        KillTimer(g_hwnd, IDT_FENCE_FILL);
        g_fillJob = FenceFillJob();
    }
}

void SwitchExperiment(int exp) {
    if (currentExperiment == exp) return;
    currentExperiment = exp;
    DiscardD2DResources();
    shapes.clear();
    g_fenceFills.clear();
    CancelFenceFillJob();
    currentMode = NONE;
    tempPoints.clear();
    isDrawing = false;
    g_hasHover = false;
    InvalidateRect(g_hwnd, nullptr, TRUE);
}

void ClearCanvas() {
    shapes.clear();
    g_fenceFills.clear();
    CancelFenceFillJob();
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

// 改造后的：用 LineTo 连接，画笔样式(实线/虚线)由外部控制
void DrawBSplineSegment(HDC hdc, const Point& p0, const Point& p1,
    const Point& p2, const Point& p3, COLORREF /*color*/) {
    const int steps = 50;

    bool hasPrev = false;
    int prevX = 0, prevY = 0;

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

// ================== 扫描线填充 ==================
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
        // 插入新边
        if (!ET[y - ymin].empty()) AET.insert(AET.end(), ET[y - ymin].begin(), ET[y - ymin].end());
        // 删除到达yMax的边
        AET.erase(std::remove_if(AET.begin(), AET.end(), [&](const Edge& e) { return e.yMax <= y; }), AET.end());
        // 按交点x排序
        std::sort(AET.begin(), AET.end(), [](const Edge& a, const Edge& b) { return a.x < b.x; });
        // 成对填充
        for (size_t i = 0; i + 1 < AET.size(); i += 2) {
            int x0 = static_cast<int>(std::ceil(AET[i].x));
            int x1 = static_cast<int>(std::floor(AET[i + 1].x));
            if (x0 <= x1) { MoveToEx(hdc, x0, y, nullptr); LineTo(hdc, x1 + 1, y); }
        }
        // 更新交点
        for (auto& e : AET) e.x += e.invSlope;
    }

    SelectObject(hdc, oldPen); DeleteObject(hPen);
}

// ======== 点在图形内判定，用于“扫描线填充模式”点击 ========
static bool PointInPolygon(const std::vector<Point>& poly, int x, int y) {
    if (poly.size() < 3) return false;
    bool inside = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Point& pi = poly[i];
        const Point& pj = poly[j];
        bool intersect = ((pi.y > y) != (pj.y > y)) &&
            (x < (double)(pj.x - pi.x) * (y - pi.y) / (double)(pj.y - pi.y) + pi.x);
        if (intersect) inside = !inside;
    }
    return inside;
}

static bool PointInRectShape(const Shape& s, int x, int y) {
    if (s.points.size() < 2) return false;
    int x0 = s.points[0].x, y0 = s.points[0].y;
    int x1 = s.points[1].x, y1 = s.points[1].y;
    int left = std::min(x0, x1);
    int right = std::max(x0, x1);
    int top = std::min(y0, y1);
    int bottom = std::max(y0, y1);
    return x >= left && x <= right && y >= top && y <= bottom;
}

static bool PointInCircleShape(const Shape& s, int x, int y) {
    if (s.points.size() < 2) return false;
    int cx = s.points[0].x;
    int cy = s.points[0].y;
    int dx = s.points[1].x - cx;
    int dy = s.points[1].y - cy;
    int r2 = dx * dx + dy * dy;

    int px = x - cx;
    int py = y - cy;
    int d2 = px * px + py * py;
    return d2 <= r2;
}

static bool ShapeContainsPoint(const Shape& s, int x, int y) {
    switch (s.type) {
    case RECTANGLE:
        return PointInRectShape(s, x, y);
    case POLYGON:
        return PointInPolygon(s.points, x, y);
    case CIRCLE_MIDPOINT:
    case CIRCLE_BRESENHAM:
        return PointInCircleShape(s, x, y);
    default:
        return false;
    }
}

// ======== 圆的扫描线填充，用于 isFilled 的圆 ========
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

// ================== 绘制工具 ==================
static void DrawFenceSpans(HDC dc, const std::vector<Span>& spans, COLORREF color) {
    if (spans.empty()) return;
    HPEN pen = CreatePen(PS_SOLID, 1, color); HGDIOBJ oldPen = SelectObject(dc, pen);
    for (const auto& s : spans) {
        MoveToEx(dc, s.x0, s.y, nullptr);
        LineTo(dc, s.x1 + 1, s.y);
    }
    SelectObject(dc, oldPen); DeleteObject(pen);
}

// “从右往左”扫的绘制：只画 x >= sweepX 的部分
static void DrawFenceSpansSweepRightToLeft(HDC dc, const std::vector<Span>& spans,
    COLORREF color, int sweepX) {
    if (spans.empty()) return;
    HPEN pen = CreatePen(PS_SOLID, 1, color); HGDIOBJ oldPen = SelectObject(dc, pen);

    for (const auto& s : spans) {
        int x0 = std::max(s.x0, sweepX);
        int x1 = s.x1;
        if (x0 <= x1) {
            // 从右往左画一条线
            MoveToEx(dc, x1, s.y, nullptr);
            LineTo(dc, x0 - 1, s.y);
        }
    }

    SelectObject(dc, oldPen); DeleteObject(pen);
}

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

    // 再画边界
    for (const auto& shape : shapes) {
        switch (shape.type) {
        case LINE_MIDPOINT:
            if (shape.points.size() == 2)
                DrawLineMidpoint(memDC, shape.points[0].x, shape.points[0].y, shape.points[1].x, shape.points[1].y, shape.color);
            break;
        case LINE_BRESENHAM:
            if (shape.points.size() == 2)
                DrawLineBresenham(memDC, shape.points[0].x, shape.points[0].y, shape.points[1].x, shape.points[1].y, shape.color);
            break;
        case CIRCLE_MIDPOINT:
            if (shape.points.size() == 2) {
                {
                    int r = static_cast<int>(std::sqrt(std::pow(shape.points[1].x - shape.points[0].x, 2.0) + std::pow(shape.points[1].y - shape.points[0].y, 2.0)));
                    DrawCircleMidpoint(memDC, shape.points[0].x, shape.points[0].y, r, shape.color);
                }
            }
            break;
        case CIRCLE_BRESENHAM:
            if (shape.points.size() == 2) {
                int r = static_cast<int>(std::sqrt(std::pow(shape.points[1].x - shape.points[0].x, 2.0) + std::pow(shape.points[1].y - shape.points[0].y, 2.0)));
                DrawCircleBresenham(memDC, shape.points[0].x, shape.points[0].y, r, shape.color);
            }
            break;
        case RECTANGLE:
            if (shape.points.size() == 2) {
                HPEN hPen = CreatePen(PS_SOLID, 1, shape.color);
                HGDIOBJ oldPen = SelectObject(memDC, hPen);
                HGDIOBJ oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));
                Rectangle(memDC, shape.points[0].x, shape.points[0].y, shape.points[1].x, shape.points[1].y);
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

    if (includeFenceFills) {
        // 已完成的栅栏填充区域：直接画满
        for (const auto& reg : g_fenceFills)
            DrawFenceSpans(memDC, reg.spans, reg.color);

        // 正在进行的栅栏填充：若逻辑填充已完成，则用右->左扫的动画
        if (g_fillJob.active && g_fillJob.regionReady && !g_fillJob.partialSpans.empty()) {
            DrawFenceSpansSweepRightToLeft(memDC, g_fillJob.partialSpans,
                g_fillJob.fillColor, g_fillJob.sweepX);
        }
    }

    // 临时预览
    if (includeTempPreview && isDrawing && !tempPoints.empty()) {
        COLORREF tempColor = RGB(255, 0, 0);
        HPEN hTempPen = CreatePen(PS_DOT, 1, tempColor);
        HGDIOBJ oldPen = SelectObject(memDC, hTempPen);
        HGDIOBJ oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));

        if ((currentMode == LINE_MIDPOINT || currentMode == LINE_BRESENHAM) && tempPoints.size() == 2) {
            if (currentMode == LINE_MIDPOINT)
                DrawLineMidpoint(memDC, tempPoints[0].x, tempPoints[0].y, tempPoints[1].x, tempPoints[1].y, tempColor);
            else
                DrawLineBresenham(memDC, tempPoints[0].x, tempPoints[0].y, tempPoints[1].x, tempPoints[1].y, tempColor);
        }
        else if ((currentMode == CIRCLE_MIDPOINT || currentMode == CIRCLE_BRESENHAM) && tempPoints.size() == 2) {
            int r = static_cast<int>(std::sqrt(std::pow(tempPoints[1].x - tempPoints[0].x, 2.0) + std::pow(tempPoints[1].y - tempPoints[0].y, 2.0)));
            if (currentMode == CIRCLE_MIDPOINT)
                DrawCircleMidpoint(memDC, tempPoints[0].x, tempPoints[0].y, r, tempColor);
            else
                DrawCircleBresenham(memDC, tempPoints[0].x, tempPoints[0].y, r, tempColor);
        }
        else if (currentMode == RECTANGLE && tempPoints.size() == 2) {
            Rectangle(memDC, tempPoints[0].x, tempPoints[0].y, tempPoints[1].x, tempPoints[1].y);
        }
        else if (currentMode == POLYGON) {
            for (size_t i = 0; i + 1 < tempPoints.size(); ++i)
                DrawLineBresenham(memDC, tempPoints[i].x, tempPoints[i].y, tempPoints[i + 1].x, tempPoints[i + 1].y, RGB(128, 128, 128));
            if (g_hasHover && !tempPoints.empty()) {
                DrawLineBresenham(memDC, tempPoints.back().x, tempPoints.back().y, g_hoverPoint.x, g_hoverPoint.y, tempColor);
            }
        }
        else if (currentMode == BSPLINE) {
            if (tempPoints.size() >= 1) {
                std::vector<Point> preview = tempPoints;
                if (g_hasHover) preview.push_back(g_hoverPoint);
                for (size_t i = 0; i + 1 < preview.size(); ++i)
                    DrawLineBresenham(memDC, preview[i].x, preview[i].y, preview[i + 1].x, preview[i + 1].y, RGB(160, 160, 160));
                DrawBSpline(memDC, preview, tempColor);
            }
        }

        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(hTempPen);
    }
}

// ================== 栅栏填充 ==================
static inline bool ColorEqualRGB(uint32_t pix, COLORREF rgb) {
    BYTE r = GetRValue(rgb), g = GetGValue(rgb), b = GetBValue(rgb);
    BYTE pr = (BYTE)(pix & 0xFF); BYTE pg = (BYTE)((pix >> 8) & 0xFF); BYTE pb = (BYTE)((pix >> 16) & 0xFF);
    return pr == r && pg == g && pb == b;
}

static void SnapshotScene(std::vector<uint32_t>& out, int& w, int& h) {
    RECT rc; GetClientRect(g_hwnd, &rc);
    w = rc.right - rc.left; h = rc.bottom - rc.top; if (w <= 0 || h <= 0) return;

    BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr; HDC hdc = GetDC(g_hwnd);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP dib = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(memDC, dib);

    DrawAllShapesToDC(memDC, rc, /*includeFenceFills=*/false, /*includeTempPreview=*/false);

    out.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    memcpy(out.data(), bits, out.size() * sizeof(uint32_t));

    SelectObject(memDC, oldBmp);
    DeleteObject(dib); DeleteDC(memDC); ReleaseDC(g_hwnd, hdc);
}

static inline bool InBounds(int x, int y, int w, int h) { return (x >= 0 && x < w && y >= 0 && y < h); }

static bool IsFence(const FenceFillJob& job, int x, int y) {
    uint32_t pix = job.snapshot[static_cast<size_t>(y) * job.width + x];
    return ColorEqualRGB(pix, job.fenceColor);
}

static void StartFenceFillJob(int sx, int sy) {
    CancelFenceFillJob();

    RECT rc; GetClientRect(g_hwnd, &rc);
    int width = rc.right - rc.left, height = rc.bottom - rc.top;
    if (!InBounds(sx, sy, width, height)) return;

    FenceFillJob job; job.fillColor = RGB(255, 200, 200); job.fenceColor = RGB(0, 0, 0);
    SnapshotScene(job.snapshot, job.width, job.height);
    if (job.width == 0 || job.height == 0) return;

    if (!InBounds(sx, sy, job.width, job.height)) return;
    if (IsFence(job, sx, sy)) {
        MessageBox(g_hwnd, TEXT("请点击封闭区域内部，不要点在边界上。"), TEXT("栅栏填充"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    job.seedX = sx; job.seedY = sy; job.active = true; job.touchedEdge = false;
    job.vis.assign(static_cast<size_t>(job.width) * job.height, 0);
    job.stack.clear(); job.partialSpans.clear(); job.stack.emplace_back(sx, sy);
    job.regionReady = false;
    job.sweepX = job.width;  // 从最右边开始往左扫

    g_fillJob = std::move(job);

    // 设置一个合理的动画速度（可在代码里调整）
    SetFenceFillSpeed(9);

    // 定时器周期稍微大一点，看得清楚动画（例如 20ms）
    SetTimer(g_hwnd, IDT_FENCE_FILL, 10, nullptr);
}

// 单步处理：扫描线式的区域生长，返回是否“已完成”
static bool FenceFillStep(int budget) {
    if (!g_fillJob.active) return true;
    auto& J = g_fillJob;
    int W = J.width, H = J.height;

    int steps = 0;
    while (!J.stack.empty() && steps < budget) {
        Point seed = J.stack.back(); J.stack.pop_back();
        int x = seed.x, y = seed.y;
        if (!InBounds(x, y, W, H)) { J.touchedEdge = true; continue; }
        size_t idx = static_cast<size_t>(y) * W + x;
        if (J.vis[idx]) continue;
        if (IsFence(J, x, y)) continue;

        // 向左右扩展
        int xl = x, xr = x;
        while (xl - 1 >= 0 && !IsFence(J, xl - 1, y) && !J.vis[static_cast<size_t>(y) * W + (xl - 1)]) xl--;
        while (xr + 1 < W && !IsFence(J, xr + 1, y) && !J.vis[static_cast<size_t>(y) * W + (xr + 1)]) xr++;

        // 边界检测（泄露到窗口边缘视为未封闭）
        if (y == 0 || y == H - 1 || xl == 0 || xr == W - 1) J.touchedEdge = true;

        // 记录span并标记访问
        for (int xx = xl; xx <= xr; ++xx) {
            J.vis[static_cast<size_t>(y) * W + xx] = 1;
        }
        J.partialSpans.push_back({ y, xl, xr });

        auto enqueueRow = [&](int ny) {
            if (ny < 0 || ny >= H) { J.touchedEdge = true; return; }
            int cx = xl;
            while (cx <= xr) {
                while (cx <= xr) {
                    size_t id2 = static_cast<size_t>(ny) * W + cx;
                    if (!J.vis[id2] && !IsFence(J, cx, ny)) break; else cx++;
                }
                if (cx > xr) break;
                int start = cx;
                while (cx <= xr) {
                    size_t id2 = static_cast<size_t>(ny) * W + cx;
                    if (J.vis[id2] || IsFence(J, cx, ny)) break; else cx++;
                }
                int endx = cx - 1;
                J.stack.emplace_back((start + endx) / 2, ny);
            }
            };

        enqueueRow(y - 1);
        enqueueRow(y + 1);
        steps++;
    }

    if (J.stack.empty()) return true; // 完成
    return false;                      // 还有剩余
}

static void FinishFenceFillJob() {
    KillTimer(g_hwnd, IDT_FENCE_FILL);
    if (!g_fillJob.active) return;

    if (g_fillJob.touchedEdge) {
        g_fillJob = FenceFillJob();
        MessageBox(g_hwnd, TEXT("未发现封闭图形或区域与窗口边缘连通，已取消栅栏填充。"), TEXT("栅栏填充"), MB_OK | MB_ICONWARNING);
        return;
    }

    // 封闭：提交结果到持久层
    g_fenceFills.push_back({ g_fillJob.partialSpans, g_fillJob.fillColor });
    g_fillJob = FenceFillJob();
}

// ================== 实验二渲染 ==================
void RenderExperiment2(HWND hwnd) {
    PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
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
        AppendMenu(hFillMenu, MF_STRING, ID_FILL_SCANLINE, TEXT("扫描线填充 (点击封闭图形)"));
        AppendMenu(hFillMenu, MF_STRING, ID_FILL_FENCE_MODE, TEXT("栅栏填充模式 (点击区域内部)"));
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
        if ((id >= ID_DRAW_LINE_MIDPOINT && id <= ID_DRAW_BSPLINE) ||
            id == ID_FILL_FENCE_MODE || id == ID_FILL_SCANLINE) {
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
            currentMode = FILL_FENCE; // 等待点击种子点
            break;

        case ID_FILL_SCANLINE:
            if (currentExperiment == 2) {
                // 进入扫描线填充模式：点击封闭多边形/矩形/圆内部完成填充
                currentMode = FILL_SCANLINE_MODE;
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
            if (currentMode == FILL_FENCE) {
                // 栅栏填充：异步，不阻塞其它按钮
                StartFenceFillJob(p.x, p.y);
            }
            else if (currentMode == FILL_SCANLINE_MODE) {
                // 扫描线填充模式：点击封闭多边形/矩形/圆内部进行填充
                for (int i = static_cast<int>(shapes.size()) - 1; i >= 0; --i) {
                    const Shape& s = shapes[i];
                    if (ShapeContainsPoint(s, p.x, p.y)) {
                        shapes[i].isFilled = true;
                        shapes[i].fillColor = RGB(200, 200, 255);
                        InvalidateRect(hwnd, nullptr, FALSE);
                        break;
                    }
                }
            }
            else if (currentMode == POLYGON || currentMode == BSPLINE) {
                // 左键添加顶点，并开启预览
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

    case WM_TIMER:
        if (wParam == IDT_FENCE_FILL) {
            if (!g_fillJob.active) {
                KillTimer(hwnd, IDT_FENCE_FILL);
                return 0;
            }

            if (!g_fillJob.regionReady) {
                // 先把区域逻辑填充完整（这里给较大的预算，避免拖太久）
                bool done = FenceFillStep(20000);
                if (done) {
                    g_fillJob.regionReady = true;
                    // 若发现区域与边界连通，则视为不封闭，直接结束
                    if (g_fillJob.touchedEdge) {
                        FinishFenceFillJob();
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            else {
                // 动画阶段：从右往左扫，逐步显示 partialSpans
                g_fillJob.sweepX -= g_fenceFillSweepPixelsPerTick;
                if (g_fillJob.sweepX <= 0) {
                    // 动画结束：把结果提交到永久区域
                    FinishFenceFillJob();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }

            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
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
        CancelFenceFillJob();
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
