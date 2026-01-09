#include "hakoniwa/pdu/bridge/bridge_builder.hpp"
#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/endpoint.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <vector>
#include <cstddef>

namespace hakoniwa::pdu::bridge::test {

namespace {
    std::string config_path(const std::string& filename) {
        return (std::filesystem::path(TEST_CONFIG_DIR) / filename).string();
    }
}

TEST(BridgeCoreFlowTest, ImmediatePolicyFlow) {
    // 1. Setup
    std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container = 
        std::make_shared<hakoniwa::pdu::EndpointContainer>("node1", config_path("endpoints.json"));
    HakoPduErrorType init_ret = endpoint_container->initialize();
    ASSERT_EQ(init_ret, HAKO_PDU_ERR_OK);

    auto result = hakoniwa::pdu::bridge::build(config_path("bridge-core-flow-test.json"), "node1", 1000, endpoint_container);
    ASSERT_TRUE(result.ok()) << result.error_message;
    auto bridge_core = std::move(result.core);

    ASSERT_TRUE(bridge_core != nullptr);
    ASSERT_EQ(endpoint_container->list_endpoint_ids().size(), 2U);

    ASSERT_EQ(endpoint_container->start_all(), HAKO_PDU_ERR_OK);

    bridge_core->start();

    auto src_ep = endpoint_container->ref("n1-epSrc");
    auto dst_ep = endpoint_container->ref("n1-epDst");

    // 2. Execution
    hakoniwa::pdu::PduKey key = {"Drone", "pos"};
    size_t pdu_size = src_ep->get_pdu_size(key);
    ASSERT_EQ(pdu_size, 72U);

    std::vector<std::byte> send_pdu(pdu_size);
    // Fill with some test data
    for (size_t i = 0; i < pdu_size; ++i) {
        send_pdu[i] = std::byte(i % 256);
    }

    // Write data directly to the source endpoint's cache
    auto send_ret = src_ep->send(key, send_pdu);
    ASSERT_EQ(send_ret, HAKO_PDU_ERR_OK);

    // Execute one step of the bridge logic
    bool success = bridge_core->advance_timestep();
    ASSERT_TRUE(success);

    // 3. Assertion
    std::vector<std::byte> recv_pdu(pdu_size);
    size_t received_size = 0;
    
    // Read data directly from the destination endpoint's cache
    auto recv_ret = dst_ep->recv(key, recv_pdu, received_size);
    ASSERT_EQ(recv_ret, HAKO_PDU_ERR_OK);

    ASSERT_EQ(received_size, pdu_size);
    ASSERT_EQ(send_pdu, recv_pdu);
}

}
