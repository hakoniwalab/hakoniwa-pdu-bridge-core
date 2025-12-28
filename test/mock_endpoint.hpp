#pragma once

#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/endpoint_types.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace hako::pdu::bridge::test_support {

class MockEndpoint : public hakoniwa::pdu::Endpoint {
public:
    explicit MockEndpoint(const std::string& name)
        : hakoniwa::pdu::Endpoint(name, HAKO_PDU_ENDPOINT_DIRECTION_INOUT)
        , name_(name) {
    }

    void set_pdu_data(const hakoniwa::pdu::PduKey& key, std::vector<std::byte> data) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string map_key = make_key(key);
        pdu_data_[map_key] = std::move(data);
    }

    size_t get_pdu_size(const hakoniwa::pdu::PduKey& key) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string map_key = make_key(key);
        auto it = pdu_data_.find(map_key);
        if (it == pdu_data_.end()) {
            return 0;
        }
        return it->second.size();
    }

    HakoPduErrorType recv(const hakoniwa::pdu::PduKey& key,
                          std::span<std::byte> buffer,
                          size_t& received_size) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string map_key = make_key(key);
        auto it = pdu_data_.find(map_key);
        if (it == pdu_data_.end()) {
            received_size = 0;
            return HAKO_PDU_ERR_OK;
        }
        received_size = std::min(buffer.size(), it->second.size());
        std::memcpy(buffer.data(), it->second.data(), received_size);
        return HAKO_PDU_ERR_OK;
    }

    HakoPduErrorType send(const hakoniwa::pdu::PduKey& key,
                          std::span<const std::byte> buffer) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string map_key = make_key(key);
        last_sent_data_[map_key] = std::vector<std::byte>(buffer.begin(), buffer.end());
        send_counts_[map_key] += 1;
        return HAKO_PDU_ERR_OK;
    }

    size_t send_count(const hakoniwa::pdu::PduKey& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string map_key = make_key(key);
        auto it = send_counts_.find(map_key);
        if (it == send_counts_.end()) {
            return 0;
        }
        return it->second;
    }

    std::vector<std::byte> last_sent_data(const hakoniwa::pdu::PduKey& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string map_key = make_key(key);
        auto it = last_sent_data_.find(map_key);
        if (it == last_sent_data_.end()) {
            return {};
        }
        return it->second;
    }

    const std::string& get_name() const override {
        return name_;
    }

private:
    static std::string make_key(const hakoniwa::pdu::PduKey& key) {
        return key.robot + "." + key.pdu;
    }

    std::string name_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<std::byte>> pdu_data_;
    std::unordered_map<std::string, std::vector<std::byte>> last_sent_data_;
    std::unordered_map<std::string, size_t> send_counts_;
};

} // namespace hako::pdu::bridge::test_support
