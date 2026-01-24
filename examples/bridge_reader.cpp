#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/endpoint_types.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>
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

    hakoniwa::pdu::Endpoint endpoint("bridge_reader", HAKO_PDU_ENDPOINT_DIRECTION_IN);
    HakoPduErrorType err = endpoint.open(endpoint_config_path);
    if (err != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to open endpoint: " << err << std::endl;
        return 1;
    }
    const hakoniwa::pdu::PduKey pdu_key{robot, pdu};
    const std::size_t pdu_size = endpoint.get_pdu_size(pdu_key);
    if (pdu_size == 0) {
        std::cerr << "PDU size is 0 for " << robot << "." << pdu << std::endl;
        return 1;
    }

    const auto channel_id = endpoint.get_pdu_channel_id(pdu_key);
    if (channel_id < 0) {
        std::cerr << "Failed to resolve channel for " << robot << "." << pdu << std::endl;
        return 1;
    }
    const hakoniwa::pdu::PduResolvedKey resolved_key{robot, channel_id};

    endpoint.subscribe_on_recv_callback(
        resolved_key,
        [pdu_size](const hakoniwa::pdu::PduResolvedKey& pdu_key, std::span<const std::byte> data) {
            (void)pdu_key;
            const std::size_t size = std::min(pdu_size, data.size());
            const auto begin = reinterpret_cast<const char*>(data.data());
            const auto end = begin + size;
            const auto nul = std::find(begin, end, '\0');
            std::string text(begin, nul);
            std::cout << "recv bytes=" << size << " text=\"" << text << "\"" << std::endl;
        });

    err = endpoint.start();
    if (err != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to start endpoint: " << err << std::endl;
        return 1;
    }
    while (true) {
        endpoint.process_recv_events();
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}
