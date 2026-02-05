#pragma once

#include "hakoniwa/pdu/bridge/bridge_types.hpp" // For hakoniwa::pdu::bridge::PduKey
#include "hakoniwa/pdu/bridge/pdu_transfer_policy.hpp"
#include "hakoniwa/time_source/time_source.hpp" // Include for ITimeSource
#include "hakoniwa/pdu/endpoint.hpp" // Actual Endpoint class
#include "hakoniwa/pdu/endpoint_types.hpp" // For hakoniwa::pdu::PduKey
#include "hakoniwa/pdu/pdu_definition.hpp" // For hakoniwa::pdu::PduDefinition

#include <memory>
#include <cstdint>
#include <chrono>
#include <vector> // For temporary buffer in transfer()
#include <atomic>

namespace hakoniwa::pdu::bridge {

class ITransferPdu {
public:
    virtual ~ITransferPdu() = default;

    virtual void cyclic_trigger() = 0;
    virtual void set_active(bool is_active) = 0;
    virtual void set_epoch(uint8_t epoch) = 0;
    virtual void set_epoch_validation(bool enable) = 0;
};

class TransferPdu : public ITransferPdu {
public:
    TransferPdu(
        const hakoniwa::pdu::bridge::PduKey& config_key, // The PduKey from bridge.json
        std::shared_ptr<IPduTransferPolicy> policy,
        std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source,
        std::shared_ptr<hakoniwa::pdu::Endpoint> src,
        std::shared_ptr<hakoniwa::pdu::Endpoint> dst
    );

    void set_active(bool is_active) override;
    void set_epoch(uint8_t epoch) override;
    void set_epoch_validation(bool enable) override { epoch_validation_ = enable; }
    
    // Attempts to transfer data based on the policy.
    void cyclic_trigger() override
    {
        if (policy_->is_cyclic_trigger()) {
            try_transfer();
        }
    }
        
private:
    hakoniwa::pdu::bridge::PduKey           config_pdu_key_; // PDU key from bridge.json
    hakoniwa::pdu::PduKey               endpoint_pdu_key_; // PDU key for endpoint API
    hakoniwa::pdu::PduResolvedKey     endpoint_pdu_resolved_key_; // Resolved PDU key for endpoint API
    std::shared_ptr<IPduTransferPolicy> policy_;
    std::shared_ptr<hakoniwa::pdu::Endpoint>            src_endpoint_;
    std::shared_ptr<hakoniwa::pdu::Endpoint>            dst_endpoint_;
    std::shared_ptr<hakoniwa::time_source::ITimeSource>  time_source_;
    bool is_active_ = false;
    std::atomic<uint8_t> owner_epoch_{0};
    bool epoch_validation_ = false;
    void on_recv_callback(const hakoniwa::pdu::PduResolvedKey& pdu_key, std::span<const std::byte> data)
    {
        //std::cout << "TransferPdu: on_recv_callback triggered for Robot: " << pdu_key.robot << " Channel ID: " << pdu_key.channel_id << std::endl;
        try_transfer();
    }
    void try_transfer();

    void transfer();
};


class TransferAtomicPduGroup: public ITransferPdu {
public:
    TransferAtomicPduGroup(
        const std::vector<hakoniwa::pdu::bridge::PduKey>& config_keys,
        std::shared_ptr<IPduTransferPolicy> policy,
        std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source,
        std::shared_ptr<hakoniwa::pdu::Endpoint> src,
        std::shared_ptr<hakoniwa::pdu::Endpoint> dst
    );
    void set_active(bool is_active) override;
    void set_epoch(uint8_t epoch) override;
    void set_epoch_validation(bool enable) override { epoch_validation_ = enable; }
    // Event-driven only; cyclic_trigger is intentionally ignored.
    void cyclic_trigger() override;
private:
    std::vector<std::unique_ptr<hakoniwa::pdu::PduResolvedKey>> transfer_atomic_pdu_group_;
    std::shared_ptr<IPduTransferPolicy> policy_;
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source_;
    std::shared_ptr<hakoniwa::pdu::Endpoint>            src_endpoint_;
    std::shared_ptr<hakoniwa::pdu::Endpoint>            dst_endpoint_;
    bool is_active_ = false;
    std::atomic<uint8_t> owner_epoch_{0};
    bool epoch_validation_ = false;
    void on_recv_callback(const hakoniwa::pdu::PduResolvedKey& pdu_key, std::span<const std::byte> data)
    {
        //std::cout << "TransferAtomicPduGroup: on_recv_callback triggered for Robot: " << pdu_key.robot << " Channel ID: " << pdu_key.channel_id << std::endl;
        try_transfer(pdu_key, data);
    }
    void try_transfer(const hakoniwa::pdu::PduResolvedKey& pdu_key, std::span<const std::byte> data);
    void try_transfer_group();
};

} // namespace hakoniwa::pdu::bridge
