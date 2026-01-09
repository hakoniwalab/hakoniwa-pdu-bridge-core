#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource

namespace hakoniwa::pdu::bridge {

bool ImmediatePolicy::should_transfer(const PduResolvedKey& pdu_key, const std::shared_ptr<hakoniwa::time_source::ITimeSource>& /* time_source */) {
    if (is_atomic_) {
        // In atomic mode, check if all PDUs have been received.
        for (const auto& pair : recv_states_) {
            if (!pair.second) {
                return false; // At least one PDU has not been received yet.
            }
        }
        return true; // All PDUs have been received.
    } else {
        return true;
    }
}

void ImmediatePolicy::on_transferred(const PduResolvedKey& pdu_key, const std::shared_ptr<hakoniwa::time_source::ITimeSource>& /* time_source */) {
    if (is_atomic_) {
        // Mark this PDU as received.
        recv_states_[{pdu_key.robot, pdu_key.channel_id}] = true;
        for (auto& pair : recv_states_) {
            if (!pair.second) {
                return;
            }
        }
        // Reset all states for the next atomic transfer.
        for (auto& pair : recv_states_) {
            pair.second = false;
        }
    }
    else {
        // No state management needed for non-atomic mode.
    }
}

} // namespace hakoniwa::pdu::bridge
