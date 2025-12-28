#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/bridge/time_source.hpp" // For ITimeSource
#include <thread>
#include <chrono>

namespace hako::pdu::bridge {

BridgeCore::BridgeCore(const std::string& node_name, std::shared_ptr<ITimeSource> time_source) 
    : node_name_(node_name), is_running_(false), time_source_(time_source) {
    if (!time_source_) {
        throw std::runtime_error("BridgeCore: Time source cannot be null.");
    }
}

void BridgeCore::add_connection(std::unique_ptr<BridgeConnection> connection) {
    connections_.push_back(std::move(connection));
}

void BridgeCore::run() {
    if (is_running_.exchange(true)) {
        // Already running in another thread.
        return;
    }

    while (is_running_) {
        // Get time from the abstract time source

        for (auto& connection : connections_) {
            if (connection->getNodeId() == node_name_) {
                connection->step(time_source_);
            }
        }

        // The sleep duration determines the resolution of the bridge.
        // For ticker policies, this should be small enough not to miss ticks.
        // 1 millisecond is a common choice for high-resolution timers.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void BridgeCore::stop() {
    is_running_ = false;
}

} // namespace hako::pdu::bridge
