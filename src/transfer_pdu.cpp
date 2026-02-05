#include "hakoniwa/pdu/bridge/transfer_pdu.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource
#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/pdu/pdu_primitive_ctypes.h"
#include <iostream>
#include <vector> // For std::vector<std::byte>

hakoniwa::pdu::bridge::TransferPdu::TransferPdu(
    const hakoniwa::pdu::bridge::PduKey& config_key,
    std::shared_ptr<IPduTransferPolicy> policy,
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source,
    std::shared_ptr<hakoniwa::pdu::Endpoint> src,
    std::shared_ptr<hakoniwa::pdu::Endpoint> dst)
    : config_pdu_key_(config_key),
      endpoint_pdu_key_({config_key.robot_name, config_key.pdu_name}), // Convert to endpoint PduKey
      policy_(policy),
      time_source_(time_source),
      src_endpoint_(src),
      dst_endpoint_(dst),
      is_active_(true),
      owner_epoch_(0) {
    if (!src_endpoint_ || !dst_endpoint_) {
        is_active_ = false;
        return;
    }
    auto channel_id = src->get_pdu_channel_id(endpoint_pdu_key_);
    endpoint_pdu_resolved_key_ = {
        .robot = endpoint_pdu_key_.robot,
        .channel_id = channel_id
    };
    PduResolvedKey pdu_resolved_key{
        .robot = endpoint_pdu_key_.robot,
        .channel_id = channel_id
    };
    if (!policy_->is_cyclic_trigger()) {
        // Register callback for event-driven triggers
        #ifdef ENABLE_DEBUG_MESSAGES
        std::cout << "INFO: Registering PDU for event-driven transfer: "
                  << " robot=" << pdu_resolved_key.robot
                  << " pdu_name=" << config_key.pdu_name
                  << " channel=" << pdu_resolved_key.channel_id
                  << std::endl;
        #endif
        src_endpoint_->subscribe_on_recv_callback(
            pdu_resolved_key,
            [this](const hakoniwa::pdu::PduResolvedKey& pdu_key, std::span<const std::byte> data) {
                this->on_recv_callback(pdu_key, data);
            }
        );
    } else {
        // Suppress endpoint "no subscribers" log for cyclic policies (e.g., ticker).
        src_endpoint_->subscribe_on_recv_callback(
            pdu_resolved_key,
            [](const hakoniwa::pdu::PduResolvedKey&, std::span<const std::byte>) {}
        );
    }
    if (auto immediate_policy = std::dynamic_pointer_cast<ImmediatePolicy>(policy_)) {
        immediate_policy->add_pdu_key(endpoint_pdu_resolved_key_);
    }
}

void hakoniwa::pdu::bridge::TransferPdu::set_active(bool is_active) {
    is_active_ = is_active;
}

void hakoniwa::pdu::bridge::TransferPdu::set_epoch(uint8_t epoch) {
    owner_epoch_.store(epoch, std::memory_order_relaxed);
}

void hakoniwa::pdu::bridge::TransferPdu::try_transfer() {
    if (!is_active_) {
        return;
    }
    if (policy_->should_transfer(endpoint_pdu_resolved_key_, time_source_)) {
        #ifdef ENABLE_DEBUG_MESSAGES
        std::cout << "INFO: Bridge transfer triggered: " << config_pdu_key_.id
                  << " src=" << src_endpoint_->get_name()
                  << " dst=" << dst_endpoint_->get_name()
                  << " robot=" << endpoint_pdu_resolved_key_.robot
                  << " channel=" << endpoint_pdu_resolved_key_.channel_id
                  << std::endl;
        #endif
        transfer();
        policy_->on_transferred(endpoint_pdu_resolved_key_, time_source_);
    }
}

void hakoniwa::pdu::bridge::TransferPdu::transfer() {
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


    if (epoch_validation_) {
        uint8_t pdu_epoch = 0;
        if (hako_pdu_get_epoch(buffer.data(), &pdu_epoch) != 0) {
            std::cerr << "ERROR: Failed to get epoch from PDU "
                      << endpoint_pdu_key_.robot << "." << endpoint_pdu_key_.pdu << std::endl;
            return;
        }
        if (pdu_epoch != owner_epoch_.load(std::memory_order_relaxed)) {
            #ifdef ENABLE_DEBUG_MESSAGES
            std::cout << "DEBUG: Discarding PDU " << config_pdu_key_.id
                      << " (epoch " << static_cast<int>(pdu_epoch)
                      << ", owner " << static_cast<int>(owner_epoch_.load(std::memory_order_relaxed)) << ")" << std::endl;
            #endif
            return;
        }
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
    #ifdef ENABLE_DEBUG_MESSAGES
    std::cout << "INFO: Bridge transfer completed: " << config_pdu_key_.id
              << " bytes=" << received_size
              << " src=" << src_endpoint_->get_name()
              << " dst=" << dst_endpoint_->get_name()
              << std::endl;
    #endif
}

// Implementation of TransferAtomicPduGroup
hakoniwa::pdu::bridge::TransferAtomicPduGroup::TransferAtomicPduGroup(
    const std::vector<hakoniwa::pdu::bridge::PduKey>& config_keys,
    std::shared_ptr<IPduTransferPolicy> policy,
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source,
    std::shared_ptr<hakoniwa::pdu::Endpoint> src,
    std::shared_ptr<hakoniwa::pdu::Endpoint> dst)
    : policy_(policy),
      time_source_(time_source),
      src_endpoint_(src),
      dst_endpoint_(dst),
      is_active_(true),
      owner_epoch_(0)
{
    if (!src || !dst) {
        is_active_ = false;
        return;
    }
    auto immediate_policy = std::dynamic_pointer_cast<ImmediatePolicy>(policy_);
    for (const auto& key : config_keys) {
            auto channel_id = src->get_pdu_channel_id({key.robot_name, key.pdu_name});
            PduResolvedKey pdu_resolved_key{
            .robot = key.robot_name,
            .channel_id = channel_id
        };
        #ifdef ENABLE_DEBUG_MESSAGES
        std::cout << "INFO: Registering atomic group PDU: "
                  << " robot=" << pdu_resolved_key.robot
                  << " pdu_name=" << key.pdu_name
                  << " channel=" << pdu_resolved_key.channel_id
                  << std::endl;
        #endif
        transfer_atomic_pdu_group_.emplace_back(std::make_unique<hakoniwa::pdu::PduResolvedKey>(pdu_resolved_key));
        if (immediate_policy) {
            immediate_policy->add_pdu_key(pdu_resolved_key);
        }
        src_endpoint_->subscribe_on_recv_callback(
            pdu_resolved_key,
            [this](const hakoniwa::pdu::PduResolvedKey& pdu_key, std::span<const std::byte> data) {
                this->on_recv_callback(pdu_key, data);
            }
        );
    }
}

void hakoniwa::pdu::bridge::TransferAtomicPduGroup::set_active(bool is_active)
{
    is_active_ = is_active;
}

void hakoniwa::pdu::bridge::TransferAtomicPduGroup::set_epoch(uint8_t epoch)
{
    owner_epoch_.store(epoch, std::memory_order_relaxed);
}

void hakoniwa::pdu::bridge::TransferAtomicPduGroup::cyclic_trigger()
{
    // Event-driven only: cyclic_trigger is intentionally ignored.
}

void hakoniwa::pdu::bridge::TransferAtomicPduGroup::try_transfer(
    const hakoniwa::pdu::PduResolvedKey& pdu_key, std::span<const std::byte> data)
{
    #ifdef ENABLE_DEBUG_MESSAGES
    std::cout << "INFO: TransferAtomicPduGroup try_transfer called for Robot: "
              << pdu_key.robot << " Channel ID: " << pdu_key.channel_id << std::endl;
    #endif
    (void)pdu_key; // Not used in this context as we transfer the whole group
    (void)data;    // Not used
    if (!is_active_) {
        std::cerr << "INFO: TransferAtomicPduGroup is inactive. Skipping transfer." << std::endl;
        return;
    }
    // Event-driven policies gate transfers by should_transfer().
    if (policy_->should_transfer(pdu_key, time_source_)) {
        try_transfer_group();
        policy_->on_transferred(pdu_key, time_source_);
    }
    else {
        #ifdef ENABLE_DEBUG_MESSAGES
        std::cout << "INFO: TransferAtomicPduGroup transfer not allowed by policy for Robot: "
                  << pdu_key.robot << " Channel ID: " << pdu_key.channel_id << std::endl;
        #endif
    }
}

void hakoniwa::pdu::bridge::TransferAtomicPduGroup::try_transfer_group()
{
    //std::cout << "DEBUG: START transfer" << std::endl;
    struct PduBuffer {
        hakoniwa::pdu::PduResolvedKey key;
        std::string pdu_name;
        std::vector<std::byte> data;
    };
    std::vector<PduBuffer> buffers;
    buffers.reserve(transfer_atomic_pdu_group_.size());

    for (auto& pdu_resolved_key : transfer_atomic_pdu_group_) {
#ifdef ENABLE_DEBUG_MESSAGES
        std::cout << "INFO: Bridge atomic group transfer triggered: "
                  << " src=" << src_endpoint_->get_name()
                  << " dst=" << dst_endpoint_->get_name()
                  << " robot=" << pdu_resolved_key->robot
                  << " channel=" << pdu_resolved_key->channel_id
                  << std::endl;
#endif
        // Read PDU
        std::string pdu_name = src_endpoint_->get_pdu_name(*pdu_resolved_key);
        size_t pdu_size = src_endpoint_->get_pdu_size(
            {pdu_resolved_key->robot, pdu_name}
        );
        if (pdu_size == 0) {
            std::cerr << "ERROR: PDU size is 0 for " << pdu_resolved_key->robot 
                      << "." << pdu_name << ". Skipping transfer." << std::endl;
            continue;
        }
        std::vector<std::byte> buffer(pdu_size); // Use std::byte for raw PDU data
        size_t received_size = 0;
        // Read from source endpoint
        HakoPduErrorType read_err = src_endpoint_->recv(
            *pdu_resolved_key, std::span<std::byte>(buffer), received_size
        );
        if (read_err != HAKO_PDU_ERR_OK) {
            std::cerr << "ERROR: Failed to read PDU " << pdu_resolved_key->robot 
                      << "." << pdu_name << " from source: " << read_err << std::endl;
            continue;
        }
        if (received_size != pdu_size) {
             std::cerr << "WARNING: PDU " << pdu_resolved_key->robot 
                      << "." << pdu_name << " read " << received_size 
                      << " bytes, expected " << pdu_size << std::endl;
            continue;
        }

        if (epoch_validation_) {
            uint8_t pdu_epoch = 0;
            if (hako_pdu_get_epoch(buffer.data(), &pdu_epoch) != 0) {
                std::cerr << "ERROR: Failed to get epoch from PDU "
                          << pdu_resolved_key->robot << "." << pdu_name << std::endl;
                return;
            }
            if (pdu_epoch != owner_epoch_.load(std::memory_order_relaxed)) {
                #ifdef ENABLE_DEBUG_MESSAGES
                std::cout << "DEBUG: Discarding atomic group (epoch " << static_cast<int>(pdu_epoch)
                          << ", owner " << static_cast<int>(owner_epoch_.load(std::memory_order_relaxed)) << ")" << std::endl;
                #endif
                return;
            }
        }
        buffers.push_back(PduBuffer{*pdu_resolved_key, pdu_name, std::move(buffer)});
    }

    for (const auto& entry : buffers) {
        // write to destination endpoint
        HakoPduErrorType write_err = dst_endpoint_->send(
            entry.key, std::span<const std::byte>(entry.data)
        );
        if (write_err != HAKO_PDU_ERR_OK) {
            std::cerr << "ERROR: Failed to write PDU " << entry.key.robot
                      << "." << entry.pdu_name << " to destination: " << write_err << std::endl;
            continue;
        }
        dst_endpoint_->process_recv_events(); // Ensure the destination processes the received PDU
    }
#ifdef ENABLE_DEBUG_MESSAGES
    std::cout << "INFO: Bridge atomic group transfer completed: "
              << " bytes=" << transfer_atomic_pdu_group_.size()
              << " src=" << src_endpoint_->get_name()
              << " dst=" << dst_endpoint_->get_name()
              << std::endl;
#endif
}
