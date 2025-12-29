#include "hakoniwa/pdu/bridge/bridge_types.hpp"
#include "hakoniwa/pdu/bridge/bridge_core.hpp"
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
#include <filesystem>
namespace fs = std::filesystem;

namespace hako::pdu::bridge {

    using json = nlohmann::json;
    fs::path resolve_under_base(const fs::path& base_dir, const std::string& maybe_rel)
    {
        fs::path p(maybe_rel);
        if (p.is_absolute()) {
            return p.lexically_normal();
        }
        return (base_dir / p).lexically_normal();
    }

    // Helper to parse BridgeConfig from file
    BridgeConfig parse(const std::string& config_path) {
        std::ifstream ifs(config_path);
        if (!ifs.is_open()) {
            throw std::runtime_error("BridgeLoader: Failed to open bridge config file: " + config_path);
        }
        nlohmann::json j;
        ifs >> j;
        return j.get<BridgeConfig>();
    }
    std::unique_ptr<BridgeCore> build(const std::string& config_file_path, const std::string& node_name)
    {
        fs::path bridge_path(config_file_path);
        fs::path base_dir = bridge_path.parent_path();

        BridgeConfig bridge_config = parse(config_file_path);

        /*
         * time source selection
         */
        std::shared_ptr<ITimeSource> time_source;
        if (bridge_config.time_source_type == "real") {
            time_source = std::make_shared<RealTimeSource>();
        } else if (bridge_config.time_source_type == "virtual") {
            time_source = std::make_shared<VirtualTimeSource>();
        } else if (bridge_config.time_source_type == "hakoniwa") {
            time_source = std::make_shared<HakoniwaTimeSource>();
        } else {
            throw std::runtime_error("BridgeLoader: Unknown time source type: " + bridge_config.time_source_type);
        }
        /*
         * bridge core creation
         */
        std::unique_ptr<BridgeCore> core = std::make_unique<BridgeCore>(
            node_name,
            time_source
        );

        /*
         * transfer policy section
         */
        std::map<std::string, std::shared_ptr<IPduTransferPolicy>> policy_map;
        for (const auto& pair : bridge_config.transferPolicies) {
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
            else {
                throw std::runtime_error("BridgeLoader: Unknown transfer policy type: " + policy_def.type);
            }
        }

        /*
         * endpoint section
         */
        std::map<std::string, std::shared_ptr<hakoniwa::pdu::Endpoint>> endpoints_by_id;

        for (const auto& node_eps : bridge_config.endpoints) {
            if (node_eps.nodeId != node_name) continue;

            for (const auto& ep_def : node_eps.endpoints) {
                // Safety: disallow duplicate IDs
                if (endpoints_by_id.contains(ep_def.id)) {
                    throw std::runtime_error("Duplicate endpoint id: " + ep_def.id);
                }
                HakoPduEndpointDirectionType direction;
                if (ep_def.direction == "in") {
                    direction = HAKO_PDU_ENDPOINT_DIRECTION_IN;
                } else if (ep_def.direction == "out") {
                    direction = HAKO_PDU_ENDPOINT_DIRECTION_OUT;
                } else if (ep_def.direction == "inout") {
                    direction = HAKO_PDU_ENDPOINT_DIRECTION_INOUT;
                } else {
                    throw std::runtime_error("Invalid endpoint direction: " + ep_def.direction);
                }
                auto endpoint = std::make_shared<hakoniwa::pdu::Endpoint>(
                    ep_def.id, direction
                );
                auto resolved_ep_path = resolve_under_base(base_dir, ep_def.config_path);
                HakoPduErrorType err = endpoint->open(resolved_ep_path.string());
                if (err != HAKO_PDU_ERR_OK) {
                    throw std::runtime_error(
                        "BridgeLoader: Failed to open endpoint config " + ep_def.config_path +
                        ": " + std::to_string(err)
                    );
                }
                endpoints_by_id[ep_def.id] = std::move(endpoint);
            }
        }
         
        /*
         * TransferPdu && connection section
         */
        for (const auto& conn_def : bridge_config.connections) {
            if (conn_def.nodeId != node_name) {
                continue; // Skip connections not intended for this node
            }
            auto connection = std::make_unique<BridgeConnection>(conn_def.id);
            
            auto src_ep_it = endpoints_by_id.find(conn_def.source.endpointId);
            if (src_ep_it == endpoints_by_id.end()) throw std::runtime_error("Source endpoint not found: " + conn_def.source.endpointId);
            std::shared_ptr<hakoniwa::pdu::Endpoint> src_ep = src_ep_it->second;
            

            for (const auto& dest_def : conn_def.destinations) {
                auto dst_ep_it = endpoints_by_id.find(dest_def.endpointId);
                if (dst_ep_it == endpoints_by_id.end()) throw std::runtime_error("Destination endpoint not found: " + dest_def.endpointId);
                std::shared_ptr<hakoniwa::pdu::Endpoint> dst_ep = dst_ep_it->second;

                for (const auto& trans_pdu_def : conn_def.transferPdus) {
                    auto pit = policy_map.find(trans_pdu_def.policyId);
                    if (pit == policy_map.end()) {
                        throw std::runtime_error("Transfer policy not found: " + trans_pdu_def.policyId);
                    }
                    auto policy = pit->second;

                    const auto& pdu_keys = bridge_config.pduKeyGroups.at(trans_pdu_def.pduKeyGroupId);

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
