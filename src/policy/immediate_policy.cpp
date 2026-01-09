#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource

namespace hakoniwa::pdu::bridge {

bool ImmediatePolicy::should_transfer(const PduResolvedKey& pdu_key, const std::shared_ptr<hakoniwa::time_source::ITimeSource>& /* time_source */) {
    // For immediate policy, the answer is always yes when checked upon PDU update.
    return true;
}

void ImmediatePolicy::on_transferred(const PduResolvedKey& pdu_key, const std::shared_ptr<hakoniwa::time_source::ITimeSource>& /* time_source */) {
    // No state needs to be updated.
}

} // namespace hakoniwa::pdu::bridge
