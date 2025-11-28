#pragma once

#include <windows.h>
#include <vector>

namespace GraphicsEngine {

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;

constexpr UINT ID_DRAW_LINE_MIDPOINT = 1001;
constexpr UINT ID_DRAW_LINE_BRESENHAM = 1002;
constexpr UINT ID_DRAW_CIRCLE_MIDPOINT = 1003;
constexpr UINT ID_DRAW_CIRCLE_BRESENHAM = 1004;
constexpr UINT ID_DRAW_RECTANGLE = 1005;
constexpr UINT ID_DRAW_POLYGON = 1006;
constexpr UINT ID_DRAW_BSPLINE = 1007;
constexpr UINT ID_FILL_SCANLINE = 1008;
constexpr UINT ID_FILL_FENCE = 1009;
constexpr UINT ID_EDIT_FINISH = 1010;
constexpr UINT ID_EDIT_CLEAR = 1011;
constexpr UINT ID_TRANS_TRANSLATE = 1012;
constexpr UINT ID_TRANS_SCALE = 1013;
constexpr UINT ID_TRANS_ROTATE = 1014;
constexpr UINT ID_CLIP_LINE_CS = 1015;
constexpr UINT ID_CLIP_LINE_MID = 1016;
constexpr UINT ID_CLIP_POLY_SH = 1017;
constexpr UINT ID_CLIP_POLY_WA = 1018;

enum class DrawMode {
    None,
    DrawLineMidpoint,
    DrawLineBresenham,
    DrawCircleMidpoint,
    DrawCircleBresenham,
    DrawRectangle,
    DrawPolygon,
    DrawBSpline,
    FillScanline,
    FillFence,
    TransformTranslate,
    TransformScale,
    TransformRotate,
    ClipLineCS,
    ClipLineMid,
    ClipPolySH,
    ClipPolyWA
};

struct Point {
    int x;
    int y;
};

struct Shape {
    DrawMode type;
    std::vector<Point> vertices;
    COLORREF color;
    COLORREF fillColor;
    int fillMode;
};

void Initialize(HWND hwnd);
void Shutdown();
void Resize(HWND hwnd);
void HandleCommand(int commandId);
void HandleLButtonDown(int x, int y);
void HandleMouseMove(int x, int y);
void HandleMouseWheel(short delta);
void OnPaint(HWND hwnd);

} // namespace GraphicsEngine
