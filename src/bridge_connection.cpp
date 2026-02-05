#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource

namespace hakoniwa::pdu::bridge {

void BridgeConnection::add_transfer_pdu(std::unique_ptr<ITransferPdu> pdu) {
    pdu->set_epoch(epoch_.load(std::memory_order_relaxed));
    pdu->set_epoch_validation(epoch_validation_);
    transfer_pdus_.push_back(std::move(pdu));
}

void BridgeConnection::set_active(bool is_active) {
    is_active_ = is_active;
    for (auto& pdu : transfer_pdus_) {
        pdu->set_active(is_active);
    }
}

void BridgeConnection::increment_epoch() {
    uint8_t new_epoch = epoch_.fetch_add(1, std::memory_order_relaxed) + 1;
    for (auto& pdu : transfer_pdus_) {
        pdu->set_epoch(new_epoch);
    }
}

void BridgeConnection::cyclic_trigger() {
    #ifdef ENABLE_DEBUG_MESSAGES
    std::cout << "DEBUG: BridgeConnection cyclic_trigger called. size=" << transfer_pdus_.size() << std::endl;
    #endif
    if (!is_active_) {
        return;
    }
    for (auto& pdu : transfer_pdus_) {
        pdu->cyclic_trigger();
    }
}

} // namespace hakoniwa::pdu::bridge
