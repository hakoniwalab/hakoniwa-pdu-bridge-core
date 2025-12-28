#pragma once

#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/pdu/bridge/time_source/time_source.hpp" // Include for ITimeSource
#include <vector>
#include <memory>
#include <atomic>
#include <string>

namespace hako::pdu::bridge {

class BridgeCore {
public:
    BridgeCore(const std::string& node_name, std::shared_ptr<ITimeSource> time_source);
    ~BridgeCore() = default;

    void add_connection(std::unique_ptr<BridgeConnection> connection);

    // Starts the main execution loop. This is a blocking call.
    void run();

    // Stops the execution loop. This can be called from a different thread.
    void stop();

private:
    std::string node_name_;
    std::vector<std::unique_ptr<BridgeConnection>> connections_;
    std::atomic<bool> is_running_;
    std::shared_ptr<ITimeSource> time_source_;
};

} // namespace hako::pdu::bridge
