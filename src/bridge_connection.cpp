#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource

namespace hakoniwa::pdu::bridge {

void BridgeConnection::add_transfer_pdu(std::unique_ptr<ITransferPdu> pdu) {
    transfer_pdus_.push_back(std::move(pdu));
}

void BridgeConnection::cyclic_trigger() {
    #ifdef ENABLE_DEBUG_MESSAGES
    std::cout << "DEBUG: BridgeConnection cyclic_trigger called. size=" << transfer_pdus_.size() << std::endl;
    #endif
    for (auto& pdu : transfer_pdus_) {
        pdu->cyclic_trigger();
    }
}

} // namespace hakoniwa::pdu::bridge
