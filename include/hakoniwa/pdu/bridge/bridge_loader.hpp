#pragma once

#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/bridge/bridge_types.hpp" // For BridgeConfig
#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/pdu_definition.hpp"
#include <string>
#include <memory>
#include <map>

namespace hako::pdu::bridge {

class BridgeLoader {
public:
    static BridgeConfig load_config(const std::string& config_file_path);
    // Loads the config from a file, uses the provided endpoints and PDU definitions,
    // and returns a fully constructed BridgeCore for the specified node.
    static std::unique_ptr<BridgeCore> load(
        const BridgeConfig& config, // Changed from config_path
        const std::string& node_name,
        const std::map<std::string, std::shared_ptr<hakoniwa::pdu::Endpoint>>& endpoints,
        const std::map<std::string, std::shared_ptr<hakoniwa::pdu::PduDefinition>>& pdu_definitions
    );
    // Creates a bridge from a config file, handling endpoint creation and sharing.
    static std::unique_ptr<BridgeCore> create_bridge_from_config_file(
        const std::string& config_file_path,
        const std::string& node_name
    );
};

} // namespace hako::pdu::bridge
