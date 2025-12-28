#include "hakoniwa/pdu/bridge/bridge_loader.hpp"
#include <iostream>
#include <signal.h>
#include <memory>
#include <thread>

// Global pointer to the core for the signal handler
std::unique_ptr<hako::pdu::bridge::BridgeCore> g_core;

void signal_handler(int signum) {
    if (signum == SIGINT) {
        std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
        if (g_core) {
            g_core->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_bridge.json> [node_name]" << std::endl;
        return 1;
    }
    signal(SIGINT, signal_handler);

    std::string config_path = argv[1];
    std::string node_name = (argc > 2) ? argv[2] : "node1";

    try {
        // Load the bridge core using the high-level factory method
        g_core = hako::pdu::bridge::BridgeLoader::create_bridge_from_config_file(config_path, node_name);

        std::cout << "Bridge core loaded for node " << node_name << ". Running... (Press Ctrl+C to stop)" << std::endl;
        g_core->run();
        std::cout << "Bridge core stopped." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
