#pragma once

#include "hakoniwa/pdu/bridge/pdu_transfer_policy.hpp"
#include <memory> // For std::shared_ptr
#include <chrono>

namespace hakoniwa::pdu::bridge {

class TickerPolicy : public IPduTransferPolicy {
public:
    explicit TickerPolicy(uint64_t interval);
    ~TickerPolicy() = default;

    bool should_transfer(const std::shared_ptr<ITimeSource>& time_source) override;
    void on_transferred(const std::shared_ptr<ITimeSource>& time_source) override;
    bool is_cyclic_trigger() const override { return true; }

private:
    uint64_t interval_;
    uint64_t next_tick_time_;
    bool initialized_;
};

} // namespace hakoniwa::pdu::bridge
