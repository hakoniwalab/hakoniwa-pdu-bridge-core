#include "hakoniwa/pdu/bridge/policy/throttle_policy.hpp"
#include "hakoniwa/pdu/bridge/time_source.hpp" // For ITimeSource

namespace hako::pdu::bridge {

ThrottlePolicy::ThrottlePolicy(std::chrono::milliseconds interval)
    : interval_(interval), last_transfer_time_(std::chrono::steady_clock::time_point::min()) {}

bool ThrottlePolicy::should_transfer(const std::shared_ptr<ITimeSource>& time_source) {
    std::chrono::steady_clock::time_point now = time_source->get_steady_clock_time();
    if ((now - last_transfer_time_) >= interval_) {
        return true;
    }
    return false;
}

void ThrottlePolicy::on_transferred(const std::shared_ptr<ITimeSource>& time_source) {
    last_transfer_time_ = time_source->get_steady_clock_time();
}

} // namespace hako::pdu::bridge
