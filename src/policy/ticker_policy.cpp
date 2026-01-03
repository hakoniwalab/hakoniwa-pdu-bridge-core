#include "hakoniwa/pdu/bridge/policy/ticker_policy.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource

namespace hakoniwa::pdu::bridge {

TickerPolicy::TickerPolicy(uint64_t interval)
    : interval_(interval), initialized_(false) {}

bool TickerPolicy::should_transfer(const std::shared_ptr<hakoniwa::time_source::ITimeSource>& time_source) {
    uint64_t now = time_source->get_microseconds();
    if (!initialized_) {
        // On the first check, set the initial tick time but do not trigger a transfer.
        // The first transfer will occur after the first interval has passed.
        next_tick_time_ = now + interval_;
        initialized_ = true;
        return false;
    }
    return now >= next_tick_time_;
}

void TickerPolicy::on_transferred(const std::shared_ptr<hakoniwa::time_source::ITimeSource>& time_source) {
    uint64_t now = time_source->get_microseconds();
    next_tick_time_ = now + interval_;
}

} // namespace hakoniwa::pdu::bridge
