#pragma once

#include "hakoniwa/pdu/bridge/transfer_pdu.hpp"
#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/time_source/time_source.hpp" // Include for ITimeSource
#include <vector>
#include <memory>
#include <chrono>
#include <string>
#include <cstdint>
#include <atomic>
#include <mutex>

namespace hakoniwa::pdu::bridge {

class BridgeConnection {
public:
    // Backward-compatible constructor.
    BridgeConnection(const std::string& node_id,
                     const std::string& connection_id,
                     bool epoch_validation)
        : BridgeConnection(node_id, connection_id, epoch_validation, nullptr) {}

    BridgeConnection(const std::string& node_id,
                     const std::string& connection_id,
                     bool epoch_validation,
                     std::shared_ptr<hakoniwa::pdu::Endpoint> src_endpoint)
        : node_id_(node_id), connection_id_(connection_id), src_endpoint_(std::move(src_endpoint)), epoch_validation_(epoch_validation) {}

    const std::string& getNodeId() const { return node_id_; }
    const std::string& getConnectionId() const { return connection_id_; }

    void add_transfer_pdu(std::unique_ptr<ITransferPdu> pdu);
    ITransferPdu* add_monitor_transfer_pdu(std::unique_ptr<ITransferPdu> pdu);
    bool remove_transfer_pdu(ITransferPdu* transfer);
    void set_active(bool is_active);
    bool is_active() const { return is_active_; }
    uint8_t get_epoch() const { return epoch_.load(std::memory_order_relaxed); }
    void increment_epoch();
    bool epoch_validation_enabled() const { return epoch_validation_; }
    std::shared_ptr<hakoniwa::pdu::Endpoint> get_source_endpoint() const { return src_endpoint_; }

    void cyclic_trigger();

private:
    std::string node_id_;
    std::string connection_id_;
    std::shared_ptr<hakoniwa::pdu::Endpoint> src_endpoint_;
    mutable std::mutex transfer_mtx_;
    std::vector<std::unique_ptr<ITransferPdu>> transfer_pdus_;
    bool is_active_ = true;
    std::atomic<uint8_t> epoch_{0};
    bool epoch_validation_ = false;
};

} // namespace hakoniwa::pdu::bridge
