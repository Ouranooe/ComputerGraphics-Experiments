#include "Clip.h"
#include <cmath>
#include <algorithm>
#include <list>
#include <set>
#include <map>

namespace GraphicsEngine {

// ----- Cohen-Sutherland 裁剪 -----
static const int CS_INSIDE = 0;
static const int CS_LEFT   = 1;
static const int CS_RIGHT  = 2;
static const int CS_BOTTOM = 4;
static const int CS_TOP    = 8;

int CS_GetOutCode(double x, double y, double xmin, double xmax, double ymin, double ymax) {
    int code = CS_INSIDE;
    if (x < xmin) code |= CS_LEFT;
    else if (x > xmax) code |= CS_RIGHT;
    if (y < ymin) code |= CS_TOP;
    else if (y > ymax) code |= CS_BOTTOM;
    return code;
}

bool CohenSutherlandClip(double& x1, double& y1, double& x2, double& y2,
    double xmin, double xmax, double ymin, double ymax) {
    int out1 = CS_GetOutCode(x1, y1, xmin, xmax, ymin, ymax);
    int out2 = CS_GetOutCode(x2, y2, xmin, xmax, ymin, ymax);
    bool accept = false;
    while (true) {
        if ((out1 | out2) == 0) { accept = true; break; }
        else if (out1 & out2) { break; }
        else {
            double x, y;
            int out = out1 ? out1 : out2;
            if (out & CS_TOP) {
                y = ymin;
                x = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            }
            else if (out & CS_BOTTOM) {
                y = ymax;
                x = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            }
            else if (out & CS_RIGHT) {
                x = xmax;
                y = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
            }
            else {
                x = xmin;
                y = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
            }
            if (out == out1) {
                x1 = x; y1 = y;
                out1 = CS_GetOutCode(x1, y1, xmin, xmax, ymin, ymax);
            }
            else {
                x2 = x; y2 = y;
                out2 = CS_GetOutCode(x2, y2, xmin, xmax, ymin, ymax);
            }
        }
    }
    return accept;
}

void ClipAllLines_CohenSutherland(const RECT& clip) {
    double xmin = clip.left;
    double xmax = clip.right;
    double ymin = clip.top;
    double ymax = clip.bottom;

    std::vector<Shape> newShapes;
    for (auto s : g_shapes) {
        if (s.type == DrawMode::DrawLineMidpoint || s.type == DrawMode::DrawLineBresenham) {
            double x1 = s.vertices[0].x;
            double y1 = s.vertices[0].y;
            double x2 = s.vertices[1].x;
            double y2 = s.vertices[1].y;
            if (CohenSutherlandClip(x1, y1, x2, y2, xmin, xmax, ymin, ymax)) {
                s.vertices[0].x = (int)std::round(x1);
                s.vertices[0].y = (int)std::round(y1);
                s.vertices[1].x = (int)std::round(x2);
                s.vertices[1].y = (int)std::round(y2);
                newShapes.push_back(s);
            }
        }
        else {
            newShapes.push_back(s);
        }
    }
    g_shapes.swap(newShapes);
}

// ----- 中点分割法 -----
bool InsideRect(double x, double y, double xmin, double xmax, double ymin, double ymax) {
    return x >= xmin && x <= xmax && y >= ymin && y <= ymax;
}

void MidClipLineRec(double x1, double y1, double x2, double y2,
    double xmin, double xmax, double ymin, double ymax,
    int depth,
    std::vector<std::pair<Point, Point>>& outSegs) {
    bool in1 = InsideRect(x1, y1, xmin, xmax, ymin, ymax);
    bool in2 = InsideRect(x2, y2, xmin, xmax, ymin, ymax);

    if (in1 && in2) {
        Point a{ (int)std::round(x1), (int)std::round(y1) };
        Point b{ (int)std::round(x2), (int)std::round(y2) };
        outSegs.push_back({ a, b });
        return;
    }
    if (depth > 20) {
        if (in1 || in2) {
            Point a{ (int)std::round(x1), (int)std::round(y1) };
            Point b{ (int)std::round(x2), (int)std::round(y2) };
            outSegs.push_back({ a, b });
        }
        return;
    }

    double dx = x2 - x1;
    double dy = y2 - y1;
    if (std::fabs(dx) < 0.5 && std::fabs(dy) < 0.5) {
        if (in1 && in2) {
            Point a{ (int)std::round(x1), (int)std::round(y1) };
            Point b{ (int)std::round(x2), (int)std::round(y2) };
            outSegs.push_back({ a, b });
        }
        return;
    }

    double mx = (x1 + x2) * 0.5;
    double my = (y1 + y2) * 0.5;

    MidClipLineRec(x1, y1, mx, my, xmin, xmax, ymin, ymax, depth + 1, outSegs);
    MidClipLineRec(mx, my, x2, y2, xmin, xmax, ymin, ymax, depth + 1, outSegs);
}

void ClipAllLines_Midpoint(const RECT& clip) {
    double xmin = clip.left;
    double xmax = clip.right;
    double ymin = clip.top;
    double ymax = clip.bottom;

    std::vector<Shape> newShapes;
    for (auto s : g_shapes) {
        if (s.type == DrawMode::DrawLineMidpoint || s.type == DrawMode::DrawLineBresenham) {
            std::vector<std::pair<Point, Point>> segs;
            MidClipLineRec(
                (double)s.vertices[0].x, (double)s.vertices[0].y,
                (double)s.vertices[1].x, (double)s.vertices[1].y,
                xmin, xmax, ymin, ymax, 0, segs
            );
            for (auto& seg : segs) {
                Shape ns = s;
                ns.vertices.clear();
                ns.vertices.push_back(seg.first);
                ns.vertices.push_back(seg.second);
                newShapes.push_back(ns);
            }
        }
        else {
            newShapes.push_back(s);
        }
    }
    g_shapes.swap(newShapes);
}

// ----- Sutherland-Hodgman 多边形裁剪 -----
Point IntersectEdge(const Point& p1, const Point& p2, char edge, const RECT& r) {
    double x1 = p1.x, y1 = p1.y;
    double x2 = p2.x, y2 = p2.y;
    double x = 0, y = 0;
    switch (edge) {
    case 'L': x = r.left;  y = y1 + (y2 - y1) * (x - x1) / (x2 - x1); break;
    case 'R': x = r.right; y = y1 + (y2 - y1) * (x - x1) / (x2 - x1); break;
    case 'T': y = r.top;   x = x1 + (x2 - x1) * (y - y1) / (y2 - y1); break;
    case 'B': y = r.bottom;x = x1 + (x2 - x1) * (y - y1) / (y2 - y1); break;
    }
    return { (int)std::round(x), (int)std::round(y) };
}

bool InsideEdge(const Point& p, char edge, const RECT& r) {
    switch (edge) {
    case 'L': return p.x >= r.left;
    case 'R': return p.x <= r.right;
    case 'T': return p.y >= r.top;
    case 'B': return p.y <= r.bottom;
    }
    return false;
}

std::vector<Point> ClipWithEdge(const std::vector<Point>& poly, char edge, const RECT& r) {
    std::vector<Point> out;
    if (poly.empty()) return out;
    Point S = poly.back();
    for (auto& E : poly) {
        bool Sin = InsideEdge(S, edge, r);
        bool Ein = InsideEdge(E, edge, r);
        if (Sin && Ein) {
            out.push_back(E);
        }
        else if (Sin && !Ein) {
            Point I = IntersectEdge(S, E, edge, r);
            out.push_back(I);
        }
        else if (!Sin && Ein) {
            Point I = IntersectEdge(S, E, edge, r);
            out.push_back(I);
            out.push_back(E);
        }
        S = E;
    }
    return out;
}

std::vector<Point> ClipPolygon_SutherlandHodgman(const std::vector<Point>& poly, const RECT& r) {
    std::vector<Point> out = poly;
    out = ClipWithEdge(out, 'L', r);
    out = ClipWithEdge(out, 'R', r);
    out = ClipWithEdge(out, 'T', r);
    out = ClipWithEdge(out, 'B', r);
    return out;
}

// ============== Weiler-Atherton 完整实现 ==============

// 顶点节点结构
struct WAVertex {
    double x, y;
    bool isIntersection;    // 是否为交点
    bool isEntering;        // true=进入裁剪区, false=离开裁剪区
    bool visited;           // 遍历时是否已访问
    WAVertex* next;         // 在当前多边形链表中的下一个
    WAVertex* other;        // 指向另一个多边形链表中对应的交点节点
    double t;               // 交点的参数位置（用于排序）
    
    WAVertex(double _x, double _y)
        : x(_x), y(_y), isIntersection(false), isEntering(false),
          visited(false), next(nullptr), other(nullptr), t(0) {}
};

// 判断点是否在矩形内部
static bool WA_PointInRect(double x, double y, const RECT& r) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

// 计算两线段的交点
static bool WA_LineIntersect(double x1, double y1, double x2, double y2,
                              double x3, double y3, double x4, double y4,
                              double& ix, double& iy, double& t1, double& t2) {
    double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (std::abs(denom) < 1e-10) return false;
    
    t1 = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
    t2 = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;
    
    // 检查交点是否在两条线段上（不包括端点，避免重复计算）
    if (t1 > 1e-10 && t1 < 1 - 1e-10 && t2 > 1e-10 && t2 < 1 - 1e-10) {
        ix = x1 + t1 * (x2 - x1);
        iy = y1 + t1 * (y2 - y1);
        return true;
    }
    return false;
}

// 释放顶点链表
static void WA_FreeList(WAVertex* head) {
    if (!head) return;
    WAVertex* curr = head;
    do {
        WAVertex* next = curr->next;
        delete curr;
        curr = next;
    } while (curr && curr != head);
}

// Weiler-Atherton 算法主函数
std::vector<Point> ClipPolygon_WeilerAtherton_Rect(const std::vector<Point>& poly, const RECT& r) {
    if (poly.size() < 3) return {};
    
    // 创建裁剪矩形的四个顶点（顺时针）
    std::vector<Point> clipPoly;
    clipPoly.push_back({r.left, r.top});
    clipPoly.push_back({r.right, r.top});
    clipPoly.push_back({r.right, r.bottom});
    clipPoly.push_back({r.left, r.bottom});
    
    // 检查主多边形是否完全在裁剪区内
    bool allInside = true;
    for (const auto& p : poly) {
        if (!WA_PointInRect((double)p.x, (double)p.y, r)) {
            allInside = false;
            break;
        }
    }
    if (allInside) return poly;
    
    // 检查主多边形是否完全在裁剪区外
    bool allOutside = true;
    for (const auto& p : poly) {
        if (WA_PointInRect((double)p.x, (double)p.y, r)) {
            allOutside = false;
            break;
        }
    }
    if (allOutside) {
        // 可能还有交点，不能直接返回空
    }
    
    // 构建主多边形的循环链表
    WAVertex* subjHead = nullptr;
    WAVertex* subjTail = nullptr;
    std::vector<WAVertex*> subjVertices;
    for (const auto& p : poly) {
        WAVertex* v = new WAVertex((double)p.x, (double)p.y);
        subjVertices.push_back(v);
        if (!subjHead) {
            subjHead = v;
            subjTail = v;
        } else {
            subjTail->next = v;
            subjTail = v;
        }
    }
    subjTail->next = subjHead;
    
    // 构建裁剪多边形的循环链表
    WAVertex* clipHead = nullptr;
    WAVertex* clipTail = nullptr;
    std::vector<WAVertex*> clipVertices;
    for (const auto& p : clipPoly) {
        WAVertex* v = new WAVertex((double)p.x, (double)p.y);
        clipVertices.push_back(v);
        if (!clipHead) {
            clipHead = v;
            clipTail = v;
        } else {
            clipTail->next = v;
            clipTail = v;
        }
    }
    clipTail->next = clipHead;
    
    // 存储所有交点，按主多边形边排序
    std::vector<std::vector<std::pair<double, WAVertex*>>> subjIntersections(poly.size());
    std::vector<std::vector<std::pair<double, WAVertex*>>> clipIntersections(clipPoly.size());
    
    // 计算所有交点
    for (size_t i = 0; i < poly.size(); i++) {
        size_t i2 = (i + 1) % poly.size();
        double sx1 = (double)poly[i].x, sy1 = (double)poly[i].y;
        double sx2 = (double)poly[i2].x, sy2 = (double)poly[i2].y;
        
        for (size_t j = 0; j < clipPoly.size(); j++) {
            size_t j2 = (j + 1) % clipPoly.size();
            double cx1 = (double)clipPoly[j].x, cy1 = (double)clipPoly[j].y;
            double cx2 = (double)clipPoly[j2].x, cy2 = (double)clipPoly[j2].y;
            
            double ix, iy, t1, t2;
            if (WA_LineIntersect(sx1, sy1, sx2, sy2, cx1, cy1, cx2, cy2, ix, iy, t1, t2)) {
                // 创建两个交点节点
                WAVertex* vSubj = new WAVertex(ix, iy);
                WAVertex* vClip = new WAVertex(ix, iy);
                vSubj->isIntersection = true;
                vClip->isIntersection = true;
                vSubj->t = t1;
                vClip->t = t2;
                vSubj->other = vClip;
                vClip->other = vSubj;
                
                // 判断进入/离开
                // 从主多边形边的方向向量和裁剪边的法向量计算
                double dx = sx2 - sx1;
                double dy = sy2 - sy1;
                // 裁剪边的内法向量（指向矩形内部）
                double cx = cx2 - cx1;
                double cy = cy2 - cy1;
                // 根据边界判断法向量方向
                double nx, ny;
                if (j == 0) { nx = 0; ny = 1; }       // 上边，内法向向下
                else if (j == 1) { nx = -1; ny = 0; } // 右边，内法向向左
                else if (j == 2) { nx = 0; ny = -1; } // 下边，内法向向上
                else { nx = 1; ny = 0; }              // 左边，内法向向右
                
                double dot = dx * nx + dy * ny;
                vSubj->isEntering = (dot > 0);
                vClip->isEntering = !vSubj->isEntering;
                
                subjIntersections[i].push_back({t1, vSubj});
                clipIntersections[j].push_back({t2, vClip});
            }
        }
    }
    
    // 如果没有交点
    if (subjIntersections.empty() || 
        std::all_of(subjIntersections.begin(), subjIntersections.end(),
            [](const auto& v) { return v.empty(); })) {
        // 释放资源
        WA_FreeList(subjHead);
        WA_FreeList(clipHead);
        
        if (allInside) return poly;
        return {};
    }
    
    // 按参数t排序每条边上的交点
    for (auto& v : subjIntersections) {
        std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    }
    for (auto& v : clipIntersections) {
        std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    }
    
    // 将交点插入主多边形链表
    for (size_t i = 0; i < poly.size(); i++) {
        WAVertex* startV = subjVertices[i];
        WAVertex* endV = startV->next;
        WAVertex* curr = startV;
        for (auto& p : subjIntersections[i]) {
            WAVertex* inter = p.second;
            inter->next = curr->next;
            curr->next = inter;
            curr = inter;
        }
    }
    
    // 将交点插入裁剪多边形链表
    for (size_t j = 0; j < clipPoly.size(); j++) {
        WAVertex* startV = clipVertices[j];
        WAVertex* curr = startV;
        for (auto& p : clipIntersections[j]) {
            WAVertex* inter = p.second;
            inter->next = curr->next;
            curr->next = inter;
            curr = inter;
        }
    }
    
    // 收集结果多边形
    std::vector<Point> result;
    
    // 从一个进入交点开始遍历
    WAVertex* start = nullptr;
    for (size_t i = 0; i < subjIntersections.size() && !start; i++) {
        for (auto& p : subjIntersections[i]) {
            if (p.second->isEntering && !p.second->visited) {
                start = p.second;
                break;
            }
        }
    }
    
    if (!start) {
        // 没有进入点，释放资源并返回
        // 需要分别释放主链表和裁剪链表，以及所有交点
        WAVertex* curr = subjHead;
        do {
            WAVertex* next = curr->next;
            if (!curr->isIntersection) delete curr;
            curr = next;
        } while (curr != subjHead);
        
        curr = clipHead;
        do {
            WAVertex* next = curr->next;
            if (!curr->isIntersection) delete curr;
            curr = next;
        } while (curr != clipHead);
        
        for (auto& v : subjIntersections) {
            for (auto& p : v) delete p.second;
        }
        
        return {};
    }
    
    // 遍历生成结果多边形
    WAVertex* curr = start;
    bool onSubject = true;  // 当前在主多边形上遍历
    
    do {
        result.push_back({(LONG)std::round(curr->x), (LONG)std::round(curr->y)});
        curr->visited = true;
        if (curr->other) curr->other->visited = true;
        
        if (curr->isIntersection) {
            // 切换到另一个多边形
            if (curr->isEntering) {
                // 进入点：沿主多边形前进
                onSubject = true;
            } else {
                // 离开点：切换到裁剪多边形
                onSubject = false;
                curr = curr->other;
            }
        }
        
        curr = curr->next;
        
        // 如果遇到进入点但在裁剪多边形上，切回主多边形
        if (curr->isIntersection && !onSubject && curr->other->isEntering) {
            curr = curr->other;
            onSubject = true;
        }
        
    } while (curr != start && result.size() < poly.size() * 4 + 10);
    
    // 清理内存
    WAVertex* v = subjHead;
    do {
        WAVertex* next = v->next;
        delete v;
        v = next;
    } while (v != subjHead);
    
    // 裁剪多边形的顶点已在上面释放（交点是共享的）
    // 只释放非交点的裁剪顶点
    for (WAVertex* cv : clipVertices) {
        delete cv;
    }
    
    return result;
}

void ClipAllPolygons_SH(const RECT& clip) {
    for (auto& s : g_shapes) {
        if (s.type == DrawMode::DrawPolygon)
            s.vertices = ClipPolygon_SutherlandHodgman(s.vertices, clip);
    }
    g_shapes.erase(
        std::remove_if(g_shapes.begin(), g_shapes.end(),
            [](const Shape& s) { return s.type == DrawMode::DrawPolygon && s.vertices.size() < 3; }),
        g_shapes.end()
    );
}

void ClipAllPolygons_WA(const RECT& clip) {
    for (auto& s : g_shapes) {
        if (s.type == DrawMode::DrawPolygon)
            s.vertices = ClipPolygon_WeilerAtherton_Rect(s.vertices, clip);
    }
    g_shapes.erase(
        std::remove_if(g_shapes.begin(), g_shapes.end(),
            [](const Shape& s) { return s.type == DrawMode::DrawPolygon && s.vertices.size() < 3; }),
        g_shapes.end()
    );
}

} // namespace GraphicsEngine
