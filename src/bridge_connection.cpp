#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource

namespace hakoniwa::pdu::bridge {

void BridgeConnection::add_transfer_pdu(std::unique_ptr<ITransferPdu> pdu) {
    std::lock_guard<std::mutex> lock(transfer_mtx_);
    pdu->set_epoch(epoch_.load(std::memory_order_relaxed));
    pdu->set_epoch_validation(epoch_validation_);
    transfer_pdus_.push_back(std::move(pdu));
}

ITransferPdu* BridgeConnection::add_monitor_transfer_pdu(std::unique_ptr<ITransferPdu> pdu) {
    std::lock_guard<std::mutex> lock(transfer_mtx_);
    pdu->set_epoch(epoch_.load(std::memory_order_relaxed));
    pdu->set_epoch_validation(epoch_validation_);
    ITransferPdu* handle = pdu.get();
    transfer_pdus_.push_back(std::move(pdu));
    return handle;
}

bool BridgeConnection::remove_transfer_pdu(ITransferPdu* transfer) {
    if (!transfer) {
        return false;
    }
    std::lock_guard<std::mutex> lock(transfer_mtx_);
    auto it = std::find_if(
        transfer_pdus_.begin(),
        transfer_pdus_.end(),
        [transfer](const std::unique_ptr<ITransferPdu>& p) { return p.get() == transfer; });
    if (it == transfer_pdus_.end()) {
        return false;
    }
    transfer_pdus_.erase(it);
    return true;
}

void BridgeConnection::set_active(bool is_active) {
    std::lock_guard<std::mutex> lock(transfer_mtx_);
    is_active_ = is_active;
    for (auto& pdu : transfer_pdus_) {
        pdu->set_active(is_active);
    }
}

void BridgeConnection::increment_epoch() {
    std::lock_guard<std::mutex> lock(transfer_mtx_);
    uint8_t new_epoch = epoch_.fetch_add(1, std::memory_order_relaxed) + 1;
    for (auto& pdu : transfer_pdus_) {
        pdu->set_epoch(new_epoch);
    }
}

void BridgeConnection::cyclic_trigger() {
    std::lock_guard<std::mutex> lock(transfer_mtx_);
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
