#pragma once

#include "hakoniwa/pdu/bridge/time_source.hpp"

namespace hako::pdu::bridge {

class RealTimeSource : public ITimeSource {
public:
    RealTimeSource() : start_time_(std::chrono::steady_clock::now()) {}

    virtual std::chrono::steady_clock::time_point get_steady_clock_time() const override {
        return std::chrono::steady_clock::now();
    }

    virtual uint64_t get_microseconds() const override {
        auto duration = std::chrono::steady_clock::now() - start_time_;
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }

private:
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace hako::pdu::bridge
