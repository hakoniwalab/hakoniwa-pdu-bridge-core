#pragma once

#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/bridge/bridge_types.hpp" // For BridgeConfig
#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/pdu_definition.hpp"
#include <string>
#include <memory>
#include <map>

namespace hakoniwa::pdu::bridge {

/*
 * just simply, loads the config file and converts to BridgeConfig
 */
BridgeConfig parse(const std::string& config_file_path);

std::unique_ptr<BridgeCore> build(const std::string& config_file_path, const std::string& node_name, uint64_t delta_time_step_usec);

} // namespace hakoniwa::pdu::bridge
