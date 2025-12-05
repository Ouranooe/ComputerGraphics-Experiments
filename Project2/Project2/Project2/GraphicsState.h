#pragma once

#include <windows.h>
#include <vector>
#include "GraphicsEngine.h"

namespace GraphicsEngine {

extern HWND g_hwnd;
extern HDC g_hdcMem;
extern HBITMAP g_hbmMem;

extern DrawMode g_currentMode;
extern std::vector<Shape> g_shapes;
extern std::vector<Point> g_currentPoints;
extern bool g_isDrawing;

extern COLORREF g_drawColor;
extern COLORREF g_fillColor;

extern int g_selectedShapeIndex;
extern Point g_firstClick;
extern double g_scaleBaseDist;
extern double g_rotBaseAngle;

// 鼠标当前位置（用于预览）
extern Point g_currentMousePos;

void RecreateBackBuffer(HWND hwnd);

} // namespace GraphicsEngine
