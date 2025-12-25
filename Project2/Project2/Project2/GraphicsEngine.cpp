#include "GraphicsEngine.h"
#include "GraphicsState.h"
#include "DrawingPrimitives.h"
#include "Shapes.h"
#include "Fill.h"
#include "Transform.h"
#include "Clip.h"
#include "resource.h"

#include <windowsx.h>
#include <commdlg.h>
#include <algorithm>
#include <cmath>
#include <gdiplus.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

namespace GraphicsEngine {

// ===== Global state definitions =====
HWND g_hwnd = nullptr;
HDC g_hdcMem = nullptr;
HBITMAP g_hbmMem = nullptr;
ULONG_PTR g_gdiplusToken;

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

Point g_currentMousePos{ 0, 0 };

// 3D Globals
bool is3DMode = false;
HGLRC g_hRC = nullptr;
std::vector<Object3D> g_objects;
Object3D* selectedObject = nullptr;
Camera g_camera = { {0, 5, 10}, {0, 0, 0}, {0, 1, 0} };
Light g_light = { {5, 10, 5}, {0.2f, 0.2f, 0.2f, 1.0f}, {0.8f, 0.8f, 0.8f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} };
Point g_lastMousePos = {0, 0};

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

static void RedrawAllShapesOn(HDC hdc, const std::vector<Shape>& shapes, int highlightIndex = -1) {
    for (size_t i = 0; i < shapes.size(); i++) {
        const auto& s = shapes[i];
        if (s.fillMode == 1)
            FillShapeScanline(hdc, s, s.fillColor);
        else if (s.fillMode == 2)
            FillShapeFence(hdc, s, s.fillColor);
        DrawShapeBorder(hdc, s);
        
        // 高亮显示选中的图形
        if ((int)i == highlightIndex && !s.vertices.empty()) {
            // 绘制选中边框（虚线）
            HPEN hPenHighlight = CreatePen(PS_DASH, 2, RGB(0, 120, 255));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPenHighlight);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            
            // 计算包围盒
            LONG minX = s.vertices[0].x, maxX = s.vertices[0].x;
            LONG minY = s.vertices[0].y, maxY = s.vertices[0].y;
            for (const auto& v : s.vertices) {
                if (v.x < minX) minX = v.x;
                if (v.x > maxX) maxX = v.x;
                if (v.y < minY) minY = v.y;
                if (v.y > maxY) maxY = v.y;
            }
            // 绘制包围盒
            Rectangle(hdc, minX - 5, minY - 5, maxX + 5, maxY + 5);
            
            // 绘制控制点
            HBRUSH hBrushPoint = CreateSolidBrush(RGB(0, 120, 255));
            for (const auto& v : s.vertices) {
                RECT rcPoint = { v.x - 4, v.y - 4, v.x + 4, v.y + 4 };
                FillRect(hdc, &rcPoint, hBrushPoint);
            }
            DeleteObject(hBrushPoint);
            
            SelectObject(hdc, hOldBrush);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPenHighlight);
        }
    }
}

static void RedrawAllShapes(HDC hdc) {
    RedrawAllShapesOn(hdc, g_shapes);
}

// ===== Public API =====
void Initialize(HWND hwnd) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    g_hwnd = hwnd;
    RecreateBackBuffer(hwnd);
    InitGL(hwnd);
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
    if (g_hRC) {
        wglDeleteContext(g_hRC);
        g_hRC = nullptr;
    }
    g_hwnd = nullptr;
    g_shapes.clear();
    g_currentPoints.clear();
    g_isDrawing = false;
    g_selectedShapeIndex = -1;

    GdiplusShutdown(g_gdiplusToken);
}

void Resize(HWND hwnd) {
    g_hwnd = hwnd;
    RecreateBackBuffer(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

void HandleCommand(int commandId) {
    if (commandId == ID_MODE_SWITCH) {
        is3DMode = !is3DMode;
        InvalidateRect(g_hwnd, NULL, TRUE);
        return;
    }

    if (is3DMode) {
        switch (commandId) {
        case ID_3D_SPHERE: AddObject3D(ModelType::Sphere); break;
        case ID_3D_CUBE: AddObject3D(ModelType::Cube); break;
        case ID_3D_CYLINDER: AddObject3D(ModelType::Cylinder); break;
        case ID_3D_PLANE: AddObject3D(ModelType::Ground); break;
        case ID_3D_LIGHT_SETTINGS: 
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_LIGHT_DIALOG), g_hwnd, LightDlgProc); 
            break;
        case ID_3D_EDIT_TRANSFORM:
            if (selectedObject)
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_TRANSFORM_DIALOG), g_hwnd, TransformDlgProc);
            else
                MessageBox(g_hwnd, L"\u8BF7\u5148\u9009\u62E9\u4E00\u4E2A\u7269\u4F53", L"\u63D0\u793A", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_3D_EDIT_MATERIAL:
            if (selectedObject)
                DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_MATERIAL_DIALOG), g_hwnd, MaterialDlgProc);
            else
                MessageBox(g_hwnd, L"\u8BF7\u5148\u9009\u62E9\u4E00\u4E2A\u7269\u4F53", L"\u63D0\u793A", MB_OK | MB_ICONINFORMATION);
            break;
        }
        return;
    }

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
    if (is3DMode) {
        SelectObject3D(x, y);
        g_lastMousePos = {x, y};
        return;
    }

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

// 绘制裁剪预览矩形
static void DrawClipPreviewRect(HDC hdc, const Point& p1, const Point& p2) {
    HPEN hPen = CreatePen(PS_DASH, 1, RGB(255, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    
    LONG left = (std::min)(p1.x, p2.x);
    LONG right = (std::max)(p1.x, p2.x);
    LONG top = (std::min)(p1.y, p2.y);
    LONG bottom = (std::max)(p1.y, p2.y);
    
    Rectangle(hdc, left, top, right, bottom);
    
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

void HandleMouseMove(int x, int y) {
    if (is3DMode) {
        if (GetKeyState(VK_LBUTTON) & 0x8000) {
            float dx = (float)(x - g_lastMousePos.x) * 0.05f;
            float dy = (float)(y - g_lastMousePos.y) * 0.05f;

            if (selectedObject) {
                // 恢复为世界坐标系 X-Y 平面移动 (红绿轴)
                selectedObject->position.x += dx;
                selectedObject->position.y -= dy;
            } else {
                float theta = -dx * 0.5f;
                float c = cos(theta);
                float s = sin(theta);
                float newX = g_camera.position.x * c - g_camera.position.z * s;
                float newZ = g_camera.position.x * s + g_camera.position.z * c;
                g_camera.position.x = newX;
                g_camera.position.z = newZ;
                g_camera.position.y += dy;
            }
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
        g_lastMousePos = {x, y};
        return;
    }

    if (!g_hwnd || !g_hdcMem) return;
    
    g_currentMousePos = { x, y };

    HDC hdc = GetDC(g_hwnd);
    RECT rc; GetClientRect(g_hwnd, &rc);

    FillRect(g_hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    std::vector<Shape> temp;
    const std::vector<Shape>* shapesToDraw = &g_shapes;
    int highlightIdx = -1;

    if (g_isDrawing) {
        switch (g_currentMode) {
        case DrawMode::TransformTranslate:
            if (g_selectedShapeIndex >= 0 && g_selectedShapeIndex < (int)g_shapes.size()) {
                temp = g_shapes;
                int ddx = x - g_firstClick.x;
                int ddy = y - g_firstClick.y;
                TranslateShape(temp[g_selectedShapeIndex], ddx, ddy);
                shapesToDraw = &temp;
                highlightIdx = g_selectedShapeIndex;
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
                highlightIdx = g_selectedShapeIndex;
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
                highlightIdx = g_selectedShapeIndex;
            }
            break;
        default:
            break;
        }
    }

    RedrawAllShapesOn(g_hdcMem, *shapesToDraw, highlightIdx);
    
    // 绘制裁剪预览矩形
    if (g_isDrawing && (g_currentMode == DrawMode::ClipLineCS ||
                        g_currentMode == DrawMode::ClipLineMid ||
                        g_currentMode == DrawMode::ClipPolySH ||
                        g_currentMode == DrawMode::ClipPolyWA)) {
        DrawClipPreviewRect(g_hdcMem, g_firstClick, { x, y });
    }

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
    if (is3DMode) {
        float d = (float)delta * 0.01f;
        if (selectedObject) {
            selectedObject->position.z += d;
        } else {
            g_camera.position.x *= (1.0f - d * 0.1f);
            g_camera.position.y *= (1.0f - d * 0.1f);
            g_camera.position.z *= (1.0f - d * 0.1f);
        }
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }

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
    if (is3DMode) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawScene(hdc);
        EndPaint(hwnd, &ps);
        return;
    }

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

//-------------三维实验相关代码----------------

// 初始化OpenGL上下文
void InitGL(HWND hwnd) {
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        24, 8, 0, PFD_MAIN_PLANE,
        0, 0, 0, 0
    };
    HDC hdc = GetDC(hwnd);
    int format = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, format, &pfd);
    g_hRC = wglCreateContext(hdc);
    ReleaseDC(hwnd, hdc);
}

GLuint LoadTexture(const wchar_t* filename) {
    Bitmap bitmap(filename);
    if (bitmap.GetLastStatus() != Ok) return 0;

    BitmapData bitmapData;
    Rect rect(0, 0, bitmap.GetWidth(), bitmap.GetHeight());

    // Lock the bits
    bitmap.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bitmapData);

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.GetWidth(), bitmap.GetHeight(), 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, bitmapData.Scan0);

    bitmap.UnlockBits(&bitmapData);
    return texID;
}

// 绘制三维场景
void DrawScene(HDC hdc) {
    if (!g_hRC) return;
    bool releaseDC = false;
    if (!hdc) {
        hdc = GetDC(g_hwnd);
        releaseDC = true;
    }
    wglMakeCurrent(hdc, g_hRC);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    RECT rc; GetClientRect(g_hwnd, &rc);
    gluPerspective(45.0, (double)(rc.right - rc.left) / (rc.bottom - rc.top), 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(g_camera.position.x, g_camera.position.y, g_camera.position.z,
              g_camera.target.x, g_camera.target.y, g_camera.target.z,
              g_camera.up.x, g_camera.up.y, g_camera.up.z);

    glLightfv(GL_LIGHT0, GL_POSITION, (float*)&g_light.position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, g_light.ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, g_light.diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, g_light.specular);

    //  绘制坐标轴
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);

    glBegin(GL_LINES);
    // 坐标轴 - X 轴 红色
    glColor4f(1.0f, 0.0f, 0.0f, 0.5f);
    glVertex3f(-100.0f, 0.0f, 0.0f);
    glVertex3f(100.0f, 0.0f, 0.0f);

    // Y轴 绿色
    glColor4f(0.0f, 1.0f, 0.0f, 0.5f);
    glVertex3f(0.0f, -100.0f, 0.0f);
    glVertex3f(0.0f, 100.0f, 0.0f);

    // Z轴 蓝色
    glColor4f(0.0f, 0.0f, 1.0f, 0.5f);
    glVertex3f(0.0f, 0.0f, -100.0f);
    glVertex3f(0.0f, 0.0f, 100.0f);
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    // 绘制三维对象
    for (auto& obj : g_objects) {
        glPushMatrix(); // 保存当前矩阵状态
        glTranslatef(obj.position.x, obj.position.y, obj.position.z); // 平移到对象位置
        glRotatef(obj.rotation.x, 1, 0, 0); // 旋转
        glRotatef(obj.rotation.y, 0, 1, 0);
        glRotatef(obj.rotation.z, 0, 0, 1);
        glScalef(obj.scale.x, obj.scale.y, obj.scale.z);   // 缩放

        glMaterialfv(GL_FRONT, GL_AMBIENT, obj.material.ambient);   // 设置材质属性
        glMaterialfv(GL_FRONT, GL_DIFFUSE, obj.material.diffuse);   // 设置材质属性
        glMaterialfv(GL_FRONT, GL_SPECULAR, obj.material.specular);
        glMaterialf(GL_FRONT, GL_SHININESS, obj.material.shininess);

        if (obj.selected) {
            float emission[] = {0.3f, 0.3f, 0.3f, 1.0f};
            glMaterialfv(GL_FRONT, GL_EMISSION, emission);
        } else {
            float emission[] = {0.0f, 0.0f, 0.0f, 1.0f};
            glMaterialfv(GL_FRONT, GL_EMISSION, emission);
        }

        if (obj.hasTexture && obj.textureID) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, obj.textureID);
            GLint wrap = (obj.textureWrapMode == 0) ? GL_REPEAT : GL_CLAMP;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        } else {
            glDisable(GL_TEXTURE_2D);
        }

        GLUquadric* quad = gluNewQuadric();
        if (obj.hasTexture && obj.textureID) {
            gluQuadricTexture(quad, GL_TRUE);
        }

        switch (obj.type) {
            case ModelType::Sphere: gluSphere(quad, 1.0, 32, 32); break;
            case ModelType::Cylinder: gluCylinder(quad, 1.0, 1.0, 2.0, 32, 1); break;
            case ModelType::Cube: {
                glBegin(GL_QUADS);
                // Front Face
                glNormal3f(0, 0, 1); glTexCoord2f(0.0f, 0.0f); glVertex3f(-1, -1, 1); glTexCoord2f(1.0f, 0.0f); glVertex3f(1, -1, 1); glTexCoord2f(1.0f, 1.0f); glVertex3f(1, 1, 1); glTexCoord2f(0.0f, 1.0f); glVertex3f(-1, 1, 1);
                // Back Face
                glNormal3f(0, 0, -1); glTexCoord2f(1.0f, 0.0f); glVertex3f(-1, -1, -1); glTexCoord2f(1.0f, 1.0f); glVertex3f(-1, 1, -1); glTexCoord2f(0.0f, 1.0f); glVertex3f(1, 1, -1); glTexCoord2f(0.0f, 0.0f); glVertex3f(1, -1, -1);
                // Top Face
                glNormal3f(0, 1, 0); glTexCoord2f(0.0f, 1.0f); glVertex3f(-1, 1, -1); glTexCoord2f(0.0f, 0.0f); glVertex3f(-1, 1, 1); glTexCoord2f(1.0f, 0.0f); glVertex3f(1, 1, 1); glTexCoord2f(1.0f, 1.0f); glVertex3f(1, 1, -1);
                // Bottom Face
                glNormal3f(0, -1, 0); glTexCoord2f(1.0f, 1.0f); glVertex3f(-1, -1, -1); glTexCoord2f(0.0f, 1.0f); glVertex3f(1, -1, -1); glTexCoord2f(0.0f, 0.0f); glVertex3f(1, -1, 1); glTexCoord2f(1.0f, 0.0f); glVertex3f(-1, -1, 1);
                // Right Face
                glNormal3f(1, 0, 0); glTexCoord2f(1.0f, 0.0f); glVertex3f(1, -1, -1); glTexCoord2f(1.0f, 1.0f); glVertex3f(1, 1, -1); glTexCoord2f(0.0f, 1.0f); glVertex3f(1, 1, 1); glTexCoord2f(0.0f, 0.0f); glVertex3f(1, -1, 1);
                // Left Face
                glNormal3f(-1, 0, 0); glTexCoord2f(0.0f, 0.0f); glVertex3f(-1, -1, -1); glTexCoord2f(1.0f, 0.0f); glVertex3f(-1, -1, 1); glTexCoord2f(1.0f, 1.0f); glVertex3f(-1, 1, 1); glTexCoord2f(0.0f, 1.0f); glVertex3f(-1, 1, -1);
                glEnd();
            } break;
            case ModelType::Ground: {
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(-5, 0, -5); 
                glTexCoord2f(0.0f, 1.0f); glVertex3f(-5, 0, 5); 
                glTexCoord2f(1.0f, 1.0f); glVertex3f(5, 0, 5); 
                glTexCoord2f(1.0f, 0.0f); glVertex3f(5, 0, -5);
                glEnd();
            } break;
        }
        gluDeleteQuadric(quad);
        glPopMatrix();
    }

    SwapBuffers(hdc);
    wglMakeCurrent(NULL, NULL);
    if (releaseDC) {
        ReleaseDC(g_hwnd, hdc);
    }
}

void AddObject3D(ModelType type) {
    Object3D obj;
    obj.type = type;
    obj.position = {0, 0, 0};
    obj.rotation = {0, 0, 0};
    obj.scale = {1, 1, 1};
    obj.material = {{0.2f, 0.2f, 0.2f, 1.0f}, {0.8f, 0.8f, 0.8f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 0.0f};
    obj.selected = false;
    g_objects.push_back(obj);
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

void SelectObject3D(int x, int y) {
    if (!g_hRC) return;
    HDC hdc = GetDC(g_hwnd);
    wglMakeCurrent(hdc, g_hRC);

    GLuint selectBuf[512];
    glSelectBuffer(512, selectBuf);
    glRenderMode(GL_SELECT);

    glInitNames();
    glPushName(0);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    RECT rc; GetClientRect(g_hwnd, &rc);
    GLint viewport[4] = {0, 0, rc.right - rc.left, rc.bottom - rc.top};
    gluPickMatrix(x, viewport[3] - y, 5, 5, viewport);
    gluPerspective(45.0, (double)(rc.right - rc.left) / (rc.bottom - rc.top), 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(g_camera.position.x, g_camera.position.y, g_camera.position.z,
              g_camera.target.x, g_camera.target.y, g_camera.target.z,
              g_camera.up.x, g_camera.up.y, g_camera.up.z);

    for (size_t i = 0; i < g_objects.size(); ++i) {
        glLoadName((GLuint)i);
        glPushMatrix();
        glTranslatef(g_objects[i].position.x, g_objects[i].position.y, g_objects[i].position.z);
        glRotatef(g_objects[i].rotation.x, 1, 0, 0);
        glRotatef(g_objects[i].rotation.y, 0, 1, 0);
        glRotatef(g_objects[i].rotation.z, 0, 0, 1);
        glScalef(g_objects[i].scale.x, g_objects[i].scale.y, g_objects[i].scale.z);
        
        GLUquadric* quad = gluNewQuadric();
        switch (g_objects[i].type) {
            case ModelType::Sphere: gluSphere(quad, 1.0, 8, 8); break;
            case ModelType::Cylinder: gluCylinder(quad, 1.0, 1.0, 2.0, 8, 1); break;
            case ModelType::Cube: gluSphere(quad, 1.5, 8, 8); break;
            case ModelType::Ground: gluSphere(quad, 5.0, 8, 8); break;
        }
        gluDeleteQuadric(quad);
        glPopMatrix();
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glFlush();

    GLint hits = glRenderMode(GL_RENDER);
    
    if (selectedObject) selectedObject->selected = false;
    selectedObject = nullptr;

    if (hits > 0) {
        GLuint* ptr = (GLuint*)selectBuf;
        GLuint minZ = 0xffffffff;
        int index = -1;
        for (int i = 0; i < hits; i++) {
            GLuint names = *ptr; ptr++;
            GLuint z1 = *ptr; ptr++;
            GLuint z2 = *ptr; ptr++;
            if (z1 < minZ) {
                minZ = z1;
                index = *ptr;
            }
            ptr += names;
        }
        if (index >= 0 && index < g_objects.size()) {
            selectedObject = &g_objects[index];
            selectedObject->selected = true;
        }
    }

    wglMakeCurrent(NULL, NULL);
    ReleaseDC(g_hwnd, hdc);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

// Dialog Procedures
INT_PTR CALLBACK TransformDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    using namespace GraphicsEngine;
    switch (message) {
    case WM_INITDIALOG:
        if (selectedObject) {
            SetDlgItemInt(hDlg, IDC_EDIT_POS_X, (int)selectedObject->position.x, TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_POS_Y, (int)selectedObject->position.y, TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_POS_Z, (int)selectedObject->position.z, TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_ROT_X, (int)selectedObject->rotation.x, TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_ROT_Y, (int)selectedObject->rotation.y, TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_ROT_Z, (int)selectedObject->rotation.z, TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_SCALE_X, (int)(selectedObject->scale.x * 100), TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_SCALE_Y, (int)(selectedObject->scale.y * 100), TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_SCALE_Z, (int)(selectedObject->scale.z * 100), TRUE);
        }
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            if (selectedObject) {
                selectedObject->position.x = (float)GetDlgItemInt(hDlg, IDC_EDIT_POS_X, NULL, TRUE);
                selectedObject->position.y = (float)GetDlgItemInt(hDlg, IDC_EDIT_POS_Y, NULL, TRUE);
                selectedObject->position.z = (float)GetDlgItemInt(hDlg, IDC_EDIT_POS_Z, NULL, TRUE);
                selectedObject->rotation.x = (float)GetDlgItemInt(hDlg, IDC_EDIT_ROT_X, NULL, TRUE);
                selectedObject->rotation.y = (float)GetDlgItemInt(hDlg, IDC_EDIT_ROT_Y, NULL, TRUE);
                selectedObject->rotation.z = (float)GetDlgItemInt(hDlg, IDC_EDIT_ROT_Z, NULL, TRUE);
                selectedObject->scale.x = (float)GetDlgItemInt(hDlg, IDC_EDIT_SCALE_X, NULL, TRUE) / 100.0f;
                selectedObject->scale.y = (float)GetDlgItemInt(hDlg, IDC_EDIT_SCALE_Y, NULL, TRUE) / 100.0f;
                selectedObject->scale.z = (float)GetDlgItemInt(hDlg, IDC_EDIT_SCALE_Z, NULL, TRUE) / 100.0f;
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        } else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK LightDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    using namespace GraphicsEngine;
    switch (message) {
    case WM_INITDIALOG:
        SetDlgItemInt(hDlg, IDC_EDIT_LIGHT_X, (int)g_light.position.x, TRUE);
        SetDlgItemInt(hDlg, IDC_EDIT_LIGHT_Y, (int)g_light.position.y, TRUE);
        SetDlgItemInt(hDlg, IDC_EDIT_LIGHT_Z, (int)g_light.position.z, TRUE);
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            g_light.position.x = (float)GetDlgItemInt(hDlg, IDC_EDIT_LIGHT_X, NULL, TRUE);
            g_light.position.y = (float)GetDlgItemInt(hDlg, IDC_EDIT_LIGHT_Y, NULL, TRUE);
            g_light.position.z = (float)GetDlgItemInt(hDlg, IDC_EDIT_LIGHT_Z, NULL, TRUE);
            InvalidateRect(g_hwnd, NULL, FALSE);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        } else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK MaterialDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    using namespace GraphicsEngine;
    static wchar_t tempTexturePath[260];

    switch (message) {
    case WM_INITDIALOG:
        if (selectedObject) {
            SetDlgItemInt(hDlg, IDC_EDIT_MAT_AMBIENT, (int)(selectedObject->material.ambient[0] * 100), TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_MAT_DIFFUSE, (int)(selectedObject->material.diffuse[0] * 100), TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_MAT_SPECULAR, (int)(selectedObject->material.specular[0] * 100), TRUE);
            SetDlgItemInt(hDlg, IDC_EDIT_MAT_SHININESS, (int)selectedObject->material.shininess, TRUE);

            // Texture Init
            CheckDlgButton(hDlg, IDC_CHECK_TEXTURE, selectedObject->hasTexture ? BST_CHECKED : BST_UNCHECKED);
            wcscpy_s(tempTexturePath, selectedObject->texturePath);
            SetDlgItemTextW(hDlg, IDC_STATIC_TEXTURE_PATH, tempTexturePath[0] ? tempTexturePath : L"\u65E0");
            
            HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_TEXTURE_WRAP);
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u91CD\u590D (Repeat)");
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"\u622A\u65AD (Clamp)");
            SendMessage(hCombo, CB_SETCURSEL, selectedObject->textureWrapMode, 0);
        }
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_LOAD_TEXTURE) {
            OPENFILENAMEW ofn = {0};
            wchar_t szFile[260] = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hDlg;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"Image Files\0*.bmp;*.jpg;*.jpeg;*.png\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameW(&ofn) == TRUE) {
                wcscpy_s(tempTexturePath, szFile);
                SetDlgItemTextW(hDlg, IDC_STATIC_TEXTURE_PATH, tempTexturePath);
                CheckDlgButton(hDlg, IDC_CHECK_TEXTURE, BST_CHECKED);
            }
        }
        else if (LOWORD(wParam) == IDOK) {
            if (selectedObject) {
                float a = (float)GetDlgItemInt(hDlg, IDC_EDIT_MAT_AMBIENT, NULL, TRUE) / 100.0f;
                float d = (float)GetDlgItemInt(hDlg, IDC_EDIT_MAT_DIFFUSE, NULL, TRUE) / 100.0f;
                float s = (float)GetDlgItemInt(hDlg, IDC_EDIT_MAT_SPECULAR, NULL, TRUE) / 100.0f;
                selectedObject->material.ambient[0] = selectedObject->material.ambient[1] = selectedObject->material.ambient[2] = a;
                selectedObject->material.diffuse[0] = selectedObject->material.diffuse[1] = selectedObject->material.diffuse[2] = d;
                selectedObject->material.specular[0] = selectedObject->material.specular[1] = selectedObject->material.specular[2] = s;
                selectedObject->material.shininess = (float)GetDlgItemInt(hDlg, IDC_EDIT_MAT_SHININESS, NULL, TRUE);

                // Texture Update
                bool enable = IsDlgButtonChecked(hDlg, IDC_CHECK_TEXTURE) == BST_CHECKED;
                selectedObject->hasTexture = enable;
                
                HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_TEXTURE_WRAP);
                selectedObject->textureWrapMode = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);

                if (enable && wcscmp(selectedObject->texturePath, tempTexturePath) != 0) {
                    wcscpy_s(selectedObject->texturePath, tempTexturePath);
                    // Load Texture
                    if (selectedObject->textureID) glDeleteTextures(1, &selectedObject->textureID);
                    
                    HDC hdc = GetDC(g_hwnd);
                    wglMakeCurrent(hdc, g_hRC);
                    selectedObject->textureID = LoadTexture(selectedObject->texturePath);
                    wglMakeCurrent(NULL, NULL);
                    ReleaseDC(g_hwnd, hdc);
                }

                InvalidateRect(g_hwnd, NULL, FALSE);
            }
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        } else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

} // namespace GraphicsEngine
