#include "hakoniwa/pdu/bridge/bridge_loader.hpp"
#include "hakoniwa/pdu/bridge/bridge_types.hpp"
#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/pdu/bridge/policy/throttle_policy.hpp"
#include "hakoniwa/pdu/bridge/policy/ticker_policy.hpp"
#include "hakoniwa/pdu/bridge/time_source/real_time_source.hpp"    // For RealTimeSource
#include "hakoniwa/pdu/bridge/time_source/virtual_time_source.hpp" // For VirtualTimeSource
#include "hakoniwa/pdu/bridge/time_source/hakoniwa_time_source.hpp" // For HakoniwaTimeSource
#include "hakoniwa/pdu/endpoint.hpp"          // Actual Endpoint class
#include "hakoniwa/pdu/pdu_definition.hpp"    // For PduDefinition

#include <nlohmann/json.hpp> // nlohmann/json

#include <fstream>
#include <stdexcept>

namespace hako::pdu::bridge {

    using json = nlohmann::json;

    // Helper to parse BridgeConfig from file
    static BridgeConfig parse(const std::string& config_path) {
        std::ifstream ifs(config_path);
        if (!ifs.is_open()) {
            throw std::runtime_error("BridgeLoader: Failed to open bridge config file: " + config_path);
        }
        nlohmann::json j;
        ifs >> j;
        return j.get<BridgeConfig>();
    }
    static std::unique_ptr<BridgeCore> build(const std::string& config_file_path, const std::string& node_name)
    {
        BridgeConfig bridge_config = parse(config_file_path);

        std::shared_ptr<ITimeSource> time_source;
        if (bridge_config.time_source_type == "real") {
            time_source = std::make_shared<RealTimeSource>();
        } else if (bridge_config.time_source_type == "virtual") {
            time_source = std::make_shared<VirtualTimeSource>();
        } else {
            throw std::runtime_error("BridgeLoader: Unknown time source type: " + bridge_config.time_source_type);
        }

        std::unique_ptr<BridgeCore> bridge_core = std::make_unique<BridgeCore>(
            node_name,
            time_source
        );
        return bridge_core;
    }

}
