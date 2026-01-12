#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource
#include <thread>
#include <chrono>

namespace hakoniwa::pdu::bridge {

BridgeCore::BridgeCore(const std::string& node_name, std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source) 
    : node_name_(node_name), is_running_(false), time_source_(time_source) {
    if (!time_source_) {
        throw std::runtime_error("BridgeCore: Time source cannot be null.");
    }
}

void BridgeCore::add_connection(std::unique_ptr<BridgeConnection> connection) {
    connections_.push_back(std::move(connection));
}

void BridgeCore::start() {
    if (is_running_.exchange(true)) {
        // Already running in another thread.
        return;
    }
}

bool BridgeCore::advance_timestep() {
    if (!is_running_) {
        // Not running, so do nothing.
        return false;
    }
    for (auto& connection : connections_) {
        connection->cyclic_trigger();
    }
    return true;
}

void BridgeCore::stop() {
    is_running_ = false;
}

} // namespace hakoniwa::pdu::bridge
