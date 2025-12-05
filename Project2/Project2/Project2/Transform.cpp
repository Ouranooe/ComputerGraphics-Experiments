#include "Transform.h"
#include <cmath>
#include <vector>

namespace GraphicsEngine {

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
    // 矩形：先变成4顶点的多边形，再旋转
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
        double sn = std::sin(angleRad);
        for (auto& p : poly) {
            double dx = p.x - center.x;
            double dy = p.y - center.y;
            double nx = dx * c - dy * sn;
            double ny = dx * sn + dy * c;
            p.x = (int)std::round(center.x + nx);
            p.y = (int)std::round(center.y + ny);
        }

        s.type = DrawMode::DrawPolygon;   // 之后按多边形处理
        s.vertices = poly;
        return;
    }

    // 其他图形：直接对顶点旋转
    double c = std::cos(angleRad);
    double sn = std::sin(angleRad);
    for (auto& p : s.vertices) {
        double dx = p.x - center.x;
        double dy = p.y - center.y;
        double nx = dx * c - dy * sn;
        double ny = dx * sn + dy * c;
        p.x = (int)std::round(center.x + nx);
        p.y = (int)std::round(center.y + ny);
    }
}

} // namespace GraphicsEngine
