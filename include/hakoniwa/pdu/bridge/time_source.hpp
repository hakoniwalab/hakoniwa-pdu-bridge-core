#pragma once

#include <chrono>
#include <cstdint> // For uint64_t

namespace hako::pdu::bridge {

class ITimeSource {
public:
    virtual ~ITimeSource() = default;

    // Returns the current time as a steady_clock::time_point
    virtual std::chrono::steady_clock::time_point get_steady_clock_time() const = 0;

    // Returns the current time in microseconds
    virtual uint64_t get_microseconds() const = 0;

    // Optional: for virtual time, to advance time (may be implemented in derived classes)
    virtual void advance_time(uint64_t microseconds) { (void)microseconds; }
};

} // namespace hako::pdu::bridge
