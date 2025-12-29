#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/bridge/time_source/time_source.hpp" // For ITimeSource
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

void BridgeCore::start() {
    if (is_running_.exchange(true)) {
        // Already running in another thread.
        return;
    }
    for (auto& endpoint: endpoint_maps_) {
        HakoPduErrorType err = endpoint.second->start();
        if (err != HAKO_PDU_ERR_OK) {
            throw std::runtime_error("Failed to start endpoint: " + endpoint.first);
        }
    }
}

bool BridgeCore::advance_timestep() {
    if (!is_running_) {
        // Not running, so do nothing.
        return false;
    }
    for (auto& connection : connections_) {
        connection->step(time_source_);
    }
    time_source_->sleep_delta_time();
    return true;
}

void BridgeCore::stop() {
    is_running_ = false;
}

} // namespace hako::pdu::bridge
