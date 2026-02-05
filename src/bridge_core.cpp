#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource
#include <thread>
#include <chrono>

namespace hakoniwa::pdu::bridge {

BridgeCore::BridgeCore(const std::string& node_name, std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source, std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container) 
    : node_name_(node_name), is_running_(false), time_source_(time_source), endpoint_container_(endpoint_container) {
    endpoint_ids_ = endpoint_container_->list_endpoint_ids();
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

bool BridgeCore::cyclic_trigger() {
    if (!is_running_) {
        #ifdef ENABLE_DEBUG_MESSAGES
        std::cout << "DEBUG: BridgeCore not running, skipping cyclic_trigger." << std::endl;
        #endif
        // Not running, so do nothing.
        return false;
    }
    // Trigger recv events for hakoniwa polling shm endpoints
    for (const auto& endpoint_id : endpoint_ids_) {
        auto endpoint = endpoint_container_->ref(endpoint_id);
        if (endpoint) {
            endpoint->process_recv_events();
        }
    }
    // Trigger cyclic transfers
    #ifdef ENABLE_DEBUG_MESSAGES
    std::cout << "connections_ size: " << connections_.size() << std::endl;
    #endif
    for (auto& connection : connections_) {
        connection->cyclic_trigger();
    }
    #ifdef ENABLE_DEBUG_MESSAGES
    std::cout << "DEBUG: BridgeCore cyclic_trigger completed." << std::endl;
    #endif
    return true;
}

void BridgeCore::stop() {
    is_running_ = false;
}

bool BridgeCore::set_connection_active(const std::string& connection_id, bool is_active) {
    for (auto& connection : connections_) {
        if (connection->getConnectionId() == connection_id) {
            connection->set_active(is_active);
            return true;
        }
    }
    return false;
}

bool BridgeCore::get_connection_epoch(const std::string& connection_id, uint8_t& out_epoch) const {
    for (const auto& connection : connections_) {
        if (connection->getConnectionId() == connection_id) {
            out_epoch = connection->get_epoch();
            return true;
        }
    }
    return false;
}

bool BridgeCore::increment_connection_epoch(const std::string& connection_id) {
    for (auto& connection : connections_) {
        if (connection->getConnectionId() == connection_id) {
            connection->increment_epoch();
            return true;
        }
    }
    return false;
}

} // namespace hakoniwa::pdu::bridge
