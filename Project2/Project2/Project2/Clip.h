#pragma once

#include "GraphicsState.h"
#include <utility>

namespace GraphicsEngine {

// Cohen-Sutherland 线段裁剪
int CS_GetOutCode(double x, double y, double xmin, double xmax, double ymin, double ymax);
bool CohenSutherlandClip(double& x1, double& y1, double& x2, double& y2,
    double xmin, double xmax, double ymin, double ymax);
void ClipAllLines_CohenSutherland(const RECT& clip);

// 中点分割法线段裁剪
bool InsideRect(double x, double y, double xmin, double xmax, double ymin, double ymax);
void MidClipLineRec(double x1, double y1, double x2, double y2,
    double xmin, double xmax, double ymin, double ymax,
    int depth,
    std::vector<std::pair<Point, Point>>& outSegs);
void ClipAllLines_Midpoint(const RECT& clip);

// Sutherland-Hodgman 多边形裁剪
Point IntersectEdge(const Point& p1, const Point& p2, char edge, const RECT& r);
bool InsideEdge(const Point& p, char edge, const RECT& r);
std::vector<Point> ClipWithEdge(const std::vector<Point>& poly, char edge, const RECT& r);
std::vector<Point> ClipPolygon_SutherlandHodgman(const std::vector<Point>& poly, const RECT& r);

// Weiler-Atherton 多边形裁剪 (简化版——使用SH)
std::vector<Point> ClipPolygon_WeilerAtherton_Rect(const std::vector<Point>& poly, const RECT& r);

// 对所有图形执行裁剪
void ClipAllPolygons_SH(const RECT& clip);
void ClipAllPolygons_WA(const RECT& clip);

} // namespace GraphicsEngine
