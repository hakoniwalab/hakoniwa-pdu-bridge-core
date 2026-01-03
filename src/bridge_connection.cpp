#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource

namespace hakoniwa::pdu::bridge {

void BridgeConnection::add_transfer_pdu(std::unique_ptr<TransferPdu> pdu) {
    transfer_pdus_.push_back(std::move(pdu));
}

void BridgeConnection::cyclic_trigger() {
    for (auto& pdu : transfer_pdus_) {
        pdu->cyclic_trigger();
    }
}

} // namespace hakoniwa::pdu::bridge
