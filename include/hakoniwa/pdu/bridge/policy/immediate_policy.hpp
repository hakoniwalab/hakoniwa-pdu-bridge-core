#pragma once

#include "hakoniwa/pdu/bridge/pdu_transfer_policy.hpp"
#include <memory> // For std::shared_ptr
#include <map>

namespace hakoniwa::pdu::bridge {

class ImmediatePolicy : public IPduTransferPolicy {
public:
    ImmediatePolicy(bool is_atomic) : is_atomic_(is_atomic) {}
    ~ImmediatePolicy() = default;

    void add_pdu_key(const PduResolvedKey& pdu_key) {
        recv_states_[{pdu_key.robot, pdu_key.channel_id}] = false;        
    }

    bool should_transfer(const PduResolvedKey& pdu_key, const std::shared_ptr<hakoniwa::time_source::ITimeSource>& time_source) override;
    void on_transferred(const PduResolvedKey& pdu_key, const std::shared_ptr<hakoniwa::time_source::ITimeSource>& time_source) override;

    bool is_cyclic_trigger() const override { return false; }
private:
    bool is_atomic_ = false;

    // state for recv for each pdu keys
    std::map<std::pair<std::string, int>, bool> recv_states_;
};

} // namespace hakoniwa::pdu::bridge


