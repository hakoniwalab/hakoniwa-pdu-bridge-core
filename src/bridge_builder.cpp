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

    // Helper to parse BridgeConfig from file.
    // NOTE: JSON exceptions are caught here and converted to error_message per exception-free policy.
    std::optional<BridgeConfig> parse(const std::string& config_path, std::string& error_message) {
        std::ifstream ifs(config_path);
        if (!ifs.is_open()) {
            error_message = "BridgeLoader: Failed to open bridge config file: " + config_path;
            return std::nullopt;
        }
        nlohmann::json j = nlohmann::json::parse(ifs, nullptr, false);
        if (j.is_discarded()) {
            error_message = "BridgeLoader: Failed to parse bridge config JSON: " + config_path;
            return std::nullopt;
        }
        try {
            return j.get<BridgeConfig>();
        } catch (const std::exception& e) {
            error_message = std::string("BridgeLoader: Failed to decode bridge config: ") + e.what();
            return std::nullopt;
        }
    }
    std::shared_ptr<IPduTransferPolicy> create_policy_instance(
        const TransferPolicy& policy_def,
        std::string& error_message)
    {
        if (policy_def.type == "immediate") {
            bool is_atomic = policy_def.atomic.value_or(false);
            return std::make_shared<ImmediatePolicy>(is_atomic);
        }
        if (policy_def.type == "throttle") {
            if (!policy_def.intervalMs) {
                error_message = "BridgeLoader: throttle policy needs intervalMs";
                return nullptr;
            }
            return std::make_shared<ThrottlePolicy>(static_cast<uint64_t>(*policy_def.intervalMs) * 1000);
        }
        if (policy_def.type == "ticker") {
            if (!policy_def.intervalMs) {
                error_message = "BridgeLoader: ticker policy needs intervalMs";
                return nullptr;
            }
            return std::make_shared<TickerPolicy>(static_cast<uint64_t>(*policy_def.intervalMs) * 1000);
        }
        error_message = "BridgeLoader: Unknown transfer policy type: " + policy_def.type;
        return nullptr;
    }
    BridgeBuildResult build(const std::string& config_file_path, 
        const std::string& node_name, 
        std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source,
        std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container)
    {
        BridgeBuildResult result;
        if (!time_source) {
            result.error_message = "BridgeLoader: Time source is null";
            return result;
        }
        if (!endpoint_container) {
            result.error_message = "BridgeLoader: EndpointContainer is null";
            return result;
        }
        std::string error_message;
        auto maybe_config = parse(config_file_path, error_message);
        if (!maybe_config) {
            result.error_message = std::move(error_message);
            return result;
        }
        const BridgeConfig& bridge_config = *maybe_config;

        /*
         * bridge core creation
         */
        std::unique_ptr<BridgeCore> core = std::make_unique<BridgeCore>(
            node_name,
            time_source,
            endpoint_container
        );

        /*
         * TransferPdu && connection section
         */
        for (const auto& conn_def : bridge_config.connections) {
            if (conn_def.nodeId != node_name) {
                continue; // Skip connections not intended for this node
            }
            bool epoch_validation = conn_def.epoch_validation.value_or(false);
            auto connection = std::make_unique<BridgeConnection>(conn_def.nodeId, conn_def.id, epoch_validation);
            std::map<std::string, std::shared_ptr<IPduTransferPolicy>> connection_policy_map;
            
            std::shared_ptr<hakoniwa::pdu::Endpoint> src_ep = endpoint_container->ref(conn_def.source.endpointId);
            if (!src_ep) {
                result.error_message = "BridgeLoader: Source endpoint not found: " + conn_def.source.endpointId;
                return result;
            }

            for (const auto& dest_def : conn_def.destinations) {
                std::shared_ptr<hakoniwa::pdu::Endpoint> dst_ep = endpoint_container->ref(dest_def.endpointId);
                if (!dst_ep) {
                    result.error_message = "BridgeLoader: Destination endpoint not found: " + dest_def.endpointId;
                    return result;
                }

                for (const auto& trans_pdu_def : conn_def.transferPdus) {
                    auto policy_def_it = bridge_config.transferPolicies.find(trans_pdu_def.policyId);
                    if (policy_def_it == bridge_config.transferPolicies.end()) {
                        result.error_message = "BridgeLoader: Transfer policy not found: " + trans_pdu_def.policyId;
                        return result;
                    }
                    const auto& policy_def = policy_def_it->second;

                    auto key_group_it = bridge_config.pduKeyGroups.find(trans_pdu_def.pduKeyGroupId);
                    if (key_group_it == bridge_config.pduKeyGroups.end()) {
                        result.error_message = "BridgeLoader: PduKeyGroup not found: " + trans_pdu_def.pduKeyGroupId;
                        return result;
                    }
                    const auto& pdu_keys = key_group_it->second;

                    bool is_immediate_atomic = (policy_def.type == "immediate") && policy_def.atomic.value_or(false);
                    if (is_immediate_atomic) {
                        auto immediate_policy = std::make_shared<ImmediatePolicy>(true);
                        for (const auto& pdu_key_def : pdu_keys) {
                            auto channel_id = src_ep->get_pdu_channel_id({pdu_key_def.robot_name, pdu_key_def.pdu_name});
                            immediate_policy->add_pdu_key({pdu_key_def.robot_name, channel_id});
                        }
                        auto transfer_group = std::make_unique<TransferAtomicPduGroup>(pdu_keys, immediate_policy, time_source, src_ep, dst_ep);
                        connection->add_transfer_pdu(std::move(transfer_group));
                    } else {
                        auto pit = connection_policy_map.find(trans_pdu_def.policyId);
                        if (pit == connection_policy_map.end()) {
                            auto new_policy = create_policy_instance(policy_def, result.error_message);
                            if (!new_policy) {
                                return result;
                            }
                            pit = connection_policy_map.emplace(trans_pdu_def.policyId, std::move(new_policy)).first;
                        }
                        auto policy = pit->second;
                        for (const auto& pdu_key_def : pdu_keys) {
                            auto transfer_pdu = std::make_unique<TransferPdu>(pdu_key_def, policy, time_source, src_ep, dst_ep);
                            connection->add_transfer_pdu(std::move(transfer_pdu));
                        }
                    }
                }
            }
            core->add_connection(std::move(connection));
        }
        result.core = std::move(core);
        return result;
    }

}
