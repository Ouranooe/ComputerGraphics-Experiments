#pragma once
#include <d2d1.h>

// ����任ģ�飺��ԭ���Ƶ��������ģ���ʹY������
class CoordinateTransform
{
public:
    CoordinateTransform() : pRenderTarget(nullptr) {}

    // ��ʼ������������ϵͳ
    void Initialize(ID2D1HwndRenderTarget* renderTarget)
    {
        pRenderTarget = renderTarget;
        ApplyTransform();
    }

    // ÿ�δ��ڴ�С�仯ʱ����
    void OnResize()
    {
        ApplyTransform();
    }

private:
    ID2D1HwndRenderTarget* pRenderTarget;

    void ApplyTransform()
    {
        if (!pRenderTarget) return;

        // ��ȡ��ȾĿ���С
        D2D1_SIZE_F rtSize = pRenderTarget->GetSize();

        // Step 1: ������ԭ���ƶ�����������
        D2D1::Matrix3x2F translation = D2D1::Matrix3x2F::Translation(
            rtSize.width / 2.0f,
            rtSize.height / 2.0f
        );

        // Step 2: ��תY�ᣨ��Y����������
        D2D1::Matrix3x2F flipY = D2D1::Matrix3x2F::Scale(1.0f, -1.0f, D2D1::Point2F(0, 0));

        // Step 3: �ϲ��任����ע��˳���ȷ�ת����ƽ�ƣ�
        D2D1::Matrix3x2F finalTransform = flipY * translation;

        // Ӧ�ñ任
        pRenderTarget->SetTransform(finalTransform);
    }
};
