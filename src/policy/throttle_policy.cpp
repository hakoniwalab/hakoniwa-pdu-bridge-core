#include "hakoniwa/pdu/bridge/policy/throttle_policy.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource

namespace hakoniwa::pdu::bridge {

ThrottlePolicy::ThrottlePolicy(uint64_t interval_microseconds)
    : interval_micros_(interval_microseconds),
      last_transfer_time_micros_(0),
      has_transferred_(false) {}

bool ThrottlePolicy::should_transfer(const PduResolvedKey& pdu_key, const std::shared_ptr<hakoniwa::time_source::ITimeSource>& time_source) {
    if (!has_transferred_.load()) {
        return true;
    }
    uint64_t now = time_source->get_microseconds();
    if ((now - last_transfer_time_micros_.load()) >= interval_micros_) {
        return true;
    }
    return false;
}

void ThrottlePolicy::on_transferred(const PduResolvedKey& pdu_key, const std::shared_ptr<hakoniwa::time_source::ITimeSource>& time_source) {
    last_transfer_time_micros_ = time_source->get_microseconds();
    has_transferred_ = true;
}

} // namespace hakoniwa::pdu::bridge
