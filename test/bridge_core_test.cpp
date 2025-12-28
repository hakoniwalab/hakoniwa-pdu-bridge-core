#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/pdu/bridge/transfer_pdu.hpp"
#include "hakoniwa/pdu/bridge/virtual_time_source.hpp"

#include "mock_endpoint.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

namespace hako::pdu::bridge::test {

namespace {
std::vector<std::byte> make_payload(uint64_t epoch, size_t size) {
    std::vector<std::byte> payload(size);
    std::memcpy(payload.data(), &epoch, sizeof(epoch));
    return payload;
}

hakoniwa::pdu::PduKey make_endpoint_key(const std::string& robot, const std::string& pdu) {
    return hakoniwa::pdu::PduKey{robot, pdu};
}

std::unique_ptr<BridgeConnection> make_connection(const std::string& node_id,
                                                  const std::shared_ptr<test_support::MockEndpoint>& src,
                                                  const std::shared_ptr<test_support::MockEndpoint>& dst,
                                                  const std::string& pdu_name) {
    auto policy = std::make_shared<ImmediatePolicy>();
    auto connection = std::make_unique<BridgeConnection>(node_id);
    connection->add_transfer_pdu(std::make_unique<TransferPdu>(
        PduKey{"Robot1." + pdu_name, "Robot1", pdu_name}, policy, src, dst));
    return connection;
}
} // namespace

TEST(BridgeCoreTest, RunLoopStepsMultipleConnections) {
    auto time_source = std::make_shared<VirtualTimeSource>();
    BridgeCore core("node1", time_source);

    auto src1 = std::make_shared<test_support::MockEndpoint>("src1");
    auto dst1 = std::make_shared<test_support::MockEndpoint>("dst1");
    src1->set_pdu_data(make_endpoint_key("Robot1", "pos"), make_payload(1U, 16U));

    auto src2 = std::make_shared<test_support::MockEndpoint>("src2");
    auto dst2 = std::make_shared<test_support::MockEndpoint>("dst2");
    src2->set_pdu_data(make_endpoint_key("Robot1", "status"), make_payload(1U, 16U));

    core.add_connection(make_connection("node1", src1, dst1, "pos"));
    core.add_connection(make_connection("node1", src2, dst2, "status"));

    std::thread runner([&core]() { core.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    core.stop();
    runner.join();

    EXPECT_GT(dst1->send_count(make_endpoint_key("Robot1", "pos")), 0U);
    EXPECT_GT(dst2->send_count(make_endpoint_key("Robot1", "status")), 0U);
}

TEST(BridgeCoreTest, RunLoopIgnoresMismatchedNodeIds) {
    auto time_source = std::make_shared<VirtualTimeSource>();
    BridgeCore core("node1", time_source);

    auto src1 = std::make_shared<test_support::MockEndpoint>("src1");
    auto dst1 = std::make_shared<test_support::MockEndpoint>("dst1");
    src1->set_pdu_data(make_endpoint_key("Robot1", "pos"), make_payload(1U, 16U));

    auto src2 = std::make_shared<test_support::MockEndpoint>("src2");
    auto dst2 = std::make_shared<test_support::MockEndpoint>("dst2");
    src2->set_pdu_data(make_endpoint_key("Robot1", "status"), make_payload(1U, 16U));

    core.add_connection(make_connection("node1", src1, dst1, "pos"));
    core.add_connection(make_connection("node2", src2, dst2, "status"));

    std::thread runner([&core]() { core.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    core.stop();
    runner.join();

    EXPECT_GT(dst1->send_count(make_endpoint_key("Robot1", "pos")), 0U);
    EXPECT_EQ(dst2->send_count(make_endpoint_key("Robot1", "status")), 0U);
}

} // namespace hako::pdu::bridge::test
