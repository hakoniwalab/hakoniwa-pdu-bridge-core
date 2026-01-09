#include "hakoniwa/pdu/bridge/bridge_builder.hpp"
#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/endpoint_container.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <vector>
#include <cstddef>
#include <thread>
#include <chrono>

namespace hakoniwa::pdu::bridge::test {

namespace {
    std::string tcp_test_config_path(const std::string& filename) {
        return (std::filesystem::path(TEST_CONFIG_DIR) / "tcp_test" / filename).string();
    }
}

TEST(BridgeTcpFlowTest, ImmediatePolicyCrossNode) {
    // 1. Setup
    auto server_endpoint_container = std::make_shared<hakoniwa::pdu::EndpointContainer>("node2", tcp_test_config_path("endpoints.json"));
    ASSERT_EQ(server_endpoint_container->initialize(), HAKO_PDU_ERR_OK);
    auto client_endpoint_container = std::make_shared<hakoniwa::pdu::EndpointContainer>("node1", tcp_test_config_path("endpoints.json"));
    ASSERT_EQ(client_endpoint_container->initialize(), HAKO_PDU_ERR_OK);

    auto server_result = hakoniwa::pdu::bridge::build(tcp_test_config_path("bridge.json"), "node2", 1000, server_endpoint_container);
    auto client_result = hakoniwa::pdu::bridge::build(tcp_test_config_path("bridge.json"), "node1", 1000, client_endpoint_container);

    auto server_core = std::move(server_result.core);
    auto client_core = std::move(client_result.core);

    ASSERT_TRUE(server_core != nullptr);
    ASSERT_TRUE(client_core != nullptr);

    ASSERT_EQ(server_endpoint_container->start_all(), HAKO_PDU_ERR_OK);
    ASSERT_EQ(client_endpoint_container->start_all(), HAKO_PDU_ERR_OK);

    while (server_endpoint_container->is_running_all() == false ||
           client_endpoint_container->is_running_all() == false) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Start server first
    server_core->start();
    client_core->start();
    while (!server_core->is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    while (!client_core->is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto src_ep = client_endpoint_container->ref("epA_src");
    auto dst_ep = server_endpoint_container->ref("epB_dst");

    // 2. Execution
    hakoniwa::pdu::PduKey key = {"Drone", "pos"};
    size_t pdu_size = src_ep->get_pdu_size(key);
    ASSERT_EQ(pdu_size, 72U);

    std::vector<std::byte> send_pdu(pdu_size);
    for (size_t i = 0; i < pdu_size; ++i) {
        send_pdu[i] = std::byte(i % 256);
    }

    HakoPduErrorType send_ret = HAKO_PDU_ERR_NOT_RUNNING;
    int retry_count = 0;
    const int max_retries = 100; // Retry for up to 100 * 1ms = 100ms
    while (send_ret != HAKO_PDU_ERR_OK && retry_count < max_retries) {
        send_ret = src_ep->send(key, send_pdu);
        if (send_ret != HAKO_PDU_ERR_OK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            retry_count++;
        }
    }
    ASSERT_EQ(send_ret, HAKO_PDU_ERR_OK) << "Failed to send PDU after " << retry_count << " retries. Error: " << send_ret;


    // Run for more timesteps to allow data to flow
    for (int i = 0; i < 50; ++i) { // Increased iterations
        client_core->advance_timestep();
        server_core->advance_timestep();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 3. Assertion
    std::vector<std::byte> recv_pdu(pdu_size);
    size_t received_size = 0;
    
    auto recv_ret = dst_ep->recv(key, recv_pdu, received_size);
    ASSERT_EQ(recv_ret, HAKO_PDU_ERR_OK);
    ASSERT_EQ(received_size, pdu_size);
    ASSERT_EQ(send_pdu, recv_pdu);

    // 4. Teardown
    client_core->stop();
    server_core->stop();
}

}
