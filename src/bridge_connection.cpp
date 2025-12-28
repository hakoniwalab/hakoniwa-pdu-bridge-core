#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/pdu/bridge/time_source/time_source.hpp" // For ITimeSource

namespace hako::pdu::bridge {

void BridgeConnection::add_transfer_pdu(std::unique_ptr<TransferPdu> pdu) {
    transfer_pdus_.push_back(std::move(pdu));
}

void BridgeConnection::step(const std::shared_ptr<ITimeSource>& time_source) {
    for (auto& pdu : transfer_pdus_) {
        pdu->try_transfer(time_source);
    }
}

} // namespace hako::pdu::bridge
