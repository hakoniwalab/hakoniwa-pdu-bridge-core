#include "hakoniwa/pdu/bridge/bridge_builder.hpp"
#include "hakoniwa/time_source/time_source_factory.hpp"
#include <iostream>
#include <signal.h>
#include <memory>
#include <thread>
#include <charconv>
#include <cstring>
#include <system_error>

// Global pointer to the core for the signal handler
std::unique_ptr<hakoniwa::pdu::bridge::BridgeCore> g_core;

void signal_handler(int signum) {
    if (signum == SIGINT) {
        std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
        if (g_core) {
            g_core->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <path_to_bridge.json> <delta_time_step_usec> <path_to_endpoint_container.json> [node_name]" << std::endl;
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
    std::string node_name = (argc > 4) ? argv[4] : "node1";

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
    g_core = std::move(build_result.core);

    if (endpoint_container->start_all() != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to start all endpoints in EndpointContainer: " << endpoint_container->last_error() << std::endl;
        return 1;
    }

    g_core->start();

    std::cout << "Bridge core loaded for node " << node_name << ". Running... (Press Ctrl+C to stop)" << std::endl;
    while (g_core->cyclic_trigger()) {
        time_source->sleep_delta_time();
    }
    std::cout << "Bridge core stopped." << std::endl;

    return 0;
}
