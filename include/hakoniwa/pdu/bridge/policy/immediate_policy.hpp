#pragma once

#include "hakoniwa/pdu/bridge/pdu_transfer_policy.hpp"
#include <memory> // For std::shared_ptr

namespace hako::pdu::bridge {

class ImmediatePolicy : public IPduTransferPolicy {
public:
    ImmediatePolicy() = default;
    ~ImmediatePolicy() = default;

    bool should_transfer(const std::shared_ptr<ITimeSource>& time_source) override;
    void on_transferred(const std::shared_ptr<ITimeSource>& time_source) override;
};

} // namespace hako::pdu::bridge


