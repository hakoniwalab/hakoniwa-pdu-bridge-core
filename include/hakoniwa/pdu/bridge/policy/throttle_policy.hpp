#pragma once

#include "hakoniwa/pdu/bridge/pdu_transfer_policy.hpp"
#include <memory> // For std::shared_ptr
#include <chrono>

namespace hako::pdu::bridge {

class ThrottlePolicy : public IPduTransferPolicy {
public:
    explicit ThrottlePolicy(std::chrono::milliseconds interval);
    ~ThrottlePolicy() = default;

    bool should_transfer(const std::shared_ptr<ITimeSource>& time_source) override;
    void on_transferred(const std::shared_ptr<ITimeSource>& time_source) override;

private:
    std::chrono::milliseconds interval_;
    std::chrono::steady_clock::time_point last_transfer_time_;
};

} // namespace hako::pdu::bridge
