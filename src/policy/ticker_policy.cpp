#include "hakoniwa/pdu/bridge/policy/ticker_policy.hpp"
#include "hakoniwa/pdu/bridge/time_source/time_source.hpp" // For ITimeSource

namespace hakoniwa::pdu::bridge {

TickerPolicy::TickerPolicy(uint64_t interval)
    : interval_(interval), initialized_(false) {}

bool TickerPolicy::should_transfer(const std::shared_ptr<ITimeSource>& time_source) {
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

void TickerPolicy::on_transferred(const std::shared_ptr<ITimeSource>& time_source) {
    uint64_t now = time_source->get_microseconds();
    // This method is called after a transfer has been made.
    // We need to schedule the next tick.
    // To prevent drift, we calculate the next tick based on the previous scheduled tick,
    // not the current time 'now'.
    // We also loop to ensure the next tick is in the future, in case the system has
    // lagged for more than one interval.
    do {
        next_tick_time_ += interval_;
    } while (next_tick_time_ <= now);
}

} // namespace hakoniwa::pdu::bridge
