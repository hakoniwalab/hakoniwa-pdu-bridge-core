#pragma once

#include "hakoniwa/pdu/bridge/bridge_types.hpp" // For hakoniwa::pdu::bridge::PduKey
#include "hakoniwa/pdu/bridge/pdu_transfer_policy.hpp"
#include "hakoniwa/pdu/bridge/time_source/time_source.hpp" // Include for ITimeSource
#include "hakoniwa/pdu/endpoint.hpp" // Actual Endpoint class
#include "hakoniwa/pdu/endpoint_types.hpp" // For hakoniwa::pdu::PduKey
#include "hakoniwa/pdu/pdu_definition.hpp" // For hakoniwa::pdu::PduDefinition

#include <memory>
#include <cstdint>
#include <chrono>
#include <vector> // For temporary buffer in transfer()

namespace hakoniwa::pdu::bridge {

class TransferPdu {
public:
    TransferPdu(
        const hakoniwa::pdu::bridge::PduKey& config_key, // The PduKey from bridge.json
        std::shared_ptr<IPduTransferPolicy> policy,
        std::shared_ptr<ITimeSource> time_source,
        std::shared_ptr<hakoniwa::pdu::Endpoint> src,
        std::shared_ptr<hakoniwa::pdu::Endpoint> dst
    );

    void set_active(bool is_active);
    void set_epoch(uint64_t epoch);
    
    // Attempts to transfer data based on the policy.
    void cyclic_trigger()
    {
        if (policy_->is_cyclic_trigger()) {
            try_transfer();
        }
    }
        
private:
    hakoniwa::pdu::bridge::PduKey           config_pdu_key_; // PDU key from bridge.json
    hakoniwa::pdu::PduKey               endpoint_pdu_key_; // PDU key for endpoint API
    std::shared_ptr<IPduTransferPolicy> policy_;
    std::shared_ptr<hakoniwa::pdu::Endpoint>            src_endpoint_;
    std::shared_ptr<hakoniwa::pdu::Endpoint>            dst_endpoint_;
    std::shared_ptr<ITimeSource>                        time_source_;
    bool is_active_ = false;
    uint64_t owner_epoch_ = 0;
    void on_recv_callback(const hakoniwa::pdu::PduResolvedKey& pdu_key, std::span<const std::byte> data)
    {
        try_transfer();
    }
    void try_transfer();

    void transfer();
    bool accept_epoch(uint64_t pdu_epoch);
};

} // namespace hakoniwa::pdu::bridge

