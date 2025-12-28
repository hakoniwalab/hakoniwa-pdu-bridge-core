#include "hakoniwa/pdu/bridge/bridge_loader.hpp"
#include "hakoniwa/pdu/bridge/bridge_types.hpp"
#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/pdu/bridge/policy/throttle_policy.hpp"
#include "hakoniwa/pdu/bridge/policy/ticker_policy.hpp"
#include "hakoniwa/pdu/bridge/real_time_source.hpp"    // For RealTimeSource
#include "hakoniwa/pdu/bridge/virtual_time_source.hpp" // For VirtualTimeSource
#include "hakoniwa/pdu/endpoint.hpp"          // Actual Endpoint class
#include "hakoniwa/pdu/pdu_definition.hpp"    // For PduDefinition

#include <nlohmann/json.hpp> // nlohmann/json

#include <fstream>
#include <stdexcept>

namespace hako::pdu::bridge {

    using json = nlohmann::json;

    // Helper to parse BridgeConfig from file
    static BridgeConfig parse_bridge_config_from_file(const std::string& config_path) {
        std::ifstream ifs(config_path);
        if (!ifs.is_open()) {
            throw std::runtime_error("BridgeLoader: Failed to open bridge config file: " + config_path);
        }
        nlohmann::json j;
        ifs >> j;
        return j.get<BridgeConfig>();
    }

    BridgeConfig BridgeLoader::load_config(const std::string& config_file_path)
    {
        return parse_bridge_config_from_file(config_file_path);
    }

    std::unique_ptr<BridgeCore> BridgeLoader::create_bridge_from_config_file(
        const std::string& config_file_path,
        const std::string& node_name)
    {
        // 1. Load BridgeConfig
        auto bridge_config = parse_bridge_config_from_file(config_file_path);

        // 2. Prepare PDU definitions (mocked for now)
        std::map<std::string, std::shared_ptr<hakoniwa::pdu::PduDefinition>> pdu_definitions_map;
        auto pdu_def_node = std::make_shared<hakoniwa::pdu::PduDefinition>();
        // Populate with example data if necessary
        pdu_def_node->pdu_definitions_["Drone"]["pos"] = {"sensor_msgs/Twist", "pos", "pos", 1, 32, "shm"};
        pdu_def_node->pdu_definitions_["Drone"]["motor"] = {"std_msgs/Float64", "motor", "motor", 2, 16, "shm"};
        pdu_def_node->pdu_definitions_["Drone"]["hako_camera_data"] = {"sensor_msgs/Image", "hako_camera_data", "hako_camera_data", 3, 65536, "shm"};
        pdu_definitions_map[node_name] = pdu_def_node;

        // 3. Create and cache Endpoint instances
        std::map<std::string, std::shared_ptr<hakoniwa::pdu::Endpoint>> shared_endpoints_by_path;
        std::map<std::string, std::shared_ptr<hakoniwa::pdu::Endpoint>> named_endpoints_by_id;

        for (const auto& node_eps : bridge_config.endpoints) {
            if (node_eps.nodeId == node_name) {
                for (const auto& ep_def : node_eps.endpoints) {
                    if (shared_endpoints_by_path.find(ep_def.config_path) == shared_endpoints_by_path.end()) {
                        auto endpoint = std::make_shared<hakoniwa::pdu::Endpoint>(ep_def.id, HAKO_PDU_ENDPOINT_DIRECTION_INOUT);
                        HakoPduErrorType err = endpoint->open(ep_def.config_path);
                        if (err != HAKO_PDU_ERR_OK) {
                            throw std::runtime_error("BridgeLoader: Failed to open endpoint config " + ep_def.config_path + ": " + std::to_string(err));
                        }
                        shared_endpoints_by_path[ep_def.config_path] = endpoint;
                    }
                    named_endpoints_by_id[ep_def.id] = shared_endpoints_by_path[ep_def.config_path];
                }
            }
        }

        // 4. Load the bridge core using the low-level load function
        return BridgeLoader::load(bridge_config, node_name, named_endpoints_by_id, pdu_definitions_map);
    }


    std::unique_ptr<BridgeCore> BridgeLoader::load(
        const BridgeConfig& config, // Now takes BridgeConfig directly
        const std::string& node_name,
        const std::map<std::string, std::shared_ptr<hakoniwa::pdu::Endpoint>>& endpoints,
        const std::map<std::string, std::shared_ptr<hakoniwa::pdu::PduDefinition>>& pdu_definitions)
    {
        // Instantiate the time source
        std::shared_ptr<ITimeSource> time_source;
        if (config.time_source_type == "real") {
            time_source = std::make_shared<RealTimeSource>();
        } else if (config.time_source_type == "virtual") {
            time_source = std::make_shared<VirtualTimeSource>();
        } else {
            throw std::runtime_error("Unknown time_source_type: " + config.time_source_type);
        }

        auto core = std::make_unique<BridgeCore>(node_name, time_source); // Pass time_source to BridgeCore

        // 1. Instantiate policies
        std::map<std::string, std::shared_ptr<IPduTransferPolicy>> policy_map;
        for (const auto& pair : config.transferPolicies) {
            const auto& id = pair.first;
            const auto& policy_def = pair.second;
            if (policy_def.type == "immediate") {
                policy_map[id] = std::make_shared<ImmediatePolicy>();
            } else if (policy_def.type == "throttle") {
                if (!policy_def.intervalMs) throw std::runtime_error("throttle policy needs intervalMs");
                policy_map[id] = std::make_shared<ThrottlePolicy>(static_cast<uint64_t>(*policy_def.intervalMs) * 1000); // Convert to microseconds
            } else if (policy_def.type == "ticker") {
                if (!policy_def.intervalMs) throw std::runtime_error("ticker policy needs intervalMs");
                policy_map[id] = std::make_shared<TickerPolicy>(static_cast<uint64_t>(*policy_def.intervalMs) * 1000); // Convert to microseconds
            }
        }

        // 2. Create Connections and TransferPdus
        for (const auto& conn_def : config.connections) {
            if (conn_def.nodeId != node_name) {
                continue; // Skip connections not intended for this node
            }
            auto connection = std::make_unique<BridgeConnection>(conn_def.nodeId);
            
            auto src_ep_it = endpoints.find(conn_def.source.endpointId);
            if (src_ep_it == endpoints.end()) throw std::runtime_error("Source endpoint not found: " + conn_def.source.endpointId);
            std::shared_ptr<hakoniwa::pdu::Endpoint> src_ep = src_ep_it->second;
            
            // PduDefinition is no longer passed to TransferPdu directly.
            // It's expected to be managed by the Endpoint itself.
            // auto pdu_def_it = pdu_definitions.find(conn_def.nodeId);
            // if (pdu_def_it == pdu_definitions.end()) throw std::runtime_error("PduDefinition not found for node: " + conn_def.nodeId);
            // std::shared_ptr<hakoniwa::pdu::PduDefinition> pdu_def = pdu_def_it->second;

            for (const auto& dest_def : conn_def.destinations) {
                auto dst_ep_it = endpoints.find(dest_def.endpointId);
                if (dst_ep_it == endpoints.end()) throw std::runtime_error("Destination endpoint not found: " + dest_def.endpointId);
                std::shared_ptr<hakoniwa::pdu::Endpoint> dst_ep = dst_ep_it->second;

                for (const auto& trans_pdu_def : conn_def.transferPdus) {
                    auto policy = policy_map.at(trans_pdu_def.policyId);
                    const auto& pdu_keys = config.pduKeyGroups.at(trans_pdu_def.pduKeyGroupId);

                    for (const auto& pdu_key_def : pdu_keys) {
                        auto transfer_pdu = std::make_unique<TransferPdu>(pdu_key_def, policy, src_ep, dst_ep);
                        connection->add_transfer_pdu(std::move(transfer_pdu));
                    }
                }
            }
            core->add_connection(std::move(connection));
        }

        return core;
    }

}
