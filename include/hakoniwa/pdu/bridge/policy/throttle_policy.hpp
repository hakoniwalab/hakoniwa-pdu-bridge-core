#pragma once

#include "hakoniwa/pdu/bridge/pdu_transfer_policy.hpp"
#include <atomic>
#include <chrono>
#include <memory> // For std::shared_ptr

namespace hakoniwa::pdu::bridge {

class ThrottlePolicy : public IPduTransferPolicy {
public:
    explicit ThrottlePolicy(uint64_t interval_microseconds);

    bool should_transfer(const std::shared_ptr<ITimeSource>& time_source) override;
    void on_transferred(const std::shared_ptr<ITimeSource>& time_source) override;
    bool is_cyclic_trigger() const override { return false; }

private:
    uint64_t interval_micros_;
    std::atomic<uint64_t> last_transfer_time_micros_;
    std::atomic<bool> has_transferred_;
};

} // namespace hakoniwa::pdu::bridge
