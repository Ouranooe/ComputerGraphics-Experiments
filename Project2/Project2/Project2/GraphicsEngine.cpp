#include "GraphicsEngine.h"

#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <utility>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

namespace GraphicsEngine {
namespace {

HWND g_hwnd = nullptr;
HDC g_hdcMem = nullptr;
HBITMAP g_hbmMem = nullptr;

DrawMode g_currentMode = DrawMode::None;
std::vector<Shape> g_shapes;
std::vector<Point> g_currentPoints;
bool g_isDrawing = false;

COLORREF g_drawColor = RGB(0, 0, 0);
COLORREF g_fillColor = RGB(253, 151, 47);

int g_selectedShapeIndex = -1;
Point g_firstClick{ 0, 0 };
double g_scaleBaseDist = 1.0;
double g_rotBaseAngle = 0.0;

void RecreateBackBuffer(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    HDC hdc = GetDC(hwnd);
    if (!g_hdcMem) {
        g_hdcMem = CreateCompatibleDC(hdc);
    }
    RECT rc{};
    GetClientRect(hwnd, &rc);
    HBITMAP newBitmap = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(g_hdcMem, newBitmap));
    if (oldBitmap && oldBitmap != g_hbmMem) {
        DeleteObject(oldBitmap);
    }
    if (g_hbmMem && g_hbmMem != newBitmap) {
        DeleteObject(g_hbmMem);
    }
    g_hbmMem = newBitmap;
    ReleaseDC(hwnd, hdc);
}

void BSplineBase(float t, float* b) {
    float t2 = t * t;
    float t3 = t2 * t;
    b[0] = (-t3 + 3 * t2 - 3 * t + 1) / 6.0f;
    b[1] = (3 * t3 - 6 * t2 + 4) / 6.0f;
    b[2] = (-3 * t3 + 3 * t2 + 3 * t + 1) / 6.0f;
    b[3] = t3 / 6.0f;
}

void DrawPixel(HDC hdc, int x, int y, COLORREF c) {
    SetPixel(hdc, x, y, c);
}

void DrawPixelXor(HDC hdc, int x, int y, COLORREF c) {
    COLORREF old = GetPixel(hdc, x, y);
    if (old == CLR_INVALID) old = RGB(255, 255, 255);
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

void DrawPolyline(HDC hdc, const std::vector<Point>& v, COLORREF c, bool closed) {
    if (v.size() < 2) return;
    for (size_t i = 0; i + 1 < v.size(); ++i)
        DrawLineMidpoint(hdc, v[i].x, v[i].y, v[i + 1].x, v[i + 1].y, c);
    if (closed)
        DrawLineMidpoint(hdc, v.back().x, v.back().y, v.front().x, v.front().y, c);
}

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

double Dist2PointSeg(double x, double y, double x1, double y1, double x2, double y2) {
    double vx = x2 - x1, vy = y2 - y1;
    double wx = x - x1, wy = y - y1;
    double c1 = vx * wx + vy * wy;
    if (c1 <= 0) return (x - x1) * (x - x1) + (y - y1) * (y - y1);
    double c2 = vx * vx + vy * vy;
    if (c2 <= c1) return (x - x2) * (x - x2) + (y - y2) * (y - y2);
    double t = c1 / c2;
    double px = x1 + t * vx;
    double py = y1 + t * vy;
    return (x - px) * (x - px) + (y - py) * (y - py);
}

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
    case DrawMode::DrawLineMidpoint:
    case DrawMode::DrawLineBresenham: {
        double d2 = Dist2PointSeg((double)x, (double)y,
            (double)v[0].x, (double)v[0].y,
            (double)v[1].x, (double)v[1].y);
        return d2 <= 9.0;
    }
    default:
        return false;
    }
}

int HitTestShape(int x, int y) {
    for (int i = (int)g_shapes.size() - 1; i >= 0; --i) {
        if (PointInShape(g_shapes[i], x, y))
            return i;
    }
    return -1;
}

void FillPolygonScanline(HDC hdc, const std::vector<Point>& v, COLORREF c, bool innerOnly) {
    if (v.size() < 3) return;
    int ymin = v[0].y, ymax = v[0].y;
    for (auto& p : v) { ymin = min(ymin, p.y); ymax = max(ymax, p.y); }

    for (int y = ymin; y <= ymax; ++y) {
        std::vector<float> xs;
        for (size_t i = 0; i < v.size(); ++i) {
            Point p1 = v[i], p2 = v[(i + 1) % v.size()];
            if (p1.y == p2.y) continue;
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

void FenceFillRect(HDC hdc, const Point& p1, const Point& p2, COLORREF xorColor) {
    int x1 = min(p1.x, p2.x);
    int x2 = max(p1.x, p2.x);
    int y1 = min(p1.y, p2.y);
    int y2 = max(p1.y, p2.y);
    int fenceX = x1 - 1;

    for (int y = y1; y <= y2; ++y) {
        int xs[2] = { x1, x2 };
        for (int k = 0; k < 2; ++k) {
            int xEnd = xs[k];
            if (xEnd < fenceX) continue;
            for (int x = fenceX; x <= xEnd; ++x)
                DrawPixelXor(hdc, x, y, xorColor);
        }
    }
}

void FenceFillCircle(HDC hdc, const Point& center, const Point& onCircle, COLORREF xorColor) {
    int xc = center.x, yc = center.y;
    int r = int(std::sqrt(double((onCircle.x - xc) * (onCircle.x - xc) +
        (onCircle.y - yc) * (onCircle.y - yc))));
    if (r <= 0) return;

    int minX = xc - r;
    int fenceX = minX - 1;

    for (int y = yc - r; y <= yc + r; ++y) {
        int dy = y - yc;
        int t = r * r - dy * dy;
        if (t < 0) continue;
        int dx = int(std::sqrt(double(t)));
        int xs[2] = { xc - dx, xc + dx };
        for (int k = 0; k < 2; ++k) {
            int xEnd = xs[k];
            if (xEnd < fenceX) continue;
            for (int x = fenceX; x <= xEnd; ++x)
                DrawPixelXor(hdc, x, y, xorColor);
        }
    }
}

void FenceFillPolygon(HDC hdc, const std::vector<Point>& v, COLORREF xorColor) {
    if (v.size() < 3) return;

    int minX = v[0].x, ymin = v[0].y, ymax = v[0].y;
    for (auto& p : v) {
        minX = min(minX, p.x);
        ymin = min(ymin, p.y);
        ymax = max(ymax, p.y);
    }
    int fenceX = minX - 1;

    for (int y = ymin; y <= ymax; ++y) {
        std::vector<float> xs;
        for (size_t i = 0; i < v.size(); ++i) {
            Point p1 = v[i], p2 = v[(i + 1) % v.size()];
            if (p1.y == p2.y) continue;
            int yMin = min(p1.y, p2.y);
            int yMax = max(p1.y, p2.y);
            if (y < yMin || y >= yMax) continue;
            float x = p1.x + (float)(y - p1.y) * (float)(p2.x - p1.x) / (float)(p2.y - p1.y);
            xs.push_back(x);
        }
        if (xs.empty()) continue;
        std::sort(xs.begin(), xs.end());
        for (size_t i = 0; i < xs.size(); ++i) {
            int xEnd = (int)std::floor(xs[i]);
            if (xEnd < fenceX) continue;
            for (int x = fenceX; x <= xEnd; ++x)
                DrawPixelXor(hdc, x, y, xorColor);
        }
    }
}

void FillShapeFence(HDC hdc, const Shape& s, COLORREF fillColor) {
    const auto& v = s.vertices;
    if (v.size() < 2) return;

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

void TranslateShape(Shape& s, int dx, int dy) {
    for (auto& p : s.vertices) {
        p.x += dx;
        p.y += dy;
    }
}

void ScaleShape(Shape& s, const Point& center, double sx, double sy) {
    for (auto& p : s.vertices) {
        double dx = p.x - center.x;
        double dy = p.y - center.y;
        p.x = (int)std::round(center.x + dx * sx);
        p.y = (int)std::round(center.y + dy * sy);
    }
}

void RotateShape(Shape& s, const Point& center, double angleRad) {
    // 矩形：先变成4个顶点的多边形，再旋转
    if (s.type == DrawMode::DrawRectangle && s.vertices.size() >= 2) {
        Point p0 = s.vertices[0];
        Point p1 = s.vertices[1];

        int x1 = min(p0.x, p1.x);
        int x2 = max(p0.x, p1.x);
        int y1 = min(p0.y, p1.y);
        int y2 = max(p0.y, p1.y);

        std::vector<Point> poly(4);
        poly[0] = { x1, y1 };
        poly[1] = { x2, y1 };
        poly[2] = { x2, y2 };
        poly[3] = { x1, y2 };

        double c = std::cos(angleRad);
        double s1 = std::sin(angleRad);
        for (auto& p : poly) {
            double dx = p.x - center.x;
            double dy = p.y - center.y;
            double nx = dx * c - dy * s1;
            double ny = dx * s1 + dy * c;
            p.x = (int)std::round(center.x + nx);
            p.y = (int)std::round(center.y + ny);
        }

        s.type = DrawMode::DrawPolygon;   // 之后当作多边形处理
        s.vertices = poly;
        return;
    }

    // 其他图形：按顶点逐个旋转
    double c = std::cos(angleRad);
    double s1 = std::sin(angleRad);
    for (auto& p : s.vertices) {
        double dx = p.x - center.x;
        double dy = p.y - center.y;
        double nx = dx * c - dy * s1;
        double ny = dx * s1 + dy * c;
        p.x = (int)std::round(center.x + nx);
        p.y = (int)std::round(center.y + ny);
    }
}


const int CS_INSIDE = 0;
const int CS_LEFT = 1;
const int CS_RIGHT = 2;
const int CS_BOTTOM = 4;
const int CS_TOP = 8;

int CS_GetOutCode(double x, double y, double xmin, double xmax, double ymin, double ymax) {
    int code = CS_INSIDE;
    if (x < xmin) code |= CS_LEFT;
    else if (x > xmax) code |= CS_RIGHT;
    if (y < ymin) code |= CS_TOP;
    else if (y > ymax) code |= CS_BOTTOM;
    return code;
}

bool CohenSutherlandClip(double& x1, double& y1, double& x2, double& y2,
    double xmin, double xmax, double ymin, double ymax) {
    int out1 = CS_GetOutCode(x1, y1, xmin, xmax, ymin, ymax);
    int out2 = CS_GetOutCode(x2, y2, xmin, xmax, ymin, ymax);
    bool accept = false;
    while (true) {
        if ((out1 | out2) == 0) { accept = true; break; }
        else if (out1 & out2) { break; }
        else {
            double x, y;
            int out = out1 ? out1 : out2;
            if (out & CS_TOP) {
                y = ymin;
                x = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            }
            else if (out & CS_BOTTOM) {
                y = ymax;
                x = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            }
            else if (out & CS_RIGHT) {
                x = xmax;
                y = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
            }
            else {
                x = xmin;
                y = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
            }
            if (out == out1) {
                x1 = x; y1 = y;
                out1 = CS_GetOutCode(x1, y1, xmin, xmax, ymin, ymax);
            }
            else {
                x2 = x; y2 = y;
                out2 = CS_GetOutCode(x2, y2, xmin, xmax, ymin, ymax);
            }
        }
    }
    return accept;
}

void ClipAllLines_CohenSutherland(const RECT& clip) {
    double xmin = clip.left;
    double xmax = clip.right;
    double ymin = clip.top;
    double ymax = clip.bottom;

    std::vector<Shape> newShapes;
    for (auto s : g_shapes) {
        if (s.type == DrawMode::DrawLineMidpoint || s.type == DrawMode::DrawLineBresenham) {
            double x1 = s.vertices[0].x;
            double y1 = s.vertices[0].y;
            double x2 = s.vertices[1].x;
            double y2 = s.vertices[1].y;
            if (CohenSutherlandClip(x1, y1, x2, y2, xmin, xmax, ymin, ymax)) {
                s.vertices[0].x = (int)std::round(x1);
                s.vertices[0].y = (int)std::round(y1);
                s.vertices[1].x = (int)std::round(x2);
                s.vertices[1].y = (int)std::round(y2);
                newShapes.push_back(s);
            }
        }
        else {
            newShapes.push_back(s);
        }
    }
    g_shapes.swap(newShapes);
}

bool InsideRect(double x, double y, double xmin, double xmax, double ymin, double ymax) {
    return x >= xmin && x <= xmax && y >= ymin && y <= ymax;
}

void MidClipLineRec(double x1, double y1, double x2, double y2,
    double xmin, double xmax, double ymin, double ymax,
    int depth,
    std::vector<std::pair<Point, Point>>& outSegs) {
    bool in1 = InsideRect(x1, y1, xmin, xmax, ymin, ymax);
    bool in2 = InsideRect(x2, y2, xmin, xmax, ymin, ymax);

    if (in1 && in2) {
        Point a{ (int)std::round(x1), (int)std::round(y1) };
        Point b{ (int)std::round(x2), (int)std::round(y2) };
        outSegs.push_back({ a, b });
        return;
    }
    if (depth > 20) {
        if (in1 || in2) {
            Point a{ (int)std::round(x1), (int)std::round(y1) };
            Point b{ (int)std::round(x2), (int)std::round(y2) };
            outSegs.push_back({ a, b });
        }
        return;
    }

    double dx = x2 - x1;
    double dy = y2 - y1;
    if (std::fabs(dx) < 0.5 && std::fabs(dy) < 0.5) {
        if (in1 && in2) {
            Point a{ (int)std::round(x1), (int)std::round(y1) };
            Point b{ (int)std::round(x2), (int)std::round(y2) };
            outSegs.push_back({ a, b });
        }
        return;
    }

    double mx = (x1 + x2) * 0.5;
    double my = (y1 + y2) * 0.5;

    MidClipLineRec(x1, y1, mx, my, xmin, xmax, ymin, ymax, depth + 1, outSegs);
    MidClipLineRec(mx, my, x2, y2, xmin, xmax, ymin, ymax, depth + 1, outSegs);
}

void ClipAllLines_Midpoint(const RECT& clip) {
    double xmin = clip.left;
    double xmax = clip.right;
    double ymin = clip.top;
    double ymax = clip.bottom;

    std::vector<Shape> newShapes;
    for (auto s : g_shapes) {
        if (s.type == DrawMode::DrawLineMidpoint || s.type == DrawMode::DrawLineBresenham) {
            std::vector<std::pair<Point, Point>> segs;
            MidClipLineRec(
                (double)s.vertices[0].x, (double)s.vertices[0].y,
                (double)s.vertices[1].x, (double)s.vertices[1].y,
                xmin, xmax, ymin, ymax, 0, segs
            );
            for (auto& seg : segs) {
                Shape ns = s;
                ns.vertices.clear();
                ns.vertices.push_back(seg.first);
                ns.vertices.push_back(seg.second);
                newShapes.push_back(ns);
            }
        }
        else {
            newShapes.push_back(s);
        }
    }
    g_shapes.swap(newShapes);
}

Point IntersectEdge(const Point& p1, const Point& p2, char edge, const RECT& r) {
    double x1 = p1.x, y1 = p1.y;
    double x2 = p2.x, y2 = p2.y;
    double x = 0, y = 0;
    switch (edge) {
    case 'L': x = r.left;  y = y1 + (y2 - y1) * (x - x1) / (x2 - x1); break;
    case 'R': x = r.right; y = y1 + (y2 - y1) * (x - x1) / (x2 - x1); break;
    case 'T': y = r.top;   x = x1 + (x2 - x1) * (y - y1) / (y2 - y1); break;
    case 'B': y = r.bottom;x = x1 + (x2 - x1) * (y - y1) / (y2 - y1); break;
    }
    return { (int)std::round(x), (int)std::round(y) };
}

bool InsideEdge(const Point& p, char edge, const RECT& r) {
    switch (edge) {
    case 'L': return p.x >= r.left;
    case 'R': return p.x <= r.right;
    case 'T': return p.y >= r.top;
    case 'B': return p.y <= r.bottom;
    }
    return false;
}

std::vector<Point> ClipWithEdge(const std::vector<Point>& poly, char edge, const RECT& r) {
    std::vector<Point> out;
    if (poly.empty()) return out;
    Point S = poly.back();
    for (auto& E : poly) {
        bool Sin = InsideEdge(S, edge, r);
        bool Ein = InsideEdge(E, edge, r);
        if (Sin && Ein) {
            out.push_back(E);
        }
        else if (Sin && !Ein) {
            Point I = IntersectEdge(S, E, edge, r);
            out.push_back(I);
        }
        else if (!Sin && Ein) {
            Point I = IntersectEdge(S, E, edge, r);
            out.push_back(I);
            out.push_back(E);
        }
        S = E;
    }
    return out;
}

std::vector<Point> ClipPolygon_SutherlandHodgman(const std::vector<Point>& poly, const RECT& r) {
    std::vector<Point> out = poly;
    out = ClipWithEdge(out, 'L', r);
    out = ClipWithEdge(out, 'R', r);
    out = ClipWithEdge(out, 'T', r);
    out = ClipWithEdge(out, 'B', r);
    return out;
}

std::vector<Point> ClipPolygon_WeilerAtherton_Rect(const std::vector<Point>& poly, const RECT& r) {
    return ClipPolygon_SutherlandHodgman(poly, r);
}

void ClipAllPolygons_SH(const RECT& clip) {
    for (auto& s : g_shapes) {
        if (s.type == DrawMode::DrawPolygon)
            s.vertices = ClipPolygon_SutherlandHodgman(s.vertices, clip);
    }
    g_shapes.erase(
        std::remove_if(g_shapes.begin(), g_shapes.end(),
            [](const Shape& s) { return s.type == DrawMode::DrawPolygon && s.vertices.size() < 3; }),
        g_shapes.end()
    );
}

void ClipAllPolygons_WA(const RECT& clip) {
    for (auto& s : g_shapes) {
        if (s.type == DrawMode::DrawPolygon)
            s.vertices = ClipPolygon_WeilerAtherton_Rect(s.vertices, clip);
    }
    g_shapes.erase(
        std::remove_if(g_shapes.begin(), g_shapes.end(),
            [](const Shape& s) { return s.type == DrawMode::DrawPolygon && s.vertices.size() < 3; }),
        g_shapes.end()
    );
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
    }
    else if (s.type == DrawMode::DrawBSpline && s.vertices.size() < 4) {
    }
    else {
        g_shapes.push_back(s);
    }

    g_currentPoints.clear();
    if (g_currentMode != DrawMode::DrawPolygon &&
        g_currentMode != DrawMode::DrawBSpline) {
        g_isDrawing = false;
    }
    if (g_hwnd)
        InvalidateRect(g_hwnd, NULL, FALSE);
}

void ClearCanvas() {
    g_shapes.clear();
    g_currentPoints.clear();
    g_isDrawing = false;
    g_currentMode = DrawMode::None;
    g_selectedShapeIndex = -1;
    if (g_hwnd)
        InvalidateRect(g_hwnd, NULL, TRUE);
}

void RedrawAllShapesOn(HDC hdc, const std::vector<Shape>& shapes) {
    for (const auto& s : shapes) {
        if (s.fillMode == 1)
            FillShapeScanline(hdc, s, s.fillColor);
        else if (s.fillMode == 2)
            FillShapeFence(hdc, s, s.fillColor);
        DrawShapeBorder(hdc, s);
    }
}

void RedrawAllShapes(HDC hdc) {
    RedrawAllShapesOn(hdc, g_shapes);
}

} // namespace

void Initialize(HWND hwnd) {
    g_hwnd = hwnd;
    RecreateBackBuffer(hwnd);
}

void Shutdown() {
    if (g_hbmMem) {
        DeleteObject(g_hbmMem);
        g_hbmMem = nullptr;
    }
    if (g_hdcMem) {
        DeleteDC(g_hdcMem);
        g_hdcMem = nullptr;
    }
    g_hwnd = nullptr;
    g_shapes.clear();
    g_currentPoints.clear();
    g_isDrawing = false;
    g_selectedShapeIndex = -1;
}

void Resize(HWND hwnd) {
    g_hwnd = hwnd;
    if (!g_hdcMem) {
        RecreateBackBuffer(hwnd);
    }
    else {
        RecreateBackBuffer(hwnd);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

void HandleCommand(int commandId) {
    switch (commandId) {
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
    case ID_TRANS_TRANSLATE:
        g_currentMode = DrawMode::TransformTranslate; g_isDrawing = false; g_selectedShapeIndex = -1; break;
    case ID_TRANS_SCALE:
        g_currentMode = DrawMode::TransformScale;     g_isDrawing = false; g_selectedShapeIndex = -1; break;
    case ID_TRANS_ROTATE:
        g_currentMode = DrawMode::TransformRotate;    g_isDrawing = false; g_selectedShapeIndex = -1; break;
    case ID_CLIP_LINE_CS:
        g_currentMode = DrawMode::ClipLineCS;         g_isDrawing = false; break;
    case ID_CLIP_LINE_MID:
        g_currentMode = DrawMode::ClipLineMid;        g_isDrawing = false; break;
    case ID_CLIP_POLY_SH:
        g_currentMode = DrawMode::ClipPolySH;         g_isDrawing = false; break;
    case ID_CLIP_POLY_WA:
        g_currentMode = DrawMode::ClipPolyWA;         g_isDrawing = false; break;
    case ID_EDIT_FINISH:
        FinishDrawing();                              break;
    case ID_EDIT_CLEAR:
        ClearCanvas();                                break;
    default:
        break;
    }
}

void HandleLButtonDown(int x, int y) {
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
        g_currentPoints.push_back({ x, y });
        if (g_hwnd)
            InvalidateRect(g_hwnd, NULL, FALSE);
        break;

    case DrawMode::FillScanline:
    case DrawMode::FillFence: {
        for (auto& s : g_shapes) {
            if (PointInShape(s, x, y)) {
                s.fillColor = g_fillColor;
                if (g_currentMode == DrawMode::FillScanline) {
                    s.fillMode = 1;
                    if (g_hdcMem)
                        FillShapeScanline(g_hdcMem, s, s.fillColor);
                }
                else {
                    s.fillMode = 2;
                    if (g_hdcMem)
                        FillShapeFence(g_hdcMem, s, s.fillColor);
                }
                if (g_hwnd)
                    InvalidateRect(g_hwnd, NULL, FALSE);
                break;
            }
        }
    } break;

    case DrawMode::TransformTranslate: {
        if (!g_isDrawing) {
            int idx = HitTestShape(x, y);
            if (idx >= 0) {
                g_selectedShapeIndex = idx;
                g_firstClick = { x, y };
                g_isDrawing = true;
            }
        }
        else {
            int dx = x - g_firstClick.x;
            int dy = y - g_firstClick.y;
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size())
                TranslateShape(g_shapes[g_selectedShapeIndex], dx, dy);
            g_isDrawing = false;
            if (g_hwnd)
                InvalidateRect(g_hwnd, NULL, FALSE);
        }
    } break;

    case DrawMode::TransformScale: {
        if (!g_isDrawing) {
            int idx = HitTestShape(x, y);
            if (idx >= 0) {
                g_selectedShapeIndex = idx;
                g_firstClick = { x, y };
                if (!g_shapes[idx].vertices.empty()) {
                    Point v0 = g_shapes[idx].vertices[0];
                    double dx = v0.x - g_firstClick.x;
                    double dy = v0.y - g_firstClick.y;
                    g_scaleBaseDist = std::sqrt(dx * dx + dy * dy);
                    if (g_scaleBaseDist < 1) g_scaleBaseDist = 1;
                }
                g_isDrawing = true;
            }
        }
        else {
            double dx = x - g_firstClick.x;
            double dy = y - g_firstClick.y;
            double dist = std::sqrt(dx * dx + dy * dy);
            double s = dist / g_scaleBaseDist;
            if (s < 0.01) s = 0.01;
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size())
                ScaleShape(g_shapes[g_selectedShapeIndex], g_firstClick, s, s);
            g_isDrawing = false;
            if (g_hwnd)
                InvalidateRect(g_hwnd, NULL, FALSE);
        }
    } break;

    case DrawMode::TransformRotate: {
        if (!g_isDrawing) {
            int idx = HitTestShape(x, y);
            if (idx >= 0) {
                g_selectedShapeIndex = idx;
                g_firstClick = { x, y };
                if (!g_shapes[idx].vertices.empty()) {
                    Point v0 = g_shapes[idx].vertices[0];
                    double dx = v0.x - g_firstClick.x;
                    double dy = v0.y - g_firstClick.y;
                    g_rotBaseAngle = std::atan2(dy, dx);
                }
                g_isDrawing = true;
            }
        }
        else {
            double dx = x - g_firstClick.x;
            double dy = y - g_firstClick.y;
            double ang1 = std::atan2(dy, dx);
            double delta = ang1 - g_rotBaseAngle;
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size())
                RotateShape(g_shapes[g_selectedShapeIndex], g_firstClick, delta);
            g_isDrawing = false;
            if (g_hwnd)
                InvalidateRect(g_hwnd, NULL, FALSE);
        }
    } break;

    case DrawMode::ClipLineCS:
    case DrawMode::ClipLineMid:
    case DrawMode::ClipPolySH:
    case DrawMode::ClipPolyWA: {
        if (!g_isDrawing) {
            g_firstClick = { x, y };
            g_isDrawing = true;
        }
        else {
            Point p2{ x, y };
            g_isDrawing = false;
            RECT rc;
            rc.left = min(g_firstClick.x, p2.x);
            rc.right = max(g_firstClick.x, p2.x);
            rc.top = min(g_firstClick.y, p2.y);
            rc.bottom = max(g_firstClick.y, p2.y);

            if (g_currentMode == DrawMode::ClipLineCS)
                ClipAllLines_CohenSutherland(rc);
            else if (g_currentMode == DrawMode::ClipLineMid)
                ClipAllLines_Midpoint(rc);
            else if (g_currentMode == DrawMode::ClipPolySH)
                ClipAllPolygons_SH(rc);
            else if (g_currentMode == DrawMode::ClipPolyWA)
                ClipAllPolygons_WA(rc);

            if (g_hwnd)
                InvalidateRect(g_hwnd, NULL, FALSE);
        }
    } break;

    default: break;
    }
}

void HandleMouseMove(int x, int y) {
    if (!g_hwnd || !g_hdcMem) {
        return;
    }

    HDC hdc = GetDC(g_hwnd);
    RECT rc; GetClientRect(g_hwnd, &rc);

    FillRect(g_hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    std::vector<Shape> temp;
    const std::vector<Shape>* shapesToDraw = &g_shapes;

    if (g_isDrawing) {
        switch (g_currentMode) {
        case DrawMode::TransformTranslate:
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size()) {
                temp = g_shapes;
                int dx = x - g_firstClick.x;
                int dy = y - g_firstClick.y;
                TranslateShape(temp[g_selectedShapeIndex], dx, dy);
                shapesToDraw = &temp;
            }
            break;
        case DrawMode::TransformScale:
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size()) {
                temp = g_shapes;
                double dx = x - g_firstClick.x;
                double dy = y - g_firstClick.y;
                double dist = std::sqrt(dx * dx + dy * dy);
                double s = dist / g_scaleBaseDist;
                if (s < 0.01) s = 0.01;
                ScaleShape(temp[g_selectedShapeIndex], g_firstClick, s, s);
                shapesToDraw = &temp;
            }
            break;
        case DrawMode::TransformRotate:
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size()) {
                temp = g_shapes;
                double dx = x - g_firstClick.x;
                double dy = y - g_firstClick.y;
                double ang1 = std::atan2(dy, dx);
                double delta = ang1 - g_rotBaseAngle;
                RotateShape(temp[g_selectedShapeIndex], g_firstClick, delta);
                shapesToDraw = &temp;
            }
            break;
        default:
            break;
        }
    }

    RedrawAllShapesOn(g_hdcMem, *shapesToDraw);

    if (g_isDrawing && !g_currentPoints.empty()) {
        Point p0 = g_currentPoints[0];
        switch (g_currentMode) {
        case DrawMode::DrawLineMidpoint:
            DrawLineMidpoint(g_hdcMem, p0.x, p0.y, x, y, g_drawColor); break;
        case DrawMode::DrawLineBresenham:
            DrawLineBresenham(g_hdcMem, p0.x, p0.y, x, y, g_drawColor); break;
        case DrawMode::DrawPolygon: {
            if (g_currentPoints.size() > 1)
                DrawPolyline(g_hdcMem, g_currentPoints, g_drawColor, false);
            Point last = g_currentPoints.back();
            DrawLineMidpoint(g_hdcMem, last.x, last.y, x, y, g_drawColor);
        } break;
        case DrawMode::DrawBSpline: {
            for (auto& p : g_currentPoints)
                Ellipse(g_hdcMem, p.x - 3, p.y - 3, p.x + 3, p.y + 3);
            std::vector<Point> tempCtrl = g_currentPoints;
            tempCtrl.push_back({ x, y });
            if (tempCtrl.size() > 1)
                DrawPolyline(g_hdcMem, tempCtrl, RGB(200, 200, 200), false);
            if (tempCtrl.size() >= 4)
                DrawBSpline(g_hdcMem, tempCtrl, g_drawColor);
        } break;
        default:
            break;
        }
    }

    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, g_hdcMem, 0, 0, SRCCOPY);
    ReleaseDC(g_hwnd, hdc);
}

void HandleMouseWheel(short delta) {
    if (g_currentMode == DrawMode::TransformScale &&
        g_selectedShapeIndex >= 0 &&
        g_selectedShapeIndex < (int)g_shapes.size()) {
        double step = 0.1;
        double s = 1.0 + step * (delta / 120.0);
        if (s < 0.1) s = 0.1;
        ScaleShape(g_shapes[g_selectedShapeIndex], g_firstClick, s, s);
        if (g_hwnd)
            InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

void OnPaint(HWND hwnd) {
    if (!g_hdcMem) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return;
    }

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);

    FillRect(g_hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    RedrawAllShapes(g_hdcMem);

    if (g_currentMode == DrawMode::DrawPolygon && g_currentPoints.size() >= 2)
        DrawPolyline(g_hdcMem, g_currentPoints, g_drawColor, false);

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

} // namespace GraphicsEngine
