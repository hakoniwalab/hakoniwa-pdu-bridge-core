#pragma once

#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/pdu/bridge/bridge_monitor_core.hpp"
#include "hakoniwa/pdu/bridge/bridge_types.hpp"
#include "hakoniwa/pdu/bridge/pdu_transfer_policy.hpp"
#include "hakoniwa/pdu/endpoint_container.hpp"
#include "hakoniwa/time_source/time_source.hpp" // Include for ITimeSource
#include <vector>
#include <memory>
#include <atomic>
#include <string>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace hakoniwa::pdu::bridge {

class BridgeMonitorRuntime;

class BridgeCore : public IBridgeMonitorCore {
public:
    BridgeCore(const std::string& node_name, std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source, std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container);
    ~BridgeCore() = default;

    void add_connection(std::unique_ptr<BridgeConnection> connection);
    void register_connection_transfer_pdu_key(
        const std::string& connection_id,
        const std::string& robot,
        const std::string& pdu_name);
    uint64_t get_delta_time_microseconds() const
    {
        return time_source_->get_delta_time_microseconds();
    }

    void start();

    bool is_running() const override
    {
        if (!is_running_) {
            return false;
        }
        return true;
    }

    /*
     * Trigger cyclic processing for all connections and endpoints.
     * Returns true if the bridge is still running, false if it has been stopped.
     */
    bool cyclic_trigger();

    // Stops the execution loop. This can be called from a different thread.
    void stop();

    // Connection-level control
    bool set_connection_active(const std::string& connection_id, bool is_active);
    bool pause_connection(const std::string& connection_id) { return set_connection_active(connection_id, false); }
    bool resume_connection(const std::string& connection_id) { return set_connection_active(connection_id, true); }
    bool get_connection_epoch(const std::string& connection_id, uint8_t& out_epoch) const;
    bool increment_connection_epoch(const std::string& connection_id);

    // Low-level monitor transfer API (used by BridgeMonitorRuntime)
    std::optional<ResolvedMonitorSelection> resolve_monitor_selection(
        const std::string& connection_id,
        const std::vector<MonitorFilter>& filters,
        std::string& error) const override;
    ITransferPdu* create_monitor_transfer(
        const std::string& connection_id,
        const hakoniwa::pdu::bridge::PduKey& monitor_key,
        const MonitorPolicy& policy,
        const std::shared_ptr<hakoniwa::pdu::Endpoint>& destination_endpoint,
        std::string& error) override;
    void deactivate_monitor_transfer(ITransferPdu* transfer) override;
    bool remove_monitor_transfer(ITransferPdu* transfer) override;

    // Introspection API
    BridgeHealthDto get_health() const override;
    std::vector<ConnectionStateDto> list_connections() const override;
    std::optional<std::vector<PduStateDto>> list_pdus(const std::string& connection_id) const override;
    std::optional<ConnectionStateDto> get_connection(const std::string& connection_id) const;
    void attach_monitor_runtime(std::shared_ptr<BridgeMonitorRuntime> monitor_runtime);
    void detach_monitor_runtime();

private:
    bool has_connection_(const std::string& connection_id) const;
    const BridgeConnection* find_connection_(const std::string& connection_id) const;
    BridgeConnection* find_connection_mutable_(const std::string& connection_id);
    bool is_supported_monitor_policy_(const MonitorPolicy& policy) const;
    std::shared_ptr<IPduTransferPolicy> create_monitor_policy_(const MonitorPolicy& policy) const;
    std::optional<ResolvedMonitorSelection> resolve_monitor_keys_(
        BridgeConnection* connection,
        const std::vector<MonitorFilter>& filters,
        std::string& error) const;
    const std::vector<std::pair<std::string, std::string>>* find_transferable_pdus_(
        const std::string& connection_id) const;

    std::string node_name_;
    std::vector<std::unique_ptr<BridgeConnection>> connections_;
    std::atomic<bool> is_running_;
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source_;
    std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container_;
    std::vector<std::string> endpoint_ids_;
    mutable std::mutex state_mtx_;
    std::string last_error_;
    uint64_t started_time_usec_{0};
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> connection_transferable_pdus_;
    mutable std::mutex monitor_runtime_mtx_;
    std::shared_ptr<BridgeMonitorRuntime> monitor_runtime_;
};

} // namespace hakoniwa::pdu::bridge
