#pragma once

#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/pdu/bridge/time_source/time_source.hpp" // Include for ITimeSource
#include <vector>
#include <memory>
#include <atomic>
#include <string>

namespace hakoniwa::pdu::bridge {

class BridgeCore {
public:
    BridgeCore(const std::string& node_name, std::shared_ptr<ITimeSource> time_source);
    ~BridgeCore() = default;

    void add_connection(std::unique_ptr<BridgeConnection> connection);
    void set_endpoint_maps(std::map<std::string, std::shared_ptr<hakoniwa::pdu::Endpoint>> endpoint)
    {
        endpoint_maps_ = std::move(endpoint);
    }
    uint64_t get_delta_time_microseconds() const
    {
        return time_source_->get_delta_time_microseconds();
    }

    void start();

    // Starts the main execution loop. This is a blocking call.
    bool advance_timestep();

    // Stops the execution loop. This can be called from a different thread.
    void stop();

private:
    std::string node_name_;
    std::map<std::string, std::shared_ptr<hakoniwa::pdu::Endpoint>>  endpoint_maps_;
    std::vector<std::unique_ptr<BridgeConnection>> connections_;
    std::atomic<bool> is_running_;
    std::shared_ptr<ITimeSource> time_source_;
};

} // namespace hakoniwa::pdu::bridge
