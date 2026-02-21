#include "hakoniwa/pdu/bridge/monitor_cli_utils.hpp"
#include "hakoniwa/pdu/pdu_primitive_ctypes.h"
#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace hakoniwa::pdu::bridge::test {

TEST(MonitorCliUtilsTest, ControlApiResponseParsing)
{
    const nlohmann::json health_res = {
        {"type", "health"},
        {"health", {{"running", true}, {"uptime_usec", 1234}, {"last_error", ""}}}
    };
    const auto health = monitor_cli::parse_health(health_res);
    ASSERT_TRUE(health.has_value());
    EXPECT_TRUE(health->running);
    EXPECT_EQ(health->uptime_usec, 1234);

    const nlohmann::json connections_res = {
        {"type", "connections"},
        {"connections", nlohmann::json::array({
            {{"connection_id", "conn1"}, {"node_id", "node1"}, {"active", true}, {"epoch", 2}, {"epoch_validation", true}}
        })}
    };
    const auto connections = monitor_cli::parse_connections(connections_res);
    ASSERT_TRUE(connections.has_value());
    ASSERT_EQ(connections->size(), 1);
    EXPECT_EQ(connections->at(0).connection_id, "conn1");
    EXPECT_EQ(connections->at(0).epoch, 2);

    const nlohmann::json sessions_res = {
        {"type", "sessions"},
        {"sessions", nlohmann::json::array({
            {{"session_id", "ms-1"}, {"connection_id", "conn1"}, {"policy", {{"type", "throttle"}}}, {"state", "Active"}}
        })}
    };
    const auto sessions = monitor_cli::parse_sessions(sessions_res);
    ASSERT_TRUE(sessions.has_value());
    ASSERT_EQ(sessions->size(), 1);
    EXPECT_EQ(sessions->at(0).policy_type, "throttle");

    const nlohmann::json pdus_res = {
        {"type", "pdus"},
        {"connection_id", "conn1"},
        {"pdus", nlohmann::json::array({
            {{"robot", "Drone"}, {"pdu_name", "pos"}, {"channel_id", 1}}
        })}
    };
    const auto pdus = monitor_cli::parse_pdus(pdus_res);
    ASSERT_TRUE(pdus.has_value());
    ASSERT_EQ(pdus->size(), 1);
    EXPECT_EQ(pdus->at(0).robot, "Drone");
    EXPECT_EQ(pdus->at(0).channel_id, 1);
}

TEST(MonitorCliUtilsTest, TailLineHasRobotChannelAndSize)
{
    const hakoniwa::pdu::PduResolvedKey key{"Drone", 101};
    const auto line = monitor_cli::format_tail_line(1000, key, 72, "pos", "N/A");
    EXPECT_NE(line.find("[monitor-data]"), std::string::npos);
    EXPECT_NE(line.find("robot: Drone"), std::string::npos);
    EXPECT_NE(line.find("channel_id: 101"), std::string::npos);
    EXPECT_NE(line.find("payload_size: 72"), std::string::npos);
}

TEST(MonitorCliUtilsTest, PduNameResolutionWithAndWithoutMapping)
{
    std::unordered_map<std::string, std::string> mapping;
    mapping.emplace("Drone:1", "pos");

    EXPECT_EQ(monitor_cli::resolve_pdu_name("", "Drone", 1, mapping), "pos");
    EXPECT_EQ(monitor_cli::resolve_pdu_name("vel", "Drone", 1, mapping), "vel");
    EXPECT_EQ(monitor_cli::resolve_pdu_name("", "Unknown", 999, mapping), "N/A");
}

TEST(MonitorCliUtilsTest, EpochExtractionSupportedAndUnsupported)
{
    void* base_ptr = hako_create_empty_pdu(8, 0);
    ASSERT_NE(base_ptr, nullptr);
    void* top_ptr = hako_get_top_ptr_pdu(base_ptr);
    ASSERT_NE(top_ptr, nullptr);
    ASSERT_EQ(hako_pdu_set_epoch(top_ptr, 7), 0);

    auto* meta = hako_get_pdu_meta_data(base_ptr);
    ASSERT_NE(meta, nullptr);
    std::vector<std::byte> valid_pdu(meta->total_size);
    std::memcpy(valid_pdu.data(), top_ptr, valid_pdu.size());
    EXPECT_EQ(monitor_cli::try_get_epoch(valid_pdu), "7");
    ASSERT_EQ(hako_destroy_pdu(base_ptr), 0);

    const std::vector<std::byte> invalid_payload(16, std::byte{0});
    EXPECT_EQ(monitor_cli::try_get_epoch(invalid_payload), "N/A");
}

} // namespace hakoniwa::pdu::bridge::test
