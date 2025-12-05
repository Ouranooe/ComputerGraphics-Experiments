#include "Shapes.h"
#include <cmath>
#include <algorithm>

namespace GraphicsEngine {

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

} // namespace GraphicsEngine
