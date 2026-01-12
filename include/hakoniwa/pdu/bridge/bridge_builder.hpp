#pragma once

#include "hakoniwa/pdu/bridge/bridge_types.hpp" // For BridgeConfig
#include "hakoniwa/pdu/endpoint_container.hpp"
#include "hakoniwa/pdu/bridge/bridge_build_result.hpp"
#include <string>
#include <memory>
#include <map>

namespace hakoniwa::pdu::bridge {

/*
 * just simply, loads the config file and converts to BridgeConfig
 */
std::optional<BridgeConfig> parse(const std::string& config_file_path, std::string& error_message);

BridgeBuildResult build(const std::string& config_file_path, 
    const std::string& node_name, std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source,
    std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container);

} // namespace hakoniwa::pdu::bridge
