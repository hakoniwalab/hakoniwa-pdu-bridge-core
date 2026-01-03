#pragma once

#include "hakoniwa/pdu/bridge/pdu_transfer_policy.hpp"
#include <memory> // For std::shared_ptr

namespace hakoniwa::pdu::bridge {

class ImmediatePolicy : public IPduTransferPolicy {
public:
    ImmediatePolicy() = default;
    ~ImmediatePolicy() = default;

    bool should_transfer(const std::shared_ptr<hakoniwa::time_source::ITimeSource>& time_source) override;
    void on_transferred(const std::shared_ptr<hakoniwa::time_source::ITimeSource>& time_source) override;

    bool is_cyclic_trigger() const override { return false; }
};

} // namespace hakoniwa::pdu::bridge


