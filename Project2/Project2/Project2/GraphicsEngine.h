#pragma once

#include <windows.h>
#include <vector>
#include <gl/GL.h>
#include <gl/GLU.h>

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

// 3D Commands (matching Resource.h)
constexpr UINT ID_MODE_SWITCH = 2000;
constexpr UINT ID_3D_SPHERE = 2001;
constexpr UINT ID_3D_CUBE = 2002;
constexpr UINT ID_3D_CYLINDER = 2003;
constexpr UINT ID_3D_PLANE = 2004;
constexpr UINT ID_3D_LIGHT_SETTINGS = 2005;

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

// 3D Structures
struct Vector3 {
    float x, y, z;
};

struct Material {
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float shininess;
};

enum class ModelType {
    Sphere,
    Cube,
    Cylinder,
    Ground
};

struct Object3D {
    ModelType type;
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    Material material;
    bool selected;
    // Texture ID if needed
};

struct Camera {
    Vector3 position;
    Vector3 target;
    Vector3 up;
};

struct Light {
    Vector3 position;
    float ambient[4];
    float diffuse[4];
    float specular[4];
};

// Global State Access
extern bool is3DMode;
extern Object3D* selectedObject;
extern Light sceneLight;

void Initialize(HWND hwnd);
void Shutdown();
void Resize(HWND hwnd);
void HandleCommand(int commandId);
void HandleLButtonDown(int x, int y);
void HandleMouseMove(int x, int y);
void HandleMouseWheel(short delta);
void OnPaint(HWND hwnd);

// 3D Specific Functions
void InitGL(HWND hwnd);
void DrawScene(HDC hdc = nullptr);
void AddObject3D(ModelType type);
void SelectObject3D(int x, int y);
void UpdateObjectTransform(Object3D* obj, Vector3 pos, Vector3 rot, Vector3 scale);

// Dialog Procedures
INT_PTR CALLBACK TransformDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK LightDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK MaterialDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

} // namespace GraphicsEngine
