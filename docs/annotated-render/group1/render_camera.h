#pragma once

#include "runtime/core/math/math_headers.h"  // 数学库(向量/矩阵/四元数)

#include <mutex>  // 互斥锁(保护视图矩阵)

namespace Piccolo
{
    // 相机类型：编辑器自由相机 vs 车载(跟随)相机
    enum class RenderCameraType : int
    {
        Editor,  // 编辑器相机
        Motor    // 车载相机
    };

    // 渲染层使用的相机
    class RenderCamera
    {
    public:
        RenderCameraType m_current_camera_type {RenderCameraType::Editor};  // 当前相机类型

        static const Vector3 X, Y, Z;  // 静态基向量(X=右, Y=前, Z=上)

        Vector3    m_position {0.0f, 0.0f, 0.0f};  // 相机世界位置
        Quaternion m_rotation {Quaternion::IDENTITY};    // 相机朝向(旋转)
        Quaternion m_invRotation {Quaternion::IDENTITY};  // 旋转的逆(用于算前/上/右方向)
        float      m_znear {1000.0f};  // 近裁剪面(值由逻辑层设置)
        float      m_zfar {0.1f};      // 远裁剪面(值由逻辑层设置)
        Vector3    m_up_axis {Z};      // 上方向轴

        static constexpr float MIN_FOV {10.0f};   // 最小视场角
        static constexpr float MAX_FOV {89.0f};   // 最大视场角
        static constexpr int   MAIN_VIEW_MATRIX_INDEX {0};  // 主视图索引

        std::vector<Matrix4x4> m_view_matrices {Matrix4x4::IDENTITY};  // 视图矩阵数组(支持多视图如阴影)

        void setCurrentCameraType(RenderCameraType type);  // 设置相机类型
        // 设主视图矩阵(逻辑层算好传进来)
        void setMainViewMatrix(const Matrix4x4& view_matrix, RenderCameraType type = RenderCameraType::Editor);

        void move(Vector3 delta);     // 平移
        void rotate(Vector2 delta);   // 旋转(鼠标拖动)
        void zoom(float offset);      // 缩放(改 FOV)
        void lookAt(const Vector3& position, const Vector3& target, const Vector3& up);  // 看向目标

        void setAspect(float aspect);                    // 设宽高比
        void setFOVx(float fovx) { m_fovx = fovx; }      // 设水平 FOV

        Vector3    position() const { return m_position; }                  // 取位置
        Quaternion rotation() const { return m_rotation; }                  // 取旋转
        Vector3    forward() const { return (m_invRotation * Y); }          // 前方向 = 逆旋转作用于 Y
        Vector3    up() const { return (m_invRotation * Z); }               // 上方向 = 逆旋转作用于 Z
        Vector3    right() const { return (m_invRotation * X); }            // 右方向 = 逆旋转作用于 X
        Vector2    getFOV() const { return {m_fovx, m_fovy}; }              // 取 FOV(x,y)
        Matrix4x4  getViewMatrix();                                         // 取视图矩阵
        Matrix4x4  getPersProjMatrix() const;                               // 取透视投影矩阵
        // 由位置/前/上构造 lookAt 矩阵
        Matrix4x4  getLookAtMatrix() const { return Math::makeLookAtMatrix(position(), position() + forward(), up()); }
        float      getFovYDeprecated() const { return m_fovy; }            // 取垂直 FOV(旧接口)

    protected:
        float   m_aspect {0.f};  // 宽高比
        float   m_fovx {Degree(89.f).valueDegrees()};  // 水平 FOV(默认约89度)
        float   m_fovy {0.f};    // 垂直 FOV(由 aspect 推出)
        std::mutex m_view_matrix_mutex;  // 视图矩阵互斥锁
    };

    // 静态基向量定义
    inline const Vector3 RenderCamera::X = {1.0f, 0.0f, 0.0f};
    inline const Vector3 RenderCamera::Y = {0.0f, 1.0f, 0.0f};
    inline const Vector3 RenderCamera::Z = {0.0f, 0.0f, 1.0f};

} // namespace Piccolo
