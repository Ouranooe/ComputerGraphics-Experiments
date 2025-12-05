#pragma once

#include "GraphicsState.h"
#include "DrawingPrimitives.h"

namespace GraphicsEngine {

// 扫描线填充
void FillPolygonScanline(HDC hdc, const std::vector<Point>& v, COLORREF c, bool innerOnly);
void FillRectScanline(HDC hdc, const Point& p1, const Point& p2, COLORREF c, bool innerOnly);
void FillCircleScanline(HDC hdc, const Point& center, const Point& onCircle, COLORREF c, bool innerOnly);
void FillShapeScanline(HDC hdc, const Shape& s, COLORREF c);

// 栅栏填充 (XOR)
void FenceFillRect(HDC hdc, const Point& p1, const Point& p2, COLORREF xorColor);
void FenceFillCircle(HDC hdc, const Point& center, const Point& onCircle, COLORREF xorColor);
void FenceFillPolygon(HDC hdc, const std::vector<Point>& v, COLORREF xorColor);
void FillShapeFence(HDC hdc, const Shape& s, COLORREF fillColor);

} // namespace GraphicsEngine
