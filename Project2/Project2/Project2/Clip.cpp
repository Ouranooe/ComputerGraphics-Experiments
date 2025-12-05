#include "Clip.h"
#include <cmath>
#include <algorithm>

namespace GraphicsEngine {

// ----- Cohen-Sutherland 裁剪码 -----
static const int CS_INSIDE = 0;
static const int CS_LEFT   = 1;
static const int CS_RIGHT  = 2;
static const int CS_BOTTOM = 4;
static const int CS_TOP    = 8;

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

// ----- 中点分割法 -----
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

// ----- Sutherland-Hodgman 多边形裁剪 -----
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

} // namespace GraphicsEngine
