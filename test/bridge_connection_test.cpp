#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/pdu/bridge/transfer_pdu.hpp"
#include "hakoniwa/pdu/bridge/virtual_time_source.hpp"

#include "mock_endpoint.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace hakoniwa::pdu::bridge::test {

namespace {
std::vector<std::byte> make_payload(uint64_t epoch, size_t size) {
    std::vector<std::byte> payload(size);
    std::memcpy(payload.data(), &epoch, sizeof(epoch));
    return payload;
}

hakoniwa::pdu::PduKey make_endpoint_key(const std::string& robot, const std::string& pdu) {
    return hakoniwa::pdu::PduKey{robot, pdu};
}
} // namespace

TEST(BridgeConnectionTest, StepTransfersAllPdus) {
    auto time_source = std::make_shared<VirtualTimeSource>();
    auto policy = std::make_shared<ImmediatePolicy>();
    auto src = std::make_shared<test_support::MockEndpoint>("src");
    auto dst = std::make_shared<test_support::MockEndpoint>("dst");

    src->set_pdu_data(make_endpoint_key("Robot1", "pos"), make_payload(1U, 16U));
    src->set_pdu_data(make_endpoint_key("Robot1", "status"), make_payload(1U, 16U));

    BridgeConnection connection("node1");
    connection.add_transfer_pdu(std::make_unique<TransferPdu>(
        PduKey{"Robot1.pos", "Robot1", "pos"}, policy, src, dst));
    connection.add_transfer_pdu(std::make_unique<TransferPdu>(
        PduKey{"Robot1.status", "Robot1", "status"}, policy, src, dst));

    connection.step(time_source);

    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 1U);
    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "status")), 1U);
}

TEST(BridgeConnectionTest, StepTransfersToMultipleDestinations) {
    auto time_source = std::make_shared<VirtualTimeSource>();
    auto policy = std::make_shared<ImmediatePolicy>();
    auto src = std::make_shared<test_support::MockEndpoint>("src");
    auto dst1 = std::make_shared<test_support::MockEndpoint>("dst1");
    auto dst2 = std::make_shared<test_support::MockEndpoint>("dst2");

    src->set_pdu_data(make_endpoint_key("Robot1", "pos"), make_payload(1U, 16U));
    src->set_pdu_data(make_endpoint_key("Robot1", "status"), make_payload(1U, 16U));

    BridgeConnection connection("node1");
    connection.add_transfer_pdu(std::make_unique<TransferPdu>(
        PduKey{"Robot1.pos", "Robot1", "pos"}, policy, src, dst1));
    connection.add_transfer_pdu(std::make_unique<TransferPdu>(
        PduKey{"Robot1.status", "Robot1", "status"}, policy, src, dst2));

    connection.step(time_source);

    EXPECT_EQ(dst1->send_count(make_endpoint_key("Robot1", "pos")), 1U);
    EXPECT_EQ(dst1->send_count(make_endpoint_key("Robot1", "status")), 0U);
    EXPECT_EQ(dst2->send_count(make_endpoint_key("Robot1", "pos")), 0U);
    EXPECT_EQ(dst2->send_count(make_endpoint_key("Robot1", "status")), 1U);
}

} // namespace hakoniwa::pdu::bridge::test
