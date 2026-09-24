#include "runtime/engine.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/meta/reflection/reflection_register.h"

#if defined(_WIN32)
#    include "runtime/core/base/renderdoc_app.h"
#    include <cstdlib>

extern "C" __declspec(dllimport) void* __stdcall GetModuleHandleA(const char* module_name);
extern "C" __declspec(dllimport) void* __stdcall GetProcAddress(void* module, const char* procedure_name);
#endif

#include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/input/input_system.h"
#include "runtime/function/particle/particle_manager.h"
#include "runtime/function/physics/physics_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_scene.h"
#include "runtime/function/render/window_system.h"
#include "runtime/function/render/debugdraw/debug_draw_manager.h"

namespace Piccolo
{
    namespace
    {
#if defined(_WIN32)
        RENDERDOC_API_1_1_2* beginRenderDocCaptureIfRequested(int frame_count)
        {
            const char* requested_frame_text = std::getenv("PICCOLO_RENDERDOC_CAPTURE_FRAME");
            if (requested_frame_text == nullptr)
            {
                return nullptr;
            }

            char*      parse_end       = nullptr;
            const long requested_frame = std::strtol(requested_frame_text, &parse_end, 10);
            if (parse_end == requested_frame_text || *parse_end != '\0' || requested_frame <= 0 ||
                frame_count != requested_frame)
            {
                return nullptr;
            }

            void* renderdoc_module = GetModuleHandleA("renderdoc.dll");
            if (renderdoc_module == nullptr)
            {
                LOG_WARN("RenderDoc capture requested for frame {}, but renderdoc.dll is not loaded", frame_count);
                return nullptr;
            }

            const auto get_renderdoc_api = reinterpret_cast<pRENDERDOC_GetAPI>(
                GetProcAddress(renderdoc_module, "RENDERDOC_GetAPI"));
            RENDERDOC_API_1_1_2* renderdoc_api = nullptr;
            if (get_renderdoc_api == nullptr ||
                get_renderdoc_api(eRENDERDOC_API_Version_1_1_2,
                                  reinterpret_cast<void**>(&renderdoc_api)) != 1 ||
                renderdoc_api == nullptr)
            {
                LOG_WARN("RenderDoc capture requested for frame {}, but the in-application API is unavailable",
                         frame_count);
                return nullptr;
            }

            if (const char* capture_file = std::getenv("PICCOLO_RENDERDOC_CAPTURE_FILE"))
            {
                renderdoc_api->SetCaptureFilePathTemplate(capture_file);
            }

            renderdoc_api->StartFrameCapture(nullptr, nullptr);
            LOG_INFO("RenderDoc capture start requested for frame {}: active={}",
                     frame_count,
                     renderdoc_api->IsFrameCapturing());
            return renderdoc_api;
        }
#endif
    } // namespace

    bool                            g_is_editor_mode {false};
    std::unordered_set<std::string> g_editor_tick_component_types {};

    void PiccoloEngine::startEngine(const std::string& config_file_path)
    {
        Reflection::TypeMetaRegister::metaRegister();

        g_runtime_global_context.startSystems(config_file_path);

        LOG_INFO("engine start");
    }

    void PiccoloEngine::shutdownEngine()
    {
        LOG_INFO("engine shutdown");

        g_runtime_global_context.shutdownSystems();

        Reflection::TypeMetaRegister::metaUnregister();
    }

    void PiccoloEngine::initialize() {}
    void PiccoloEngine::clear() {}

    void PiccoloEngine::run()
    {
        std::shared_ptr<WindowSystem> window_system = g_runtime_global_context.m_window_system;
        ASSERT(window_system);

        while (!window_system->shouldClose())
        {
            const float delta_time = calculateDeltaTime();
            tickOneFrame(delta_time);
        }
    }

    float PiccoloEngine::calculateDeltaTime()
    {
        float delta_time;
        {
            using namespace std::chrono;

            steady_clock::time_point tick_time_point = steady_clock::now();
            duration<float> time_span = duration_cast<duration<float>>(tick_time_point - m_last_tick_time_point);
            delta_time                = time_span.count();

            m_last_tick_time_point = tick_time_point;
        }
        return delta_time;
    }

    bool PiccoloEngine::tickOneFrame(float delta_time)
    {
        using namespace std::chrono;

        steady_clock::time_point logic_begin = steady_clock::now();
        logicalTick(delta_time);
        steady_clock::time_point logic_end = steady_clock::now();

        float logic_ms = duration<float, std::milli>(logic_end - logic_begin).count();

        calculateFPS(delta_time);

#if defined(_WIN32)
        RENDERDOC_API_1_1_2* renderdoc_capture = beginRenderDocCaptureIfRequested(m_frame_count);
#endif

        // single thread
        // exchange data between logic and render contexts
        g_runtime_global_context.m_render_system->swapLogicRenderData();

        steady_clock::time_point render_begin = steady_clock::now();
        rendererTick(delta_time);
#if defined(_WIN32)
        if (renderdoc_capture != nullptr)
        {
            const uint32_t capture_saved = renderdoc_capture->EndFrameCapture(nullptr, nullptr);
            LOG_INFO("RenderDoc capture end requested for frame {}: saved={}", m_frame_count, capture_saved);
        }
#endif
        steady_clock::time_point render_end = steady_clock::now();

        float render_ms = duration<float, std::milli>(render_end - render_begin).count();

        // --- sliding-window average via ring buffer ---
        // drop the value about to be overwritten (oldest once full), insert new, advance pointer
        m_logic_ms_sum  -= m_logic_ms_buf[m_avg_index];
        m_render_ms_sum -= m_render_ms_buf[m_avg_index];

        m_logic_ms_buf[m_avg_index]  = logic_ms;
        m_render_ms_buf[m_avg_index] = render_ms;

        m_logic_ms_sum  += logic_ms;
        m_render_ms_sum += render_ms;

        m_avg_index = (m_avg_index + 1) % s_avg_window;
        if (m_avg_count < s_avg_window) ++m_avg_count;

        LOG_INFO("logic ms avg: {:.3f}, render ms avg: {:.3f}",
                 m_logic_ms_sum / m_avg_count, m_render_ms_sum / m_avg_count);


#ifdef ENABLE_PHYSICS_DEBUG_RENDERER
        g_runtime_global_context.m_physics_manager->renderPhysicsWorld(delta_time);
#endif

        g_runtime_global_context.m_window_system->pollEvents();


        g_runtime_global_context.m_window_system->setTitle(
            std::string("Piccolo - " + std::to_string(getFPS()) + " FPS").c_str());

        const bool should_window_close = g_runtime_global_context.m_window_system->shouldClose();
        return !should_window_close;
    }

    void PiccoloEngine::logicalTick(float delta_time)
    {
        g_runtime_global_context.m_world_manager->tick(delta_time);
        g_runtime_global_context.m_input_system->tick();
    }

    bool PiccoloEngine::rendererTick(float delta_time)
    {
        g_runtime_global_context.m_render_system->tick(delta_time);
        return true;
    }

    const float PiccoloEngine::s_fps_alpha = 1.f / 100;
    void        PiccoloEngine::calculateFPS(float delta_time)
    {
        m_frame_count++;

        if (m_frame_count == 1)
        {
            m_average_duration = delta_time;
        }
        else
        {
            m_average_duration = m_average_duration * (1 - s_fps_alpha) + delta_time * s_fps_alpha;
        }

        m_fps = static_cast<int>(1.f / m_average_duration);
    }

    EngineStats PiccoloEngine::getEngineStats() const
    {
        EngineStats stats;
        stats.fps = m_fps;

        if (m_avg_count > 0)
        {
            stats.logic_ms_avg  = m_logic_ms_sum / m_avg_count;
            stats.render_ms_avg = m_render_ms_sum / m_avg_count;
        }

        auto render_sys = g_runtime_global_context.m_render_system;
        if (render_sys)
        {
            auto scene = render_sys->getRenderScene();
            if (scene)
            {
                stats.render_entity_count = static_cast<int>(scene->m_render_entities.size());
                stats.point_light_count   = static_cast<int>(scene->m_point_light_list.m_lights.size());
                stats.visible_mesh_count  = static_cast<int>(scene->m_main_camera_visible_mesh_nodes.size());
            }
        }

        return stats;
    }
} // namespace Piccolo
