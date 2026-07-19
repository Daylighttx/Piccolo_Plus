// [UIPass] 把编辑器 UI（ImGui）画到屏幕的 subpass。
// 接在 FXAA 之后，画到 backup_even；同时 preserve backup_odd（场景图）供下一棒 combine_ui 合成时用。
#pragma once

#include "runtime/function/render/render_pass.h"

namespace Piccolo
{
    class WindowUI;

    struct UIPassInitInfo : RenderPassInitInfo
    {
        RHIRenderPass* render_pass;
    };

    class UIPass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void initializeUIRenderBackend(WindowUI* window_ui) override final;
        void draw() override final;

    private:
        void uploadFonts();

    private:
        WindowUI* m_window_ui;
    };
} // namespace Piccolo
