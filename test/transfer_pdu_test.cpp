#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/pdu/bridge/policy/throttle_policy.hpp"
#include "hakoniwa/pdu/bridge/policy/ticker_policy.hpp"
#include "hakoniwa/pdu/bridge/transfer_pdu.hpp"
#include "hakoniwa/pdu/bridge/virtual_time_source.hpp"
#include "hakoniwa/pdu/endpoint_types.hpp"

#include "mock_endpoint.hpp"

#include <gtest/gtest.h>

#include <chrono>
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

PduKey make_config_key() {
    return PduKey{"Robot1.pos", "Robot1", "pos"};
}
} // namespace

TEST(TransferPduTest, ImmediatePolicyTransfersAndAcceptsEpoch) {
    auto time_source = std::make_shared<VirtualTimeSource>();
    auto policy = std::make_shared<ImmediatePolicy>();
    auto src = std::make_shared<test_support::MockEndpoint>("src");
    auto dst = std::make_shared<test_support::MockEndpoint>("dst");

    auto payload = make_payload(1U, 16U);
    src->set_pdu_data(make_endpoint_key("Robot1", "pos"), payload);

    TransferPdu transfer_pdu(make_config_key(), policy, src, dst);
    transfer_pdu.try_transfer(time_source);

    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 1U);
    EXPECT_EQ(dst->last_sent_data(make_endpoint_key("Robot1", "pos")), payload);
}

TEST(TransferPduTest, ImmediatePolicyRejectsOlderEpoch) {
    auto time_source = std::make_shared<VirtualTimeSource>();
    auto policy = std::make_shared<ImmediatePolicy>();
    auto src = std::make_shared<test_support::MockEndpoint>("src");
    auto dst = std::make_shared<test_support::MockEndpoint>("dst");

    auto payload = make_payload(1U, 16U);
    src->set_pdu_data(make_endpoint_key("Robot1", "pos"), payload);

    TransferPdu transfer_pdu(make_config_key(), policy, src, dst);
    transfer_pdu.set_epoch(2U);
    transfer_pdu.try_transfer(time_source);

    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 0U);
}

TEST(TransferPduTest, ThrottlePolicyTransfersOnInterval) {
    auto time_source = std::make_shared<VirtualTimeSource>();
    auto policy = std::make_shared<ThrottlePolicy>(100000); // 100ms
    auto src = std::make_shared<test_support::MockEndpoint>("src");
    auto dst = std::make_shared<test_support::MockEndpoint>("dst");

    auto payload = make_payload(1U, 16U);
    src->set_pdu_data(make_endpoint_key("Robot1", "pos"), payload);

    TransferPdu transfer_pdu(make_config_key(), policy, src, dst);
    transfer_pdu.try_transfer(time_source);
    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 1U);

    transfer_pdu.try_transfer(time_source);
    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 1U);

    time_source->advance_time(50000);
    transfer_pdu.try_transfer(time_source);
    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 1U);

    time_source->advance_time(50000);
    transfer_pdu.try_transfer(time_source);
    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 2U);
}

TEST(TransferPduTest, TickerPolicyTransfersOnTicks) {
    auto time_source = std::make_shared<VirtualTimeSource>();
    auto policy = std::make_shared<TickerPolicy>(std::chrono::milliseconds(100));
    auto src = std::make_shared<test_support::MockEndpoint>("src");
    auto dst = std::make_shared<test_support::MockEndpoint>("dst");

    auto payload = make_payload(1U, 16U);
    src->set_pdu_data(make_endpoint_key("Robot1", "pos"), payload);

    TransferPdu transfer_pdu(make_config_key(), policy, src, dst);
    transfer_pdu.try_transfer(time_source);
    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 0U);

    time_source->advance_time(100'000U);
    transfer_pdu.try_transfer(time_source);
    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 1U);

    transfer_pdu.try_transfer(time_source);
    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 1U);

    time_source->advance_time(100'000U);
    transfer_pdu.try_transfer(time_source);
    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 2U);
}

TEST(TransferPduTest, ImmediatePolicyNoPdu) {
    auto time_source = std::make_shared<VirtualTimeSource>();
    auto policy = std::make_shared<ImmediatePolicy>();
    auto src = std::make_shared<test_support::MockEndpoint>("src");
    auto dst = std::make_shared<test_support::MockEndpoint>("dst");

    TransferPdu transfer_pdu(make_config_key(), policy, src, dst);
    transfer_pdu.try_transfer(time_source);

    EXPECT_EQ(dst->send_count(make_endpoint_key("Robot1", "pos")), 0U);
}

} // namespace hakoniwa::pdu::bridge::test
