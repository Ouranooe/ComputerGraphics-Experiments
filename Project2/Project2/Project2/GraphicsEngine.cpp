#include "GraphicsEngine.h"
#include "GraphicsState.h"
#include "DrawingPrimitives.h"
#include "Shapes.h"
#include "Fill.h"
#include "Transform.h"
#include "Clip.h"

#include <windowsx.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

namespace GraphicsEngine {

// ===== Global state definitions =====
HWND g_hwnd = nullptr;
HDC g_hdcMem = nullptr;
HBITMAP g_hbmMem = nullptr;

DrawMode g_currentMode = DrawMode::None;
std::vector<Shape> g_shapes;
std::vector<Point> g_currentPoints;
bool g_isDrawing = false;

COLORREF g_drawColor = RGB(0, 0, 0);
COLORREF g_fillColor = RGB(253, 151, 47);

int g_selectedShapeIndex = -1;
Point g_firstClick{ 0, 0 };
double g_scaleBaseDist = 1.0;
double g_rotBaseAngle = 0.0;

// ===== Internal helper functions =====
void RecreateBackBuffer(HWND hwnd) {
    if (!hwnd) return;
    HDC hdc = GetDC(hwnd);
    if (!g_hdcMem) {
        g_hdcMem = CreateCompatibleDC(hdc);
    }
    RECT rc{};
    GetClientRect(hwnd, &rc);
    HBITMAP newBitmap = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(g_hdcMem, newBitmap));
    if (oldBitmap && oldBitmap != g_hbmMem) {
        DeleteObject(oldBitmap);
    }
    if (g_hbmMem && g_hbmMem != newBitmap) {
        DeleteObject(g_hbmMem);
    }
    g_hbmMem = newBitmap;
    ReleaseDC(hwnd, hdc);
}

static void FinishDrawing() {
    if (g_currentPoints.empty()) { g_isDrawing = false; return; }

    Shape s;
    s.type = g_currentMode;
    s.vertices = g_currentPoints;
    s.color = g_drawColor;
    s.fillColor = g_fillColor;
    s.fillMode = 0;

    if (s.type == DrawMode::DrawPolygon && s.vertices.size() < 3) {
        // ignore invalid polygon
    }
    else if (s.type == DrawMode::DrawBSpline && s.vertices.size() < 4) {
        // ignore insufficient control points
    }
    else {
        g_shapes.push_back(s);
    }

    g_currentPoints.clear();
    if (g_currentMode != DrawMode::DrawPolygon &&
        g_currentMode != DrawMode::DrawBSpline) {
        g_isDrawing = false;
    }
    if (g_hwnd)
        InvalidateRect(g_hwnd, NULL, FALSE);
}

static void ClearCanvas() {
    g_shapes.clear();
    g_currentPoints.clear();
    g_isDrawing = false;
    g_currentMode = DrawMode::None;
    g_selectedShapeIndex = -1;
    if (g_hwnd)
        InvalidateRect(g_hwnd, NULL, TRUE);
}

static void RedrawAllShapesOn(HDC hdc, const std::vector<Shape>& shapes) {
    for (const auto& s : shapes) {
        if (s.fillMode == 1)
            FillShapeScanline(hdc, s, s.fillColor);
        else if (s.fillMode == 2)
            FillShapeFence(hdc, s, s.fillColor);
        DrawShapeBorder(hdc, s);
    }
}

static void RedrawAllShapes(HDC hdc) {
    RedrawAllShapesOn(hdc, g_shapes);
}

// ===== Public API =====
void Initialize(HWND hwnd) {
    g_hwnd = hwnd;
    RecreateBackBuffer(hwnd);
}

void Shutdown() {
    if (g_hbmMem) {
        DeleteObject(g_hbmMem);
        g_hbmMem = nullptr;
    }
    if (g_hdcMem) {
        DeleteDC(g_hdcMem);
        g_hdcMem = nullptr;
    }
    g_hwnd = nullptr;
    g_shapes.clear();
    g_currentPoints.clear();
    g_isDrawing = false;
    g_selectedShapeIndex = -1;
}

void Resize(HWND hwnd) {
    g_hwnd = hwnd;
    RecreateBackBuffer(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

void HandleCommand(int commandId) {
    switch (commandId) {
    case ID_DRAW_LINE_MIDPOINT:
        g_currentMode = DrawMode::DrawLineMidpoint;   g_currentPoints.clear(); g_isDrawing = false; break;
    case ID_DRAW_LINE_BRESENHAM:
        g_currentMode = DrawMode::DrawLineBresenham;  g_currentPoints.clear(); g_isDrawing = false; break;
    case ID_DRAW_CIRCLE_MIDPOINT:
        g_currentMode = DrawMode::DrawCircleMidpoint; g_currentPoints.clear(); g_isDrawing = false; break;
    case ID_DRAW_CIRCLE_BRESENHAM:
        g_currentMode = DrawMode::DrawCircleBresenham;g_currentPoints.clear(); g_isDrawing = false; break;
    case ID_DRAW_RECTANGLE:
        g_currentMode = DrawMode::DrawRectangle;      g_currentPoints.clear(); g_isDrawing = false; break;
    case ID_DRAW_POLYGON:
        g_currentMode = DrawMode::DrawPolygon;        g_currentPoints.clear(); g_isDrawing = true;  break;
    case ID_DRAW_BSPLINE:
        g_currentMode = DrawMode::DrawBSpline;        g_currentPoints.clear(); g_isDrawing = true;  break;
    case ID_FILL_SCANLINE:
        g_currentMode = DrawMode::FillScanline;       break;
    case ID_FILL_FENCE:
        g_currentMode = DrawMode::FillFence;          break;
    case ID_TRANS_TRANSLATE:
        g_currentMode = DrawMode::TransformTranslate; g_isDrawing = false; g_selectedShapeIndex = -1; break;
    case ID_TRANS_SCALE:
        g_currentMode = DrawMode::TransformScale;     g_isDrawing = false; g_selectedShapeIndex = -1; break;
    case ID_TRANS_ROTATE:
        g_currentMode = DrawMode::TransformRotate;    g_isDrawing = false; g_selectedShapeIndex = -1; break;
    case ID_CLIP_LINE_CS:
        g_currentMode = DrawMode::ClipLineCS;         g_isDrawing = false; break;
    case ID_CLIP_LINE_MID:
        g_currentMode = DrawMode::ClipLineMid;        g_isDrawing = false; break;
    case ID_CLIP_POLY_SH:
        g_currentMode = DrawMode::ClipPolySH;         g_isDrawing = false; break;
    case ID_CLIP_POLY_WA:
        g_currentMode = DrawMode::ClipPolyWA;         g_isDrawing = false; break;
    case ID_EDIT_FINISH:
        FinishDrawing();                              break;
    case ID_EDIT_CLEAR:
        ClearCanvas();                                break;
    default:
        break;
    }
}

void HandleLButtonDown(int x, int y) {
    switch (g_currentMode) {
    case DrawMode::DrawLineMidpoint:
    case DrawMode::DrawLineBresenham:
    case DrawMode::DrawCircleMidpoint:
    case DrawMode::DrawCircleBresenham:
    case DrawMode::DrawRectangle:
        if (!g_isDrawing) {
            g_currentPoints.clear();
            g_currentPoints.push_back({ x, y });
            g_isDrawing = true;
        }
        else {
            g_currentPoints.push_back({ x, y });
            FinishDrawing();
        }
        break;

    case DrawMode::DrawPolygon:
    case DrawMode::DrawBSpline:
        g_currentPoints.push_back({ x, y });
        if (g_hwnd)
            InvalidateRect(g_hwnd, NULL, FALSE);
        break;

    case DrawMode::FillScanline:
    case DrawMode::FillFence: {
        for (auto& s : g_shapes) {
            if (PointInShape(s, x, y)) {
                s.fillColor = g_fillColor;
                if (g_currentMode == DrawMode::FillScanline) {
                    s.fillMode = 1;
                    if (g_hdcMem)
                        FillShapeScanline(g_hdcMem, s, s.fillColor);
                }
                else {
                    s.fillMode = 2;
                    if (g_hdcMem)
                        FillShapeFence(g_hdcMem, s, s.fillColor);
                }
                if (g_hwnd)
                    InvalidateRect(g_hwnd, NULL, FALSE);
                break;
            }
        }
    } break;

    case DrawMode::TransformTranslate: {
        if (!g_isDrawing) {
            int idx = HitTestShape(x, y);
            if (idx >= 0) {
                g_selectedShapeIndex = idx;
                g_firstClick = { x, y };
                g_isDrawing = true;
            }
        }
        else {
            int dx = x - g_firstClick.x;
            int dy = y - g_firstClick.y;
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size())
                TranslateShape(g_shapes[g_selectedShapeIndex], dx, dy);
            g_isDrawing = false;
            if (g_hwnd)
                InvalidateRect(g_hwnd, NULL, FALSE);
        }
    } break;

    case DrawMode::TransformScale: {
        if (!g_isDrawing) {
            int idx = HitTestShape(x, y);
            if (idx >= 0) {
                g_selectedShapeIndex = idx;
                g_firstClick = { x, y };
                if (!g_shapes[idx].vertices.empty()) {
                    Point v0 = g_shapes[idx].vertices[0];
                    double ddx = v0.x - g_firstClick.x;
                    double ddy = v0.y - g_firstClick.y;
                    g_scaleBaseDist = std::sqrt(ddx * ddx + ddy * ddy);
                    if (g_scaleBaseDist < 1) g_scaleBaseDist = 1;
                }
                g_isDrawing = true;
            }
        }
        else {
            double ddx = x - g_firstClick.x;
            double ddy = y - g_firstClick.y;
            double dist = std::sqrt(ddx * ddx + ddy * ddy);
            double s = dist / g_scaleBaseDist;
            if (s < 0.01) s = 0.01;
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size())
                ScaleShape(g_shapes[g_selectedShapeIndex], g_firstClick, s, s);
            g_isDrawing = false;
            if (g_hwnd)
                InvalidateRect(g_hwnd, NULL, FALSE);
        }
    } break;

    case DrawMode::TransformRotate: {
        if (!g_isDrawing) {
            int idx = HitTestShape(x, y);
            if (idx >= 0) {
                g_selectedShapeIndex = idx;
                g_firstClick = { x, y };
                if (!g_shapes[idx].vertices.empty()) {
                    Point v0 = g_shapes[idx].vertices[0];
                    double ddx = v0.x - g_firstClick.x;
                    double ddy = v0.y - g_firstClick.y;
                    g_rotBaseAngle = std::atan2(ddy, ddx);
                }
                g_isDrawing = true;
            }
        }
        else {
            double ddx = x - g_firstClick.x;
            double ddy = y - g_firstClick.y;
            double ang1 = std::atan2(ddy, ddx);
            double delta = ang1 - g_rotBaseAngle;
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size())
                RotateShape(g_shapes[g_selectedShapeIndex], g_firstClick, delta);
            g_isDrawing = false;
            if (g_hwnd)
                InvalidateRect(g_hwnd, NULL, FALSE);
        }
    } break;

    case DrawMode::ClipLineCS:
    case DrawMode::ClipLineMid:
    case DrawMode::ClipPolySH:
    case DrawMode::ClipPolyWA: {
        if (!g_isDrawing) {
            g_firstClick = { x, y };
            g_isDrawing = true;
        }
        else {
            Point p2{ x, y };
            g_isDrawing = false;
            RECT rc;
            rc.left = (std::min)(g_firstClick.x, p2.x);
            rc.right = (std::max)(g_firstClick.x, p2.x);
            rc.top = (std::min)(g_firstClick.y, p2.y);
            rc.bottom = (std::max)(g_firstClick.y, p2.y);

            if (g_currentMode == DrawMode::ClipLineCS)
                ClipAllLines_CohenSutherland(rc);
            else if (g_currentMode == DrawMode::ClipLineMid)
                ClipAllLines_Midpoint(rc);
            else if (g_currentMode == DrawMode::ClipPolySH)
                ClipAllPolygons_SH(rc);
            else if (g_currentMode == DrawMode::ClipPolyWA)
                ClipAllPolygons_WA(rc);

            if (g_hwnd)
                InvalidateRect(g_hwnd, NULL, FALSE);
        }
    } break;

    default: break;
    }
}

void HandleMouseMove(int x, int y) {
    if (!g_hwnd || !g_hdcMem) return;

    HDC hdc = GetDC(g_hwnd);
    RECT rc; GetClientRect(g_hwnd, &rc);

    FillRect(g_hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    std::vector<Shape> temp;
    const std::vector<Shape>* shapesToDraw = &g_shapes;

    if (g_isDrawing) {
        switch (g_currentMode) {
        case DrawMode::TransformTranslate:
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size()) {
                temp = g_shapes;
                int ddx = x - g_firstClick.x;
                int ddy = y - g_firstClick.y;
                TranslateShape(temp[g_selectedShapeIndex], ddx, ddy);
                shapesToDraw = &temp;
            }
            break;
        case DrawMode::TransformScale:
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size()) {
                temp = g_shapes;
                double ddx = x - g_firstClick.x;
                double ddy = y - g_firstClick.y;
                double dist = std::sqrt(ddx * ddx + ddy * ddy);
                double s = dist / g_scaleBaseDist;
                if (s < 0.01) s = 0.01;
                ScaleShape(temp[g_selectedShapeIndex], g_firstClick, s, s);
                shapesToDraw = &temp;
            }
            break;
        case DrawMode::TransformRotate:
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size()) {
                temp = g_shapes;
                double ddx = x - g_firstClick.x;
                double ddy = y - g_firstClick.y;
                double ang1 = std::atan2(ddy, ddx);
                double delta = ang1 - g_rotBaseAngle;
                RotateShape(temp[g_selectedShapeIndex], g_firstClick, delta);
                shapesToDraw = &temp;
            }
            break;
        default:
            break;
        }
    }

    RedrawAllShapesOn(g_hdcMem, *shapesToDraw);

    if (g_isDrawing && !g_currentPoints.empty()) {
        Point p0 = g_currentPoints[0];
        switch (g_currentMode) {
        case DrawMode::DrawLineMidpoint:
            DrawLineMidpoint(g_hdcMem, p0.x, p0.y, x, y, g_drawColor); break;
        case DrawMode::DrawLineBresenham:
            DrawLineBresenham(g_hdcMem, p0.x, p0.y, x, y, g_drawColor); break;
        case DrawMode::DrawPolygon: {
            if (g_currentPoints.size() > 1)
                DrawPolyline(g_hdcMem, g_currentPoints, g_drawColor, false);
            Point last = g_currentPoints.back();
            DrawLineMidpoint(g_hdcMem, last.x, last.y, x, y, g_drawColor);
        } break;
        case DrawMode::DrawBSpline: {
            for (auto& p : g_currentPoints)
                Ellipse(g_hdcMem, p.x - 3, p.y - 3, p.x + 3, p.y + 3);
            std::vector<Point> tempCtrl = g_currentPoints;
            tempCtrl.push_back({ x, y });
            if (tempCtrl.size() > 1)
                DrawPolyline(g_hdcMem, tempCtrl, RGB(200, 200, 200), false);
            if (tempCtrl.size() >= 4)
                DrawBSpline(g_hdcMem, tempCtrl, g_drawColor);
        } break;
        default:
            break;
        }
    }

    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, g_hdcMem, 0, 0, SRCCOPY);
    ReleaseDC(g_hwnd, hdc);
}

void HandleMouseWheel(short delta) {
    if (g_currentMode == DrawMode::TransformScale &&
        g_selectedShapeIndex >= 0 &&
        g_selectedShapeIndex < (int)g_shapes.size()) {
        double step = 0.1;
        double s = 1.0 + step * (delta / 120.0);
        if (s < 0.1) s = 0.1;
        ScaleShape(g_shapes[g_selectedShapeIndex], g_firstClick, s, s);
        if (g_hwnd)
            InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

void OnPaint(HWND hwnd) {
    if (!g_hdcMem) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return;
    }

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);

    FillRect(g_hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    RedrawAllShapes(g_hdcMem);

    if (g_currentMode == DrawMode::DrawPolygon && g_currentPoints.size() >= 2)
        DrawPolyline(g_hdcMem, g_currentPoints, g_drawColor, false);

    if (g_currentMode == DrawMode::DrawBSpline && !g_currentPoints.empty()) {
        for (auto& p : g_currentPoints)
            Ellipse(g_hdcMem, p.x - 3, p.y - 3, p.x + 3, p.y + 3);
        if (g_currentPoints.size() > 1)
            DrawPolyline(g_hdcMem, g_currentPoints, RGB(200, 200, 200), false);
        if (g_currentPoints.size() >= 4)
            DrawBSpline(g_hdcMem, g_currentPoints, g_drawColor);
    }

    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, g_hdcMem, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
}

} // namespace GraphicsEngine
