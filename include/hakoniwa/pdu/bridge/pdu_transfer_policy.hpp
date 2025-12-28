#pragma once

#include <chrono>
#include <memory> // For std::shared_ptr
#include "hakoniwa/pdu/bridge/time_source.hpp" // Include the time source interface

namespace hako::pdu::bridge {

class IPduTransferPolicy {
public:
    virtual ~IPduTransferPolicy() = default;

    // Checks if a transfer should occur using the provided time source.
    virtual bool should_transfer(const std::shared_ptr<ITimeSource>& time_source) = 0;

    // Notifies the policy that a transfer has occurred, using the provided time source.
    virtual void on_transferred(const std::shared_ptr<ITimeSource>& time_source) = 0;
};

} // namespace hako::pdu::bridge


