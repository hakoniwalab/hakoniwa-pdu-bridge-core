#pragma once

#include "hakoniwa/pdu/bridge/bridge_monitor_core.hpp"
#include "hakoniwa/pdu/bridge/ondemand_control_handler.hpp"
#include "hakoniwa/pdu/endpoint_comm_multiplexer.hpp"
#include "hakoniwa/pdu/endpoint_types.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace hakoniwa::pdu::bridge {

struct BridgeMonitorRuntimeOptions {
    bool enable_ondemand = false;
    std::string ondemand_mux_config_path;
    std::string control_robot = "BridgeControl";
    int control_request_channel = 1;
    int control_response_channel = 2;
};

class BridgeMonitorRuntime : public std::enable_shared_from_this<BridgeMonitorRuntime> {
public:
    explicit BridgeMonitorRuntime(std::shared_ptr<IBridgeMonitorCore> core);

    HakoPduErrorType initialize(const BridgeMonitorRuntimeOptions& options);
    void process_control_plane_once();
    void shutdown();
    std::optional<std::string> attach_monitor(const MonitorSessionSpec& spec);
    bool detach_monitor(const std::string& session_id);
    std::vector<MonitorSessionInfo> list_monitor_infos() const;
    BridgeHealthDto get_health() const;
    std::vector<ConnectionStateDto> list_connections() const;
    std::optional<std::vector<PduStateDto>> list_pdus(const std::string& connection_id) const;

private:
    struct MonitorSessionRuntime {
        std::vector<ITransferPdu*> transfers;
        std::shared_ptr<hakoniwa::pdu::Endpoint> destination_endpoint;
    };

    void cleanup_disconnected_sessions_();
    void detach_sessions_for_endpoint_(const std::string& endpoint_name);
    static std::vector<std::byte> to_bytes_(const std::string& text);

    std::shared_ptr<IBridgeMonitorCore> core_;
    BridgeMonitorRuntimeOptions options_;
    bool initialized_{false};

    std::unique_ptr<hakoniwa::pdu::EndpointCommMultiplexer> ondemand_mux_;
    std::shared_ptr<OnDemandControlHandler> control_handler_;
    hakoniwa::pdu::PduResolvedKey control_req_key_{"BridgeControl", 1};
    hakoniwa::pdu::PduResolvedKey control_res_key_{"BridgeControl", 2};

    mutable std::mutex session_mtx_;
    std::vector<std::shared_ptr<hakoniwa::pdu::Endpoint>> ondemand_sessions_;
    std::unordered_set<std::string> disconnected_control_endpoints_;
    std::unordered_map<std::string, std::vector<std::string>> endpoint_session_ids_;
    std::unordered_map<std::string, MonitorSessionInfo> monitor_sessions_;
    std::unordered_map<std::string, MonitorSessionRuntime> monitor_runtime_sessions_;
    uint64_t next_monitor_session_id_{1};
};

} // namespace hakoniwa::pdu::bridge
