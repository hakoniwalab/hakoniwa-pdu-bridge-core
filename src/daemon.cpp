#include "hakoniwa/pdu/bridge/bridge_builder.hpp"
#include <iostream>
#include <signal.h>
#include <memory>
#include <thread>

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
    uint64_t delta_time_step_usec = std::stoull(argv[2]);
    std::string endpoint_container_path = argv[3];
    std::string node_name = (argc > 4) ? argv[4] : "node1";

    try {
        std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container = 
            std::make_shared<hakoniwa::pdu::EndpointContainer>(node_name, endpoint_container_path);
        HakoPduErrorType init_ret = endpoint_container->initialize();
        if (init_ret != HAKO_PDU_ERR_OK) {
            throw std::runtime_error("Failed to initialize EndpointContainer: " + endpoint_container->last_error());
        }

        // Load the bridge core using the high-level factory method
        g_core = std::move(hakoniwa::pdu::bridge::build(config_path, node_name, delta_time_step_usec, endpoint_container).core);

        if (endpoint_container->start_all() != HAKO_PDU_ERR_OK) {
            throw std::runtime_error("Failed to start all endpoints in EndpointContainer: " + endpoint_container->last_error());
        }

        g_core->start();

        std::cout << "Bridge core loaded for node " << node_name << ". Running... (Press Ctrl+C to stop)" << std::endl;
        while (g_core->advance_timestep()) {
        }
        std::cout << "Bridge core stopped." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
