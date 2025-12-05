#pragma once

#include "GraphicsState.h"
#include "DrawingPrimitives.h"

namespace GraphicsEngine {

double Dist2PointSeg(double x, double y, double x1, double y1, double x2, double y2);
bool PointInShape(const Shape& s, int x, int y);
int HitTestShape(int x, int y);
void DrawShapeBorder(HDC hdc, const Shape& s);

} // namespace GraphicsEngine
