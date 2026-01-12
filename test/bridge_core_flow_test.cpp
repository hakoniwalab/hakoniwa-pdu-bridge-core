#include "hakoniwa/pdu/bridge/bridge_builder.hpp"
#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/time_source/time_source_factory.hpp"
#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <cstddef>

namespace hakoniwa::pdu::bridge::test {

namespace {
    std::filesystem::path test_config_root() {
        if (const char* config_dir = std::getenv("HAKO_TEST_CONFIG_DIR"); config_dir && *config_dir) {
            return std::filesystem::path(config_dir);
        }
        return std::filesystem::path(TEST_CONFIG_DIR);
    }

    std::string config_path(const std::string& filename) {
        return (test_config_root() / "core_flow" / filename).string();
    }
}

TEST(BridgeCoreFlowTest, ImmediatePolicyFlow) {
    // 1. Setup
    std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container = 
        std::make_shared<hakoniwa::pdu::EndpointContainer>("node1", config_path("endpoints.json"));
    HakoPduErrorType init_ret = endpoint_container->initialize();
    ASSERT_EQ(init_ret, HAKO_PDU_ERR_OK);

    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source = 
        hakoniwa::time_source::create_time_source("real", 1000);

    auto result = hakoniwa::pdu::bridge::build(config_path("bridge-core-flow-test.json"), "node1", time_source, endpoint_container);
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
    bool success = bridge_core->cyclic_trigger();
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

TEST(BridgeCoreFlowTest, AtomicPolicyFlow) {
    // 1. Setup
    std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container = 
        std::make_shared<hakoniwa::pdu::EndpointContainer>("node1", config_path("atomic_endpoints.json"));
    HakoPduErrorType init_ret = endpoint_container->initialize();
    if (init_ret != HAKO_PDU_ERR_OK)
        std::cout << "error message: " << endpoint_container->last_error() << std::endl;
    ASSERT_EQ(init_ret, HAKO_PDU_ERR_OK);
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source = 
        hakoniwa::time_source::create_time_source("real", 1000);

    auto result = hakoniwa::pdu::bridge::build(config_path("bridge-atomic-core-test.json"), "node1", time_source, endpoint_container);
    ASSERT_TRUE(result.ok()) << result.error_message;
    auto bridge_core = std::move(result.core);

    ASSERT_TRUE(bridge_core != nullptr);
    ASSERT_EQ(endpoint_container->start_all(), HAKO_PDU_ERR_OK);
    bridge_core->start();

    auto src_ep = endpoint_container->ref("n1-epSrc");
    auto dst_ep = endpoint_container->ref("n1-epDst");

    // 2. Execution & Assertion
    hakoniwa::pdu::PduKey pdu1_key = {"Test", "pdu1"};
    hakoniwa::pdu::PduKey pdu2_key = {"Test", "pdu2"};
    hakoniwa::pdu::PduKey pdu3_key = {"Test", "pdu3"};
    hakoniwa::pdu::PduKey time_key = {"SimTime", "pdu"};

    std::vector<std::byte> pdu1_data(src_ep->get_pdu_size(pdu1_key), std::byte(1));
    std::vector<std::byte> pdu2_data(src_ep->get_pdu_size(pdu2_key), std::byte(2));
    std::vector<std::byte> pdu3_data(src_ep->get_pdu_size(pdu3_key), std::byte(3));
    std::vector<std::byte> time_data(src_ep->get_pdu_size(time_key), std::byte(9));

    // Send first 3 PDUs
    ASSERT_EQ(src_ep->send(pdu1_key, pdu1_data), HAKO_PDU_ERR_OK);
    ASSERT_EQ(src_ep->send(pdu2_key, pdu2_data), HAKO_PDU_ERR_OK);
    ASSERT_EQ(src_ep->send(pdu3_key, pdu3_data), HAKO_PDU_ERR_OK);

    // Advance time
    ASSERT_TRUE(bridge_core->cyclic_trigger());

    // Assert that nothing is received
    std::vector<std::byte> recv_buffer(128);
    size_t received_size = 0;
    ASSERT_EQ(dst_ep->recv(pdu1_key, recv_buffer, received_size), HAKO_PDU_ERR_NO_ENTRY);
    ASSERT_EQ(dst_ep->recv(pdu2_key, recv_buffer, received_size), HAKO_PDU_ERR_NO_ENTRY);
    ASSERT_EQ(dst_ep->recv(pdu3_key, recv_buffer, received_size), HAKO_PDU_ERR_NO_ENTRY);
    ASSERT_EQ(dst_ep->recv(time_key, recv_buffer, received_size), HAKO_PDU_ERR_NO_ENTRY);

    // Send the last PDU to complete the frame
    ASSERT_EQ(src_ep->send(time_key, time_data), HAKO_PDU_ERR_OK);

    // Advance time again
    ASSERT_TRUE(bridge_core->cyclic_trigger());

    // Assert that all PDUs are now received
    ASSERT_EQ(dst_ep->recv(pdu1_key, recv_buffer, received_size), HAKO_PDU_ERR_OK);
    ASSERT_EQ(received_size, pdu1_data.size());
    recv_buffer.resize(received_size);
    ASSERT_EQ(recv_buffer, pdu1_data);

    ASSERT_EQ(dst_ep->recv(pdu2_key, recv_buffer, received_size), HAKO_PDU_ERR_OK);
    ASSERT_EQ(received_size, pdu2_data.size());
    recv_buffer.resize(received_size);
    ASSERT_EQ(recv_buffer, pdu2_data);

    ASSERT_EQ(dst_ep->recv(pdu3_key, recv_buffer, received_size), HAKO_PDU_ERR_OK);
    ASSERT_EQ(received_size, pdu3_data.size());
    recv_buffer.resize(received_size);
    ASSERT_EQ(recv_buffer, pdu3_data);

    ASSERT_EQ(dst_ep->recv(time_key, recv_buffer, received_size), HAKO_PDU_ERR_OK);
    ASSERT_EQ(received_size, time_data.size());
    recv_buffer.resize(received_size);
    ASSERT_EQ(recv_buffer, time_data);
}

}
