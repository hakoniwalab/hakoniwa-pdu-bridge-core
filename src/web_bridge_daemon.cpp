#include "hakoniwa/pdu/bridge/bridge_builder.hpp"
#include "hakoniwa/pdu/bridge/bridge_monitor_runtime.hpp"
#include "hakoniwa/pdu/endpoint_container.hpp"
#include "hakoniwa/time_source/time_source_factory.hpp"
#include "hako_asset.h"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <signal.h>
#include <vector>
#include <string>
#include <system_error>

namespace {

void log_info(const std::string& message)
{
    std::cout << "[web_bridge][info] " << message << std::endl;
}

void log_error(const std::string& message)
{
    std::cerr << "[web_bridge][error] " << message << std::endl;
}

struct WebBridgeDaemonOptions {
    std::string config_root_path;
    std::string bridge_config_path;
    std::string endpoint_container_path;
    std::string asset_config_path;
    std::string ondemand_mux_config_path;
    std::string node_name;
    std::string asset_name;
    uint64_t delta_time_step_usec{20000};
    bool enable_real_sleep{true};
    bool enable_ondemand{false};
};

std::atomic<bool> g_stop_requested{false};
WebBridgeDaemonOptions g_options;
std::shared_ptr<hakoniwa::pdu::EndpointContainer> g_endpoint_container;
std::shared_ptr<hakoniwa::pdu::bridge::BridgeCore> g_core;
std::shared_ptr<hakoniwa::pdu::bridge::BridgeMonitorRuntime> g_monitor_runtime;
std::shared_ptr<hakoniwa::time_source::ITimeSource> g_bridge_time_source;
std::shared_ptr<hakoniwa::time_source::ITimeSource> g_real_sleep_time_source;

void signal_handler(int signum)
{
    if (signum == SIGINT) {
        g_stop_requested.store(true, std::memory_order_relaxed);
    }
}

std::string make_default_config_path(const std::string& relative_path)
{
    return (std::filesystem::current_path() / relative_path).lexically_normal().string();
}

std::string make_config_rooted_path(const std::string& config_root_path, const std::string& relative_path)
{
    return (std::filesystem::path(config_root_path) / relative_path).lexically_normal().string();
}

std::string resolve_default_asset_config_path(const std::string& config_root_path)
{
    namespace fs = std::filesystem;
    const fs::path pdu_dir = fs::path(config_root_path) / "pdu";
    const std::vector<std::string> preferred = {
        "drone-pdudef.json",
        "drone-visual-state.json"
    };

    for (const auto& filename : preferred) {
        const fs::path candidate = pdu_dir / filename;
        if (fs::exists(candidate)) {
            return candidate.lexically_normal().string();
        }
    }

    if (fs::exists(pdu_dir) && fs::is_directory(pdu_dir)) {
        std::vector<fs::path> candidates;
        for (const auto& entry : fs::directory_iterator(pdu_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto ext = entry.path().extension().string();
            if (ext != ".json") {
                continue;
            }
            const auto filename = entry.path().filename().string();
            if (filename.find("pdutypes") != std::string::npos) {
                continue;
            }
            candidates.push_back(entry.path());
        }
        std::sort(candidates.begin(), candidates.end());
        if (!candidates.empty()) {
            return candidates.front().lexically_normal().string();
        }
    }

    return make_config_rooted_path(config_root_path, "pdu/drone-pdudef.json");
}

void apply_config_root(WebBridgeDaemonOptions& options)
{
    options.bridge_config_path = make_config_rooted_path(options.config_root_path, "bridge/bridge.json");
    options.endpoint_container_path = make_config_rooted_path(options.config_root_path, "endpoint/endpoint_container.json");
    options.asset_config_path = resolve_default_asset_config_path(options.config_root_path);
    options.ondemand_mux_config_path = make_config_rooted_path(options.config_root_path, "monitor/mux_endpoint.json");
}

void print_usage(const char* argv0)
{
    std::cerr
        << "Usage: " << argv0
        << " [--config-root <path>]"
        << " [--bridge-config <path>]"
        << " [--endpoint-container <path>]"
        << " [--asset-config <path>]"
        << " [--enable-ondemand]"
        << " [--ondemand-mux-config <path>]"
        << " [--node-name <name>]"
        << " [--asset-name <name>]"
        << " [--delta-time-step-usec <usec>]"
        << " [--disable-real-sleep]"
        << std::endl;
}

bool parse_uint64_arg(const char* value, uint64_t& out_value)
{
    const auto parse_result = std::from_chars(value, value + std::strlen(value), out_value);
    return parse_result.ec == std::errc();
}

bool parse_args(int argc, char* argv[], WebBridgeDaemonOptions& options)
{
    options.config_root_path = make_default_config_path("config/web_bridge");
    apply_config_root(options);
    options.node_name = "web_bridge_node1";
    options.asset_name = "WebBridge";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config-root") {
            if ((i + 1) >= argc) {
                std::cerr << "--config-root requires a path" << std::endl;
                return false;
            }
            options.config_root_path = argv[++i];
            apply_config_root(options);
            continue;
        }
        if (arg == "--bridge-config") {
            if ((i + 1) >= argc) {
                std::cerr << "--bridge-config requires a path" << std::endl;
                return false;
            }
            options.bridge_config_path = argv[++i];
            continue;
        }
        if (arg == "--endpoint-container") {
            if ((i + 1) >= argc) {
                std::cerr << "--endpoint-container requires a path" << std::endl;
                return false;
            }
            options.endpoint_container_path = argv[++i];
            continue;
        }
        if (arg == "--asset-config") {
            if ((i + 1) >= argc) {
                std::cerr << "--asset-config requires a path" << std::endl;
                return false;
            }
            options.asset_config_path = argv[++i];
            continue;
        }
        if (arg == "--enable-ondemand") {
            options.enable_ondemand = true;
            continue;
        }
        if (arg == "--ondemand-mux-config") {
            if ((i + 1) >= argc) {
                std::cerr << "--ondemand-mux-config requires a path" << std::endl;
                return false;
            }
            options.ondemand_mux_config_path = argv[++i];
            continue;
        }
        if (arg == "--node-name") {
            if ((i + 1) >= argc) {
                std::cerr << "--node-name requires a value" << std::endl;
                return false;
            }
            options.node_name = argv[++i];
            continue;
        }
        if (arg == "--asset-name") {
            if ((i + 1) >= argc) {
                std::cerr << "--asset-name requires a value" << std::endl;
                return false;
            }
            options.asset_name = argv[++i];
            continue;
        }
        if (arg == "--delta-time-step-usec") {
            if ((i + 1) >= argc) {
                std::cerr << "--delta-time-step-usec requires a value" << std::endl;
                return false;
            }
            if (!parse_uint64_arg(argv[++i], options.delta_time_step_usec)) {
                std::cerr << "Invalid delta_time_step_usec: " << argv[i] << std::endl;
                return false;
            }
            continue;
        }
        if (arg == "--disable-real-sleep") {
            options.enable_real_sleep = false;
            continue;
        }

        std::cerr << "Unknown argument: " << arg << std::endl;
        return false;
    }
    if (options.enable_ondemand && options.ondemand_mux_config_path.empty()) {
        std::cerr << "--enable-ondemand requires --ondemand-mux-config" << std::endl;
        return false;
    }
    return true;
}

int bridge_on_initialize(hako_asset_context_t*)
{
    log_info("initializing endpoint container");
    g_endpoint_container = std::make_shared<hakoniwa::pdu::EndpointContainer>(
        g_options.node_name,
        g_options.endpoint_container_path);
    HakoPduErrorType init_ret = g_endpoint_container->initialize();
    if (init_ret != HAKO_PDU_ERR_OK) {
        log_error("failed to initialize EndpointContainer: " + g_endpoint_container->last_error());
        return 1;
    }

    if (g_endpoint_container->start_all() != HAKO_PDU_ERR_OK) {
        log_error("failed to start endpoints: " + g_endpoint_container->last_error());
        return 1;
    }
    if (g_endpoint_container->post_start_all() != HAKO_PDU_ERR_OK) {
        log_error("failed to post-start endpoints: " + g_endpoint_container->last_error());
        return 1;
    }

    log_info("building bridge core");
    g_bridge_time_source = hakoniwa::time_source::create_time_source("hakoniwa_callback", g_options.delta_time_step_usec);
    g_real_sleep_time_source = hakoniwa::time_source::create_time_source("real", g_options.delta_time_step_usec);
    auto build_result = hakoniwa::pdu::bridge::build(
        g_options.bridge_config_path,
        g_options.node_name,
        g_bridge_time_source,
        g_endpoint_container);
    if (!build_result.ok()) {
        log_error("bridge build failed: " + build_result.error_message);
        return 1;
    }

    g_core = std::shared_ptr<hakoniwa::pdu::bridge::BridgeCore>(std::move(build_result.core));
    g_core->start();

    if (g_options.enable_ondemand) {
        g_monitor_runtime = std::make_shared<hakoniwa::pdu::bridge::BridgeMonitorRuntime>(g_core);
        hakoniwa::pdu::bridge::BridgeMonitorRuntimeOptions runtime_options;
        runtime_options.enable_ondemand = true;
        runtime_options.ondemand_mux_config_path = g_options.ondemand_mux_config_path;
        const HakoPduErrorType runtime_init_err = g_monitor_runtime->initialize(runtime_options);
        if (runtime_init_err != HAKO_PDU_ERR_OK) {
            log_error("bridge monitor runtime init failed: " + std::to_string(static_cast<int>(runtime_init_err)));
            return 1;
        }
        g_core->attach_monitor_runtime(g_monitor_runtime);
    }

    log_info(
        "initialized asset=" + g_options.asset_name
        + " node=" + g_options.node_name
        + " ws=ws://127.0.0.1:8765"
        + " bridge_time_source=hakoniwa_callback"
        + " real_sleep=" + std::string(g_options.enable_real_sleep ? "on" : "off"));
    return 0;
}

int bridge_on_simulation_step(hako_asset_context_t*)
{
    if (!g_core || !g_bridge_time_source) {
        log_error("runtime is not initialized");
        return 1;
    }

    if (g_stop_requested.exchange(false, std::memory_order_relaxed)) {
        log_info("interrupt signal received, stopping bridge core");
        g_core->stop();
    }
    const bool running = g_core->cyclic_trigger();
    if (!running) {
        log_info("bridge core is not running");
        return 0;
    }
    if (g_options.enable_real_sleep && g_real_sleep_time_source) {
        g_real_sleep_time_source->sleep_delta_time();
    }
    return 0;
}

int bridge_on_reset(hako_asset_context_t*)
{
    log_info("reset requested");
    if (g_core) {
        g_core->detach_monitor_runtime();
        g_core->stop();
    }
    if (g_endpoint_container) {
        (void)g_endpoint_container->stop_all();
    }
    g_core.reset();
    g_monitor_runtime.reset();
    g_bridge_time_source.reset();
    g_real_sleep_time_source.reset();
    g_endpoint_container.reset();
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    if (!parse_args(argc, argv, g_options)) {
        print_usage(argv[0]);
        return 1;
    }
    signal(SIGINT, signal_handler);

    hako_asset_callbacks_t callbacks{};
    callbacks.on_initialize = bridge_on_initialize;
    callbacks.on_manual_timing_control = nullptr;
    callbacks.on_simulation_step = bridge_on_simulation_step;
    callbacks.on_reset = bridge_on_reset;

    const int register_ret = hako_asset_register(
        g_options.asset_name.c_str(),
        g_options.asset_config_path.c_str(),
        &callbacks,
        g_options.delta_time_step_usec,
        HAKO_ASSET_MODEL_CONTROLLER);
    if (register_ret != 0) {
        log_error("hako_asset_register() failed: " + std::to_string(register_ret));
        return 1;
    }

    log_info("starting hakoniwa asset runtime");
    const int start_ret = hako_asset_start();
    if (start_ret != 0) {
        log_error("hako_asset_start() returned: " + std::to_string(start_ret));
        return 1;
    }
    if (g_core) {
        g_core->detach_monitor_runtime();
    }
    if (g_endpoint_container) {
        const HakoPduErrorType stop_ret = g_endpoint_container->stop_all();
        if (stop_ret != HAKO_PDU_ERR_OK) {
            log_error("failed to stop endpoints in EndpointContainer: " + g_endpoint_container->last_error());
            return 1;
        }
    }
    log_info("web bridge stopped");
    return 0;
}
