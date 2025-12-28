#pragma once

#include "hakoniwa/pdu/bridge/time_source.hpp"

namespace hako::pdu::bridge {

class RealTimeSource : public ITimeSource {
public:
    RealTimeSource()
      : start_us_(now_us()) {}


    uint64_t get_microseconds() const override {
        return now_us() - start_us_; // “起点からの経過us”
    }

private:
    static uint64_t now_us() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }

    uint64_t start_us_;
};


} // namespace hako::pdu::bridge
