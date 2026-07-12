#include "runtime/core/log/log_system.h"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Piccolo
{
    LogSystem::LogSystem()
    {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        console_sink->set_pattern("[%^%l%$] %v");

        // 同步写文件，崩溃前最后一行也能落盘（诊断用）
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("PiccoloEditor.log", true);
        file_sink->set_level(spdlog::level::trace);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

        const spdlog::sinks_init_list sink_list = {console_sink, file_sink};

        m_logger = std::make_shared<spdlog::logger>("muggle_logger", sink_list.begin(), sink_list.end());
        m_logger->set_level(spdlog::level::trace);
        m_logger->flush_on(spdlog::level::trace); // 每条日志立即刷盘，避免崩溃丢日志

        spdlog::register_logger(m_logger);
    }

    LogSystem::~LogSystem()
    {
        m_logger->flush();
        spdlog::drop_all();
    }

} // namespace Piccolo
