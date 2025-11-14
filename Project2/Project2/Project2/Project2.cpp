/********************************************************************************
 *
 *                      计算机图形学综合实验平台
 *
 *   功能：
 *   1.  基础绘图算法:
 *       - 直线: 中点法, Bresenham算法
 *       - 圆: 中点圆算法, Bresenham圆算法
 *   2.  几何图形绘制:
 *       - 矩形 (GDI), 任意多边形, 3次B样条曲线
 *   3.  填充算法:
 *       - 扫描线填充 (边表与活动边表)
 *       - 栅栏填充 (种子填充)
 *   4.  交互设计:
 *       - 菜单驱动模式选择
 *       - 鼠标实时交互与动态预览
 *       - 双缓冲技术防闪烁
 *
 *   编译环境: Visual Studio (Windows Desktop Application)
 *   作者: Gemini AI Assistant
 *   日期: 2025-11-14
 *
 ********************************************************************************/

#include <windows.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stack>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

 // --- 1. 常量与宏定义 ---

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// 菜单ID定义
#define ID_DRAW_LINE_MIDPOINT 1001
#define ID_DRAW_LINE_BRESENHAM 1002
#define ID_DRAW_CIRCLE_MIDPOINT 1003
#define ID_DRAW_CIRCLE_BRESENHAM 1004
#define ID_DRAW_RECTANGLE 1005
#define ID_DRAW_POLYGON 1006
#define ID_DRAW_BSPLINE 1007
#define ID_FILL_SCANLINE 1008
#define ID_FILL_FENCE 1009
#define ID_EDIT_FINISH 1010
#define ID_EDIT_CLEAR 1011

 // --- 2. 数据结构设计 ---

// 绘图模式枚举
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
	FillFence
};

// 坐标点结构
struct Point {
	int x, y;
};

// 图形对象结构
struct Shape {
	DrawMode type;          // 图形类型
	std::vector<Point> vertices; // 顶点列表
	COLORREF color;         // 颜色
	bool filled;            // 是否填充
	COLORREF fillColor;     // 填充颜色
};

// B样条基函数
void B_Spline_Base_Function(float t, float* b) {
	float t2 = t * t;
	float t3 = t2 * t;
	b[0] = (-t3 + 3 * t2 - 3 * t + 1) / 6.0f;
	b[1] = (3 * t3 - 6 * t2 + 4) / 6.0f;
	b[2] = (-3 * t3 + 3 * t2 + 3 * t + 1) / 6.0f;
	b[3] = t3 / 6.0f;
}

// 扫描线填充算法所需数据结构
// 边结构
struct Edge {
	int ymax;      // 边的最大y值
	float x;       // 边与当前扫描线的交点x坐标
	float dx;      // 斜率的倒数, 1/k
	Edge* next;    // 指向下一条边的指针
};


// --- 3. 全局变量 ---

HWND g_hwnd;                       // 主窗口句柄
HDC  g_hdcMem;                     // 后备缓冲DC
HBITMAP g_hbmMem;                  // 后备缓冲位图
HBRUSH g_hBrush;                   // 画刷
HPEN g_hPen;                       // 画笔

DrawMode g_currentMode = DrawMode::None; // 当前绘图模式
std::vector<Shape> g_shapes;         // 存储所有已绘制的图形
std::vector<Point> g_currentPoints; // 存储当前正在绘制的图形的顶点
bool g_isDrawing = false;           // 标记是否正在绘制

COLORREF g_drawColor = RGB(255, 0, 0); // 当前绘制颜色 (红色)
COLORREF g_fillColor = RGB(0, 255, 0); // 当前填充颜色 (绿色)


// --- 4. 函数声明 ---

// 窗口过程函数
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// 绘图算法
void DrawPixel(HDC hdc, int x, int y, COLORREF color);
void DrawLineMidpoint(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color);
void DrawLineBresenham(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color);
void DrawCircleMidpoint(HDC hdc, int xc, int yc, int r, COLORREF color);
void DrawCircleBresenham(HDC hdc, int xc, int yc, int r, COLORREF color);
void DrawPolygon(HDC hdc, const std::vector<Point>& vertices, COLORREF color);
void DrawBSpline(HDC hdc, const std::vector<Point>& controlPoints, COLORREF color);

// 填充算法
void ScanlineFill(HDC hdc, const std::vector<Point>& vertices, COLORREF color);
void FenceFill(HDC hdc, int x, int y, COLORREF fillColor, COLORREF boundaryColor);

// 渲染与交互
void CreateMenuSystem(HWND hwnd);
void OnPaint(HWND hwnd);
void OnLButtonDown(int x, int y);
void OnMouseMove(int x, int y);
void ClearCanvas();
void FinishDrawing();
void RedrawAllShapes(HDC hdc);


// --- 5. WinMain 主函数 ---

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"ComputerGraphicsLab";
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	RegisterClass(&wc);

	g_hwnd = CreateWindowEx(
		0,
		L"ComputerGraphicsLab",
		L"计算机图形学实验平台 by Gemini",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
		NULL, NULL, hInstance, NULL
	);

	if (g_hwnd == NULL) {
		return 0;
	}

	CreateMenuSystem(g_hwnd);
	ShowWindow(g_hwnd, nCmdShow);
	UpdateWindow(g_hwnd);

	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}


// --- 6. 窗口过程 (消息处理) ---

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CREATE:
	{
		// 初始化双缓冲
		HDC hdc = GetDC(hwnd);
		g_hdcMem = CreateCompatibleDC(hdc);
		RECT rc;
		GetClientRect(hwnd, &rc);
		g_hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
		SelectObject(g_hdcMem, g_hbmMem);
		ReleaseDC(hwnd, hdc);
	}
	break;

	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// 解析菜单选择
		switch (wmId) {
		case ID_DRAW_LINE_MIDPOINT:
			g_currentMode = DrawMode::DrawLineMidpoint;
			g_currentPoints.clear();
			g_isDrawing = false;
			break;
		case ID_DRAW_LINE_BRESENHAM:
			g_currentMode = DrawMode::DrawLineBresenham;
			g_currentPoints.clear();
			g_isDrawing = false;
			break;
		case ID_DRAW_CIRCLE_MIDPOINT:
			g_currentMode = DrawMode::DrawCircleMidpoint;
			g_currentPoints.clear();
			g_isDrawing = false;
			break;
		case ID_DRAW_CIRCLE_BRESENHAM:
			g_currentMode = DrawMode::DrawCircleBresenham;
			g_currentPoints.clear();
			g_isDrawing = false;
			break;
		case ID_DRAW_RECTANGLE:
			g_currentMode = DrawMode::DrawRectangle;
			g_currentPoints.clear();
			g_isDrawing = false;
			break;
		case ID_DRAW_POLYGON:
			g_currentMode = DrawMode::DrawPolygon;
			g_currentPoints.clear();
			g_isDrawing = true; // 多边形需要持续添加点
			break;
		case ID_DRAW_BSPLINE:
			g_currentMode = DrawMode::DrawBSpline;
			g_currentPoints.clear();
			g_isDrawing = true; // B样条需要持续添加点
			break;
		case ID_FILL_SCANLINE:
			g_currentMode = DrawMode::FillScanline;
			break;
		case ID_FILL_FENCE:
			g_currentMode = DrawMode::FillFence;
			break;
		case ID_EDIT_FINISH:
			FinishDrawing();
			break;
		case ID_EDIT_CLEAR:
			ClearCanvas();
			break;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}
	break;

	case WM_LBUTTONDOWN:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		OnLButtonDown(x, y);
	}
	break;

	case WM_MOUSEMOVE:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		OnMouseMove(x, y);
	}
	break;

	case WM_SIZE:
	{
		// 窗口大小改变时重建后备缓冲
		if (g_hdcMem) {
			DeleteObject(g_hbmMem);
			HDC hdc = GetDC(hwnd);
			RECT rc;
			GetClientRect(hwnd, &rc);
			g_hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
			SelectObject(g_hdcMem, g_hbmMem);
			ReleaseDC(hwnd, hdc);
			InvalidateRect(hwnd, NULL, FALSE); // 请求重绘
		}
	}
	break;

	case WM_PAINT:
	{
		OnPaint(hwnd);
	}
	break;

	case WM_ERASEBKGND:
		return 1; // 阻止背景擦除，防止闪烁

	case WM_DESTROY:
	{
		DeleteObject(g_hbmMem);
		DeleteDC(g_hdcMem);
		PostQuitMessage(0);
	}
	break;

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}


// --- 7. 绘图算法模块 ---

// 绘制一个像素点
void DrawPixel(HDC hdc, int x, int y, COLORREF color) {
	SetPixel(hdc, x, y, color);
}

// 中点法直线算法
void DrawLineMidpoint(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
	int dx = abs(x2 - x1);
	int dy = abs(y2 - y1);
	int incX = (x2 > x1) ? 1 : -1;
	int incY = (y2 > y1) ? 1 : -1;

	int x = x1;
	int y = y1;

	DrawPixel(hdc, x, y, color);

	if (dx > dy) { // 斜率 |k| < 1
		int d = 2 * dy - dx;
		for (int i = 0; i < dx; i++) {
			x += incX;
			if (d < 0) {
				d += 2 * dy;
			}
			else {
				y += incY;
				d += 2 * (dy - dx);
			}
			DrawPixel(hdc, x, y, color);
		}
	}
	else { // 斜率 |k| >= 1
		int d = 2 * dx - dy;
		for (int i = 0; i < dy; i++) {
			y += incY;
			if (d < 0) {
				d += 2 * dx;
			}
			else {
				x += incX;
				d += 2 * (dx - dy);
			}
			DrawPixel(hdc, x, y, color);
		}
	}
}

// Bresenham直线算法
void DrawLineBresenham(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
	int dx = abs(x2 - x1);
	int dy = abs(y2 - y1);
	int incX = (x2 > x1) ? 1 : -1;
	int incY = (y2 > y1) ? 1 : -1;

	int x = x1, y = y1;
	DrawPixel(hdc, x, y, color);

	if (dx > dy) { // |k| < 1
		int e = -dx;
		for (int i = 0; i < dx; i++) {
			x += incX;
			e += 2 * dy;
			if (e >= 0) {
				y += incY;
				e -= 2 * dx;
			}
			DrawPixel(hdc, x, y, color);
		}
	}
	else { // |k| >= 1
		int e = -dy;
		for (int i = 0; i < dy; i++) {
			y += incY;
			e += 2 * dx;
			if (e >= 0) {
				x += incX;
				e -= 2 * dy;
			}
			DrawPixel(hdc, x, y, color);
		}
	}
}

// 绘制8个对称点
void DrawCirclePoints(HDC hdc, int xc, int yc, int x, int y, COLORREF color) {
	DrawPixel(hdc, xc + x, yc + y, color);
	DrawPixel(hdc, xc - x, yc + y, color);
	DrawPixel(hdc, xc + x, yc - y, color);
	DrawPixel(hdc, xc - x, yc - y, color);
	DrawPixel(hdc, xc + y, yc + x, color);
	DrawPixel(hdc, xc - y, yc + x, color);
	DrawPixel(hdc, xc + y, yc - x, color);
	DrawPixel(hdc, xc - y, yc - x, color);
}

// 中点圆算法
void DrawCircleMidpoint(HDC hdc, int xc, int yc, int r, COLORREF color) {
	if (r <= 0) return;
	int x = 0, y = r;
	int d = 1 - r; // 初始决策参数 (5/4 - r，浮点数优化为整数)

	DrawCirclePoints(hdc, xc, yc, x, y, color);

	while (x < y) {
		x++;
		if (d < 0) {
			d += 2 * x + 1;
		}
		else {
			y--;
			d += 2 * (x - y) + 1;
		}
		DrawCirclePoints(hdc, xc, yc, x, y, color);
	}
}

// Bresenham圆算法
void DrawCircleBresenham(HDC hdc, int xc, int yc, int r, COLORREF color) {
	if (r <= 0) return;
	int x = 0, y = r;
	int e = 3 - 2 * r; // 初始误差

	DrawCirclePoints(hdc, xc, yc, x, y, color);

	while (x < y) {
		if (e < 0) {
			e = e + 4 * x + 6;
		}
		else {
			e = e + 4 * (x - y) + 10;
			y--;
		}
		x++;
		DrawCirclePoints(hdc, xc, yc, x, y, color);
	}
}

// 多边形绘制
void DrawPolygon(HDC hdc, const std::vector<Point>& vertices, COLORREF color) {
	if (vertices.size() < 2) return;
	for (size_t i = 0; i < vertices.size() - 1; ++i) {
		DrawLineMidpoint(hdc, vertices[i].x, vertices[i].y, vertices[i + 1].x, vertices[i + 1].y, color);
	}
	// 封闭多边形
	DrawLineMidpoint(hdc, vertices.back().x, vertices.back().y, vertices.front().x, vertices.front().y, color);
}

// B样条曲线绘制
void DrawBSpline(HDC hdc, const std::vector<Point>& controlPoints, COLORREF color) {
	if (controlPoints.size() < 4) return;

	for (size_t i = 0; i <= controlPoints.size() - 4; ++i) {
		Point p1 = controlPoints[i];
		Point p2 = controlPoints[i + 1];
		Point p3 = controlPoints[i + 2];
		Point p4 = controlPoints[i + 3];

		Point lastPoint = p2;
		for (float t = 0.01f; t <= 1.0f; t += 0.01f) {
			float b[4];
			B_Spline_Base_Function(t, b);
			Point newPoint;
			newPoint.x = (int)(b[0] * p1.x + b[1] * p2.x + b[2] * p3.x + b[3] * p4.x);
			newPoint.y = (int)(b[0] * p1.y + b[1] * p2.y + b[2] * p3.y + b[3] * p4.y);
			DrawLineMidpoint(hdc, lastPoint.x, lastPoint.y, newPoint.x, newPoint.y, color);
			lastPoint = newPoint;
		}
	}
}

// 扫描线填充
void ScanlineFill(HDC hdc, const std::vector<Point>& vertices, COLORREF color) {
	if (vertices.size() < 3) return;

	// 1. 找出Y的最小值和最大值
	int ymin = vertices[0].y;
	int ymax = vertices[0].y;
	for (const auto& v : vertices) {
		if (v.y < ymin) ymin = v.y;
		if (v.y > ymax) ymax = v.y;
	}

	// 2. 初始化边表 (ET)
	std::vector<Edge*> ET(ymax - ymin + 1, nullptr);

	// 3. 建立边表
	for (size_t i = 0; i < vertices.size(); ++i) {
		Point p1 = vertices[i];
		Point p2 = vertices[(i + 1) % vertices.size()];

		if (p1.y == p2.y) continue; // 忽略水平边

		int y_start = min(p1.y, p2.y);
		int y_end = max(p1.y, p2.y);
		float x_start = (y_start == p1.y) ? p1.x : p2.x;
		float dx = (float)(p2.x - p1.x) / (p2.y - p1.y);

		Edge* newEdge = new Edge{ y_end, x_start, dx, nullptr };

		// 插入边表
		int bucket = y_start - ymin;
		if (!ET[bucket]) {
			ET[bucket] = newEdge;
		}
		else {
			Edge* current = ET[bucket];
			// 插入排序，按x升序
			if (newEdge->x < current->x) {
				newEdge->next = current;
				ET[bucket] = newEdge;
			}
			else {
				while (current->next && newEdge->x > current->next->x) {
					current = current->next;
				}
				newEdge->next = current->next;
				current->next = newEdge;
			}
		}
	}

	// 4. 初始化活动边表 (AET)
	Edge* AET = nullptr;

	// 5. 逐行扫描填充
	for (int y = ymin; y <= ymax; y++) {
		// 从ET中取出新边并入AET
		int bucket = y - ymin;
		if (ET[bucket]) {
			Edge* et_current = ET[bucket];
			while (et_current) {
				Edge* next_et = et_current->next;
				// 将边插入AET，保持x有序
				if (!AET || et_current->x < AET->x) {
					et_current->next = AET;
					AET = et_current;
				}
				else {
					Edge* aet_current = AET;
					while (aet_current->next && et_current->x > aet_current->next->x) {
						aet_current = aet_current->next;
					}
					et_current->next = aet_current->next;
					aet_current->next = et_current;
				}
				et_current = next_et;
			}
		}

		// 填充AET中的区间
		if (AET) {
			Edge* current = AET;
			while (current && current->next) {
				for (int x = (int)ceil(current->x); x < (int)current->next->x; x++) {
					DrawPixel(hdc, x, y, color);
				}
				current = current->next->next;
			}
		}

		// 更新AET
		Edge* prev = nullptr;
		Edge* current = AET;
		while (current) {
			// 删除ymax == y的边
			if (current->ymax == y) {
				if (prev) {
					prev->next = current->next;
					delete current;
					current = prev->next;
				}
				else {
					AET = current->next;
					delete current;
					current = AET;
				}
			}
			else {
				// 更新x坐标
				current->x += current->dx;
				prev = current;
				current = current->next;
			}
		}

		// 对AET重新排序
		if (AET) {
			Edge* head = AET;
			AET = nullptr;
			while (head) {
				Edge* temp = head;
				head = head->next;
				if (!AET || temp->x < AET->x) {
					temp->next = AET;
					AET = temp;
				}
				else {
					Edge* p = AET;
					while (p->next && temp->x > p->next->x) {
						p = p->next;
					}
					temp->next = p->next;
					p->next = temp;
				}
			}
		}
	}
}


// 栅栏填充 (基于栈的扫描线种子填充)
void FenceFill(HDC hdc, int x, int y, COLORREF fillColor, COLORREF boundaryColor) {
	COLORREF current_color = GetPixel(hdc, x, y);
	if (current_color == boundaryColor || current_color == fillColor) {
		return;
	}

	std::stack<Point> s;
	s.push({ x, y });

	while (!s.empty()) {
		Point p = s.top();
		s.pop();

		int curX = p.x;
		int curY = p.y;

		// 向右填充
		while (curX < WINDOW_WIDTH && GetPixel(hdc, curX, curY) != boundaryColor) {
			DrawPixel(hdc, curX, curY, fillColor);
			curX++;
		}
		int xRight = curX - 1;

		curX = p.x - 1;
		// 向左填充
		while (curX >= 0 && GetPixel(hdc, curX, curY) != boundaryColor) {
			DrawPixel(hdc, curX, curY, fillColor);
			curX--;
		}
		int xLeft = curX + 1;

		// 检查上一行
		curY = p.y + 1;
		if (curY < WINDOW_HEIGHT) {
			curX = xLeft;
			while (curX <= xRight) {
				bool needs_fill = false;
				while (curX <= xRight && GetPixel(hdc, curX, curY) != boundaryColor && GetPixel(hdc, curX, curY) != fillColor) {
					needs_fill = true;
					curX++;
				}
				if (needs_fill) {
					s.push({ curX - 1, curY });
				}
				// 跳过已填充或边界部分
				while (curX <= xRight && (GetPixel(hdc, curX, curY) == boundaryColor || GetPixel(hdc, curX, curY) == fillColor)) {
					curX++;
				}
			}
		}

		// 检查下一行
		curY = p.y - 1;
		if (curY >= 0) {
			curX = xLeft;
			while (curX <= xRight) {
				bool needs_fill = false;
				while (curX <= xRight && GetPixel(hdc, curX, curY) != boundaryColor && GetPixel(hdc, curX, curY) != fillColor) {
					needs_fill = true;
					curX++;
				}
				if (needs_fill) {
					s.push({ curX - 1, curY });
				}
				// 跳过已填充或边界部分
				while (curX <= xRight && (GetPixel(hdc, curX, curY) == boundaryColor || GetPixel(hdc, curX, curY) == fillColor)) {
					curX++;
				}
			}
		}
	}
}


// --- 8. 交互与渲染模块 ---

// 创建菜单
void CreateMenuSystem(HWND hwnd) {
	HMENU hMenu = CreateMenu();
	HMENU hDrawMenu = CreateMenu();
	HMENU hFillMenu = CreateMenu();
	HMENU hEditMenu = CreateMenu();

	// 绘图菜单
	AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_LINE_MIDPOINT, L"直线 (中点法)");
	AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_LINE_BRESENHAM, L"直线 (Bresenham)");
	AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_CIRCLE_MIDPOINT, L"圆 (中点法)");
	AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_CIRCLE_BRESENHAM, L"圆 (Bresenham)");
	AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_RECTANGLE, L"矩形");
	AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_POLYGON, L"多边形");
	AppendMenu(hDrawMenu, MF_STRING, ID_DRAW_BSPLINE, L"B样条曲线");
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hDrawMenu, L"绘图");

	// 填充菜单
	AppendMenu(hFillMenu, MF_STRING, ID_FILL_SCANLINE, L"扫描线填充");
	AppendMenu(hFillMenu, MF_STRING, ID_FILL_FENCE, L"栅栏填充");
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFillMenu, L"填充");

	// 编辑菜单
	AppendMenu(hEditMenu, MF_STRING, ID_EDIT_FINISH, L"完成绘制");
	AppendMenu(hEditMenu, MF_STRING, ID_EDIT_CLEAR, L"清空画布");
	AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, L"编辑");

	SetMenu(hwnd, hMenu);
}

// 绘制事件
void OnPaint(HWND hwnd) {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);

	RECT rc;
	GetClientRect(hwnd, &rc);

	// 1. 在后备缓冲上绘制
	// 清空后备缓冲为白色
	FillRect(g_hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
	// 重绘所有已保存的图形
	RedrawAllShapes(g_hdcMem);

	// 2. 将后备缓冲的内容一次性拷贝到屏幕上
	BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, g_hdcMem, 0, 0, SRCCOPY);

	EndPaint(hwnd, &ps);
}

// 鼠标左键按下事件
void OnLButtonDown(int x, int y) {
	switch (g_currentMode) {
	case DrawMode::DrawLineMidpoint:
	case DrawMode::DrawLineBresenham:
	case DrawMode::DrawCircleMidpoint:
	case DrawMode::DrawCircleBresenham:
	case DrawMode::DrawRectangle:
		if (!g_isDrawing) {
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
		InvalidateRect(g_hwnd, NULL, FALSE);
		break;

	case DrawMode::FillScanline:
	{
		// 寻找被点击的多边形
		for (auto& shape : g_shapes) {
			if (shape.type == DrawMode::DrawPolygon && !shape.filled) {
				// 简化的命中测试：检查点是否在包围盒内
				int minX = shape.vertices[0].x, maxX = shape.vertices[0].x;
				int minY = shape.vertices[0].y, maxY = shape.vertices[0].y;
				for (const auto& v : shape.vertices) {
					if (v.x < minX) minX = v.x;
					if (v.x > maxX) maxX = v.x;
					if (v.y < minY) minY = v.y;
					if (v.y > maxY) maxY = v.y;
				}
				if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
					ScanlineFill(g_hdcMem, shape.vertices, g_fillColor);
					shape.filled = true;
					shape.fillColor = g_fillColor;
					InvalidateRect(g_hwnd, NULL, FALSE);
					break;
				}
			}
		}
	}
	break;

	case DrawMode::FillFence:
		FenceFill(g_hdcMem, x, y, g_fillColor, g_drawColor);
		InvalidateRect(g_hwnd, NULL, FALSE);
		break;
	}
}

// 鼠标移动事件
void OnMouseMove(int x, int y) {
	if (!g_isDrawing || g_currentPoints.empty()) return;

	HDC hdc = GetDC(g_hwnd);
	RECT rc;
	GetClientRect(g_hwnd, &rc);

	// 使用双缓冲进行预览
	FillRect(g_hdcMem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
	RedrawAllShapes(g_hdcMem);

	Point startPoint = g_currentPoints[0];
	switch (g_currentMode) {
	case DrawMode::DrawLineMidpoint:
		DrawLineMidpoint(g_hdcMem, startPoint.x, startPoint.y, x, y, g_drawColor);
		break;
	case DrawMode::DrawLineBresenham:
		DrawLineBresenham(g_hdcMem, startPoint.x, startPoint.y, x, y, g_drawColor);
		break;
	case DrawMode::DrawCircleMidpoint:
	{
		int r = (int)sqrt(pow(x - startPoint.x, 2) + pow(y - startPoint.y, 2));
		DrawCircleMidpoint(g_hdcMem, startPoint.x, startPoint.y, r, g_drawColor);
	}
	break;
	case DrawMode::DrawCircleBresenham:
	{
		int r = (int)sqrt(pow(x - startPoint.x, 2) + pow(y - startPoint.y, 2));
		DrawCircleBresenham(g_hdcMem, startPoint.x, startPoint.y, r, g_drawColor);
	}
	break;
	case DrawMode::DrawRectangle:
		Rectangle(g_hdcMem, startPoint.x, startPoint.y, x, y);
		break;
	case DrawMode::DrawPolygon:
		if (g_currentPoints.size() > 0) {
			for (size_t i = 0; i < g_currentPoints.size() - 1; ++i) {
				DrawLineMidpoint(g_hdcMem, g_currentPoints[i].x, g_currentPoints[i].y, g_currentPoints[i + 1].x, g_currentPoints[i + 1].y, g_drawColor);
			}
			DrawLineMidpoint(g_hdcMem, g_currentPoints.back().x, g_currentPoints.back().y, x, y, g_drawColor);
		}
		break;
	case DrawMode::DrawBSpline:
		if (g_currentPoints.size() > 0) {
			for (const auto& p : g_currentPoints) {
				Ellipse(g_hdcMem, p.x - 3, p.y - 3, p.x + 3, p.y + 3); // 显示控制点
			}
			std::vector<Point> tempPoints = g_currentPoints;
			tempPoints.push_back({ x, y });
			if (tempPoints.size() > 1) {
				for (size_t i = 0; i < tempPoints.size() - 1; ++i) {
					DrawLineMidpoint(g_hdcMem, tempPoints[i].x, tempPoints[i].y, tempPoints[i + 1].x, tempPoints[i + 1].y, RGB(200, 200, 200));
				}
			}
		}
		break;
	}

	BitBlt(hdc, 0, 0, rc.right, rc.bottom, g_hdcMem, 0, 0, SRCCOPY);
	ReleaseDC(g_hwnd, hdc);
}

// 清空画布
void ClearCanvas() {
	g_shapes.clear();
	g_currentPoints.clear();
	g_isDrawing = false;
	g_currentMode = DrawMode::None;
	InvalidateRect(g_hwnd, NULL, TRUE);
}

// 完成当前图形绘制
void FinishDrawing() {
	if (g_currentPoints.empty()) {
		g_isDrawing = false;
		return;
	}

	Shape newShape;
	newShape.type = g_currentMode;
	newShape.vertices = g_currentPoints;
	newShape.color = g_drawColor;
	newShape.filled = false;

	if (g_currentMode == DrawMode::DrawPolygon && g_currentPoints.size() < 3) {
		// 多边形至少需要3个顶点
	}
	else if (g_currentMode == DrawMode::DrawBSpline && g_currentPoints.size() < 4) {
		// B样条至少需要4个控制点
	}
	else {
		g_shapes.push_back(newShape);
	}

	g_currentPoints.clear();
	// 多边形和B样条模式下，可以继续添加点，除非用户切换模式
	if (g_currentMode != DrawMode::DrawPolygon && g_currentMode != DrawMode::DrawBSpline) {
		g_isDrawing = false;
	}
	InvalidateRect(g_hwnd, NULL, FALSE);
}

// 重绘所有已保存的图形
void RedrawAllShapes(HDC hdc) {
	for (const auto& shape : g_shapes) {
		// 如果图形已填充，先执行填充
		if (shape.filled) {
			if (shape.type == DrawMode::DrawPolygon) {
				ScanlineFill(hdc, shape.vertices, shape.fillColor);
			}
			// (可以为其他可填充图形添加逻辑)
		}

		// 绘制图形边框
		switch (shape.type) {
		case DrawMode::DrawLineMidpoint:
			DrawLineMidpoint(hdc, shape.vertices[0].x, shape.vertices[0].y, shape.vertices[1].x, shape.vertices[1].y, shape.color);
			break;
		case DrawMode::DrawLineBresenham:
			DrawLineBresenham(hdc, shape.vertices[0].x, shape.vertices[0].y, shape.vertices[1].x, shape.vertices[1].y, shape.color);
			break;
		case DrawMode::DrawCircleMidpoint:
		{
			int r = (int)sqrt(pow(shape.vertices[1].x - shape.vertices[0].x, 2) + pow(shape.vertices[1].y - shape.vertices[0].y, 2));
			DrawCircleMidpoint(hdc, shape.vertices[0].x, shape.vertices[0].y, r, shape.color);
		}
		break;
		case DrawMode::DrawCircleBresenham:
		{
			int r = (int)sqrt(pow(shape.vertices[1].x - shape.vertices[0].x, 2) + pow(shape.vertices[1].y - shape.vertices[0].y, 2));
			DrawCircleBresenham(hdc, shape.vertices[0].x, shape.vertices[0].y, r, shape.color);
		}
		break;
		case DrawMode::DrawRectangle:
			Rectangle(hdc, shape.vertices[0].x, shape.vertices[0].y, shape.vertices[1].x, shape.vertices[1].y);
			break;
		case DrawMode::DrawPolygon:
			DrawPolygon(hdc, shape.vertices, shape.color);
			break;
		case DrawMode::DrawBSpline:
			DrawBSpline(hdc, shape.vertices, shape.color);
			break;
		}
	}
}