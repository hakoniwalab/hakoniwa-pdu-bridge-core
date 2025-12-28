#include "hakoniwa/pdu/bridge/transfer_pdu.hpp"
#include "hakoniwa/pdu/bridge/time_source.hpp" // For ITimeSource
#include <iostream>
#include <stdexcept> // For error handling
#include <vector> // For std::vector<std::byte>

namespace hako::pdu::bridge {

TransferPdu::TransferPdu(
    const hako::pdu::bridge::PduKey& config_key,
    std::shared_ptr<IPduTransferPolicy> policy,
    std::shared_ptr<hakoniwa::pdu::Endpoint> src,
    std::shared_ptr<hakoniwa::pdu::Endpoint> dst)
    : config_pdu_key_(config_key),
      endpoint_pdu_key_({config_key.robot_name, config_key.pdu_name}), // Convert to endpoint PduKey
      policy_(policy),
      src_endpoint_(src),
      dst_endpoint_(dst),
      is_active_(true),
      owner_epoch_(0) {
    if (!src_endpoint_ || !dst_endpoint_) {
        throw std::runtime_error("TransferPdu: Source or Destination endpoint is null.");
    }
}

void TransferPdu::set_active(bool is_active) {
    is_active_ = is_active;
}

void TransferPdu::set_epoch(uint64_t epoch) {
    owner_epoch_ = epoch;
}

void TransferPdu::try_transfer(const std::shared_ptr<ITimeSource>& time_source) {
    if (!is_active_) {
        return;
    }
    if (policy_->should_transfer(time_source)) {
        transfer();
        policy_->on_transferred(time_source);
    }
}

void TransferPdu::transfer() {
    size_t pdu_size = src_endpoint_->get_pdu_size(
        endpoint_pdu_key_
    );

    if (pdu_size == 0) {
        std::cerr << "ERROR: PDU size is 0 for " << endpoint_pdu_key_.robot 
                  << "." << endpoint_pdu_key_.pdu << ". Skipping transfer." << std::endl;
        return;
    }

    std::vector<std::byte> buffer(pdu_size); // Use std::byte for raw PDU data

    size_t received_size = 0;
    // Read from source endpoint
    HakoPduErrorType read_err = src_endpoint_->recv(
        endpoint_pdu_key_, std::span<std::byte>(buffer), received_size
    );

    if (read_err != HAKO_PDU_ERR_OK) {
        std::cerr << "ERROR: Failed to read PDU " << endpoint_pdu_key_.robot 
                  << "." << endpoint_pdu_key_.pdu << " from source: " << read_err << std::endl;
        return;
    }
    if (received_size != pdu_size) {
         std::cerr << "WARNING: PDU " << endpoint_pdu_key_.robot 
                  << "." << endpoint_pdu_key_.pdu << " read " << received_size 
                  << " bytes, expected " << pdu_size << std::endl;
    }


    // Epoch handling: Assuming epoch is part of the PDU data.
    // This requires knowledge of the PDU structure, which is not available here.
    // For now, let's assume the first sizeof(uint64_t) bytes contain the epoch.
    uint64_t pdu_epoch = 0;
    if (received_size >= sizeof(uint64_t)) {
        pdu_epoch = *reinterpret_cast<const uint64_t*>(buffer.data());
    }
    
    if (!accept_epoch(pdu_epoch)) {
        std::cout << "DEBUG: Discarding PDU " << config_pdu_key_.id << " (epoch " << pdu_epoch 
                  << ", owner " << owner_epoch_ << ")" << std::endl;
        return;
    }

    // Write to destination endpoint
    HakoPduErrorType write_err = dst_endpoint_->send(
        endpoint_pdu_key_, std::span<const std::byte>(buffer)
    );

    if (write_err != HAKO_PDU_ERR_OK) {
        std::cerr << "ERROR: Failed to write PDU " << endpoint_pdu_key_.robot 
                  << "." << endpoint_pdu_key_.pdu << " to destination: " << write_err << std::endl;
        return;
    }

    std::cout << "DEBUG: Transferred PDU: " << config_pdu_key_.id << " from " << src_endpoint_->get_name()
              << " to " << dst_endpoint_->get_name() << std::endl;
}

bool TransferPdu::accept_epoch(uint64_t pdu_epoch) {
    // Per impl-design.md, the receiver should discard PDUs from older epochs.
    // The `owner_epoch_` represents the current valid epoch for this node.
    return pdu_epoch >= owner_epoch_;
}

} // namespace hako::pdu::bridge
