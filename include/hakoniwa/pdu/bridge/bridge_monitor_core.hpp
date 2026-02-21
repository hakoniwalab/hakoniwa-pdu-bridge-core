#pragma once

#include "hakoniwa/pdu/bridge/bridge_types.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace hakoniwa::pdu {
class Endpoint;
}

namespace hakoniwa::pdu::bridge {

class ITransferPdu;

struct ResolvedMonitorSelection {
    std::vector<hakoniwa::pdu::bridge::PduKey> keys;
    std::vector<MonitorFilter> filters;
};

class IBridgeMonitorCore {
public:
    virtual ~IBridgeMonitorCore() = default;

    virtual bool is_running() const = 0;
    virtual std::optional<ResolvedMonitorSelection> resolve_monitor_selection(
        const std::string& connection_id,
        const std::vector<MonitorFilter>& filters,
        std::string& error) const = 0;
    virtual ITransferPdu* create_monitor_transfer(
        const std::string& connection_id,
        const hakoniwa::pdu::bridge::PduKey& monitor_key,
        const MonitorPolicy& policy,
        const std::shared_ptr<hakoniwa::pdu::Endpoint>& destination_endpoint,
        std::string& error) = 0;
    virtual void deactivate_monitor_transfer(ITransferPdu* transfer) = 0;
    virtual bool remove_monitor_transfer(ITransferPdu* transfer) = 0;
    virtual BridgeHealthDto get_health() const = 0;
    virtual std::vector<ConnectionStateDto> list_connections() const = 0;
    virtual std::optional<std::vector<PduStateDto>> list_pdus(const std::string& connection_id) const = 0;
};

} // namespace hakoniwa::pdu::bridge
