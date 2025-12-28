#include "hakoniwa/pdu/bridge/policy/throttle_policy.hpp"
#include "hakoniwa/pdu/bridge/time_source.hpp" // For ITimeSource

namespace hako::pdu::bridge {

ThrottlePolicy::ThrottlePolicy(uint64_t interval_microseconds)
    : interval_micros_(interval_microseconds), last_transfer_time_micros_(0) {}

bool ThrottlePolicy::should_transfer(const std::shared_ptr<ITimeSource>& time_source) {
    uint64_t now = time_source->get_microseconds();
    if ((now - last_transfer_time_micros_.load()) >= interval_micros_) {
        return true;
    }
    return false;
}

void ThrottlePolicy::on_transferred(const std::shared_ptr<ITimeSource>& time_source) {
    last_transfer_time_micros_ = time_source->get_microseconds();
}

} // namespace hako::pdu::bridge
