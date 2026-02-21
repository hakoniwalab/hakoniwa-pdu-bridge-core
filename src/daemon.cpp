#include "hakoniwa/pdu/bridge/bridge_builder.hpp"
#include "hakoniwa/pdu/bridge/bridge_monitor_runtime.hpp"
#include "hakoniwa/time_source/time_source_factory.hpp"
#include <iostream>
#include <signal.h>
#include <memory>
#include <thread>
#include <charconv>
#include <cstring>
#include <system_error>
#include <atomic>
#include <streambuf>

// Global pointer to the core for the signal handler
std::shared_ptr<hakoniwa::pdu::bridge::BridgeCore> g_core;
std::atomic<bool> g_stop_requested{false};

class LinePrefixFilterBuf : public std::streambuf {
public:
    LinePrefixFilterBuf(std::streambuf* dest, std::string drop_prefix)
        : dest_(dest), drop_prefix_(std::move(drop_prefix))
    {
    }

protected:
    int overflow(int ch) override
    {
        if (ch == traits_type::eof()) {
            return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
        }
        buffer_.push_back(static_cast<char>(ch));
        if (ch == '\n') {
            flush_line_();
        }
        return ch;
    }

    int sync() override
    {
        if (!buffer_.empty()) {
            flush_line_();
        }
        return dest_->pubsync();
    }

private:
    void flush_line_()
    {
        if (buffer_.rfind(drop_prefix_, 0) != 0) {
            (void)dest_->sputn(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        }
        buffer_.clear();
    }

    std::streambuf* dest_;
    std::string drop_prefix_;
    std::string buffer_;
};

void signal_handler(int signum) {
    if (signum == SIGINT) {
        g_stop_requested.store(true, std::memory_order_relaxed);
    }
}

int main(int argc, char* argv[]) {
    static LinePrefixFilterBuf debug_filter(std::cout.rdbuf(), "DEBUG:");
    std::cout.rdbuf(&debug_filter);

    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <path_to_bridge.json> <delta_time_step_usec> <path_to_endpoint_container.json> [node_name] "
                  << "[--enable-ondemand --ondemand-mux-config <path_to_endpoint_mux.json>]"
                  << " (on-demand subscribe default policy: throttle interval_ms=100; filters: omitted/empty only)"
                  << std::endl;
        return 1;
    }
    signal(SIGINT, signal_handler);

    std::string config_path = argv[1];
    uint64_t delta_time_step_usec = 0;
    auto delta_parse = std::from_chars(argv[2], argv[2] + std::strlen(argv[2]), delta_time_step_usec);
    if (delta_parse.ec != std::errc()) {
        std::cerr << "Invalid delta_time_step_usec: " << argv[2] << std::endl;
        return 1;
    }
    std::string endpoint_container_path = argv[3];
    std::string node_name = "node1";
    bool enable_ondemand = false;
    std::string ondemand_mux_config_path;

    for (int i = 4; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--enable-ondemand") {
            enable_ondemand = true;
            continue;
        }
        if (arg == "--ondemand-mux-config") {
            if ((i + 1) >= argc) {
                std::cerr << "--ondemand-mux-config requires a path" << std::endl;
                return 1;
            }
            ondemand_mux_config_path = argv[++i];
            continue;
        }
        if (!arg.empty() && arg[0] != '-' && node_name == "node1") {
            node_name = arg;
            continue;
        }
    }
    if (enable_ondemand && ondemand_mux_config_path.empty()) {
        std::cerr << "On-demand mode requires --ondemand-mux-config" << std::endl;
        return 1;
    }

    std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container =
        std::make_shared<hakoniwa::pdu::EndpointContainer>(node_name, endpoint_container_path);
    HakoPduErrorType init_ret = endpoint_container->initialize();
    if (init_ret != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to initialize EndpointContainer: " << endpoint_container->last_error() << std::endl;
        return 1;
    }
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source = hakoniwa::time_source::create_time_source("real", delta_time_step_usec);

    // Load the bridge core using the high-level factory method
    auto build_result = hakoniwa::pdu::bridge::build(config_path, node_name, time_source, endpoint_container);
    if (!build_result.ok()) {
        std::cerr << "Bridge build failed: " << build_result.error_message << std::endl;
        return 1;
    }
    g_core = std::shared_ptr<hakoniwa::pdu::bridge::BridgeCore>(std::move(build_result.core));

    if (endpoint_container->start_all() != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to start all endpoints in EndpointContainer: " << endpoint_container->last_error() << std::endl;
        return 1;
    }

    g_core->start();

    std::shared_ptr<hakoniwa::pdu::bridge::BridgeMonitorRuntime> monitor_runtime;
    if (enable_ondemand) {
        monitor_runtime = std::make_shared<hakoniwa::pdu::bridge::BridgeMonitorRuntime>(g_core);
        hakoniwa::pdu::bridge::BridgeMonitorRuntimeOptions runtime_options;
        runtime_options.enable_ondemand = enable_ondemand;
        runtime_options.ondemand_mux_config_path = ondemand_mux_config_path;
        auto runtime_init_err = monitor_runtime->initialize(runtime_options);
        if (runtime_init_err != HAKO_PDU_ERR_OK) {
            std::cerr << "Bridge monitor runtime init failed: " << static_cast<int>(runtime_init_err) << std::endl;
            return 1;
        }
        g_core->attach_monitor_runtime(monitor_runtime);
    }

    std::cout << "Bridge core loaded for node " << node_name << ". Running... (Press Ctrl+C to stop)" << std::endl;
    while (g_core->cyclic_trigger()) {
        if (g_stop_requested.exchange(false, std::memory_order_relaxed)) {
            std::cout << "Interrupt signal received. Stopping bridge core..." << std::endl;
            g_core->stop();
            continue;
        }
        time_source->sleep_delta_time();
    }
    g_core->detach_monitor_runtime();
    std::cout << "Bridge core stopped." << std::endl;

    return 0;
}
