#pragma once

#include "hakoniwa/pdu/bridge/time_source.hpp"
#include <atomic>

namespace hako::pdu::bridge {

class VirtualTimeSource : public ITimeSource {
public:
    VirtualTimeSource() : current_time_micros_(0) {}

    virtual std::chrono::steady_clock::time_point get_steady_clock_time() const override {
        // For virtual time, we can map microseconds to a steady_clock::time_point
        // The epoch of steady_clock is unspecified, so this is mainly for consistent type usage.
        return std::chrono::steady_clock::time_point(std::chrono::microseconds(current_time_micros_.load()));
    }

    virtual uint64_t get_microseconds() const override {
        return current_time_micros_.load();
    }

    virtual void advance_time(uint64_t microseconds) override {
        current_time_micros_ += microseconds;
    }

private:
    std::atomic<uint64_t> current_time_micros_;
};

} // namespace hako::pdu::bridge
