#include "DrawingPrimitives.h"
#include <cmath>

namespace GraphicsEngine {

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

} // namespace GraphicsEngine
