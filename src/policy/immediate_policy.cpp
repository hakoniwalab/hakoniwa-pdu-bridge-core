#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/pdu/bridge/time_source.hpp" // For ITimeSource

namespace hako::pdu::bridge {

bool ImmediatePolicy::should_transfer(const std::shared_ptr<ITimeSource>& /* time_source */) {
    // For immediate policy, the answer is always yes when checked upon PDU update.
    return true;
}

void ImmediatePolicy::on_transferred(const std::shared_ptr<ITimeSource>& /* time_source */) {
    // No state needs to be updated.
}

} // namespace hako::pdu::bridge
