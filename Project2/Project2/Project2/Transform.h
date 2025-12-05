#pragma once

#include "GraphicsEngine.h"

namespace GraphicsEngine {

void TranslateShape(Shape& s, int dx, int dy);
void ScaleShape(Shape& s, const Point& center, double sx, double sy);
void RotateShape(Shape& s, const Point& center, double angleRad);

} // namespace GraphicsEngine
