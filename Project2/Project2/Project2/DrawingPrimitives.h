#pragma once

#include <windows.h>
#include <vector>
#include "GraphicsState.h"

namespace GraphicsEngine {

void BSplineBase(float t, float* b);
void DrawPixel(HDC hdc, int x, int y, COLORREF c);
void DrawPixelXor(HDC hdc, int x, int y, COLORREF c);
void DrawLineMidpoint(HDC hdc, int x1, int y1, int x2, int y2, COLORREF c);
void DrawLineBresenham(HDC hdc, int x1, int y1, int x2, int y2, COLORREF c);
void DrawCirclePoints(HDC hdc, int xc, int yc, int x, int y, COLORREF c);
void DrawCircleMidpoint(HDC hdc, int xc, int yc, int r, COLORREF c);
void DrawCircleBresenham(HDC hdc, int xc, int yc, int r, COLORREF c);
void DrawPolyline(HDC hdc, const std::vector<Point>& v, COLORREF c, bool closed);
void DrawBSpline(HDC hdc, const std::vector<Point>& ctrl, COLORREF c);

} // namespace GraphicsEngine
