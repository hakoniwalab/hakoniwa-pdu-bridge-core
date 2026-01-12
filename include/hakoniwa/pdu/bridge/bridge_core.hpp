#pragma once

#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/pdu/endpoint_container.hpp"
#include "hakoniwa/time_source/time_source.hpp" // Include for ITimeSource
#include <vector>
#include <memory>
#include <atomic>
#include <string>

namespace hakoniwa::pdu::bridge {

class BridgeCore {
public:
    BridgeCore(const std::string& node_name, std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source, std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container);
    ~BridgeCore() = default;

    void add_connection(std::unique_ptr<BridgeConnection> connection);
    uint64_t get_delta_time_microseconds() const
    {
        return time_source_->get_delta_time_microseconds();
    }

    void start();

    bool is_running() const
    {
        if (!is_running_) {
            return false;
        }
        return true;
    }

    // Starts the main execution loop. This is a blocking call.
    bool advance_timestep();

    // Stops the execution loop. This can be called from a different thread.
    void stop();

private:
    std::string node_name_;
    std::vector<std::unique_ptr<BridgeConnection>> connections_;
    std::atomic<bool> is_running_;
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source_;
    std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container_;
    std::vector<std::string> endpoint_ids_;
};

} // namespace hakoniwa::pdu::bridge
