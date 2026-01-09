#include "hakoniwa/pdu/bridge/bridge_types.hpp"
#include "hakoniwa/pdu/endpoint_container.hpp"
#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/bridge/bridge_build_result.hpp"
#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/pdu/bridge/policy/throttle_policy.hpp"
#include "hakoniwa/pdu/bridge/policy/ticker_policy.hpp"
#include "hakoniwa/time_source/time_source_factory.hpp"
#include "hakoniwa/pdu/endpoint.hpp"          // Actual Endpoint class
#include "hakoniwa/pdu/pdu_definition.hpp"    // For PduDefinition

#include <nlohmann/json.hpp> // nlohmann/json

#include <fstream>
#include <stdexcept>
#include <filesystem>
namespace fs = std::filesystem;

namespace hakoniwa::pdu::bridge {

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
    BridgeBuildResult build(const std::string& config_file_path, const std::string& node_name, uint64_t delta_time_step_usec, std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container)
    {
        fs::path bridge_path(config_file_path);
        fs::path base_dir = bridge_path.parent_path();

        BridgeConfig bridge_config = parse(config_file_path);

        /*
         * time source selection
         */
        std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source = hakoniwa::time_source::create_time_source(bridge_config.time_source_type, delta_time_step_usec);
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
         * TransferPdu && connection section
         */
        for (const auto& conn_def : bridge_config.connections) {
            if (conn_def.nodeId != node_name) {
                continue; // Skip connections not intended for this node
            }
            auto connection = std::make_unique<BridgeConnection>(conn_def.id);
            
            std::shared_ptr<hakoniwa::pdu::Endpoint> src_ep = endpoint_container->ref(conn_def.source.endpointId);
            if (!src_ep) {
                throw std::runtime_error("Source endpoint not found: " + conn_def.source.endpointId);
            }

            for (const auto& dest_def : conn_def.destinations) {
                std::shared_ptr<hakoniwa::pdu::Endpoint> dst_ep = endpoint_container->ref(dest_def.endpointId);
                if (!dst_ep) {
                    throw std::runtime_error("Destination endpoint not found: " + dest_def.endpointId);
                }

                for (const auto& trans_pdu_def : conn_def.transferPdus) {
                    auto pit = policy_map.find(trans_pdu_def.policyId);
                    if (pit == policy_map.end()) {
                        throw std::runtime_error("Transfer policy not found: " + trans_pdu_def.policyId);
                    }
                    auto policy = pit->second;

                    const auto& pdu_keys = bridge_config.pduKeyGroups.at(trans_pdu_def.pduKeyGroupId);

                    for (const auto& pdu_key_def : pdu_keys) {
                        auto transfer_pdu = std::make_unique<TransferPdu>(pdu_key_def, policy, time_source, src_ep, dst_ep);
                        connection->add_transfer_pdu(std::move(transfer_pdu));
                    }
                }
            }
            core->add_connection(std::move(connection));
        }
        return { std::move(core) };
    }

}
