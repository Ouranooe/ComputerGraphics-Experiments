#pragma once
#include <d2d1.h>

// 坐标变换模块：将原点移到窗口中心，并使Y轴向上
class CoordinateTransform
{
public:
    CoordinateTransform() : pRenderTarget(nullptr) {}

    // 初始化并设置坐标系统
    void Initialize(ID2D1HwndRenderTarget* renderTarget)
    {
        pRenderTarget = renderTarget;
        ApplyTransform();
    }

    // 每次窗口大小变化时调用
    void OnResize()
    {
        ApplyTransform();
    }

private:
    ID2D1HwndRenderTarget* pRenderTarget;

    void ApplyTransform()
    {
        if (!pRenderTarget) return;

        // 获取渲染目标大小
        D2D1_SIZE_F rtSize = pRenderTarget->GetSize();

        // Step 1: 将坐标原点移动到窗口中心
        D2D1::Matrix3x2F translation = D2D1::Matrix3x2F::Translation(
            rtSize.width / 2.0f,
            rtSize.height / 2.0f
        );

        // Step 2: 翻转Y轴（让Y轴向上增大）
        D2D1::Matrix3x2F flipY = D2D1::Matrix3x2F::Scale(1.0f, -1.0f, D2D1::Point2F(0, 0));

        // Step 3: 合并变换矩阵（注意顺序：先翻转，再平移）
        D2D1::Matrix3x2F finalTransform = flipY * translation;

        // 应用变换
        pRenderTarget->SetTransform(finalTransform);
    }
};
