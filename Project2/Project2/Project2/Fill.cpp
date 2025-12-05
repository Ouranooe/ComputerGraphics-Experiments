#include "Fill.h"
#include <cmath>
#include <algorithm>

namespace GraphicsEngine {

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

} // namespace GraphicsEngine
