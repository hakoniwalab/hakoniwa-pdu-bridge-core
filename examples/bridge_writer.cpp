#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/endpoint_types.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {
void print_usage(const char* argv0)
{
    std::cerr << "Usage: " << argv0
              << " <endpoint.json> <robot> <pdu> [interval_ms]" << std::endl;
}
}

int main(int argc, char* argv[])
{
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string endpoint_config_path = argv[1];
    const std::string robot = argv[2];
    const std::string pdu = argv[3];
    int interval_ms = 10;
    if (argc >= 5) {
        interval_ms = std::max(1, std::atoi(argv[4]));
    }

    hakoniwa::pdu::Endpoint endpoint("bridge_writer", HAKO_PDU_ENDPOINT_DIRECTION_OUT);
    HakoPduErrorType err = endpoint.open(endpoint_config_path);
    if (err != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to open endpoint: " << err << std::endl;
        return 1;
    }
    err = endpoint.start();
    if (err != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to start endpoint: " << err << std::endl;
        return 1;
    }

    const hakoniwa::pdu::PduKey pdu_key{robot, pdu};
    const std::size_t pdu_size = endpoint.get_pdu_size(pdu_key);
    if (pdu_size == 0) {
        std::cerr << "PDU size is 0 for " << robot << "." << pdu << std::endl;
        return 1;
    }

    std::vector<std::byte> buffer(pdu_size);

    std::uint32_t seq = 0;
    while (true) {
        std::memset(buffer.data(), 0, buffer.size());
        const auto now = std::chrono::system_clock::now();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() % 1000;
        const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#if defined(_WIN32)
        localtime_s(&tm_buf, &now_time);
#else
        localtime_r(&now_time, &tm_buf);
#endif
        char time_text[64];
        std::strftime(time_text, sizeof(time_text), "%Y-%m-%dT%H:%M:%S", &tm_buf);
        const int written = std::snprintf(
            reinterpret_cast<char*>(buffer.data()),
            buffer.size(),
            "ts=%s.%03lld seq=%u",
            time_text,
            static_cast<long long>(millis),
            seq);
        std::string payload;
        if (written > 0) {
            const std::size_t len = std::min<std::size_t>(static_cast<std::size_t>(written), buffer.size() - 1);
            payload.assign(reinterpret_cast<const char*>(buffer.data()), len);
        }
        (void)endpoint.send(pdu_key, std::span<const std::byte>(buffer));
        std::cout << "sent seq=" << seq << " bytes=" << pdu_size << " text=\"" << payload << "\"" << std::endl;
        ++seq;
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}
