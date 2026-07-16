#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_set>

namespace Piccolo
{
    extern bool                            g_is_editor_mode;
    extern std::unordered_set<std::string> g_editor_tick_component_types;

    class PiccoloEngine
    {
        friend class PiccoloEditor;

        static const float s_fps_alpha;

    public:
        void startEngine(const std::string& config_file_path);
        void shutdownEngine();

        void initialize();
        void clear();

        bool isQuit() const { return m_is_quit; }
        void run();
        bool tickOneFrame(float delta_time);

        int getFPS() const { return m_fps; }

    protected:
        void logicalTick(float delta_time);
        bool rendererTick(float delta_time);

        void calculateFPS(float delta_time);

        /**
         *  Each frame can only be called once
         */
        float calculateDeltaTime();

    protected:
        bool m_is_quit {false};

        std::chrono::steady_clock::time_point m_last_tick_time_point {std::chrono::steady_clock::now()};

        float m_average_duration {0.f};
        int   m_frame_count {0};
        int   m_fps {0};
        static constexpr int            s_avg_window {120}; // sliding-window size
        std::array<float, s_avg_window>  m_logic_ms_buf  {};  // ring buffer of recent frame costs
        std::array<float, s_avg_window>  m_render_ms_buf {};
        float                           m_logic_ms_sum  {0.f}; // running sum over the window
        float                           m_render_ms_sum {0.f};
        int                             m_avg_index {0};       // write pointer, cycles 0..s_avg_window-1
        int                             m_avg_count {0};       // valid samples so far (warm-up guard)
    };

} // namespace Piccolo
