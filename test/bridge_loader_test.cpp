#include <optional>
#include "hakoniwa/pdu/bridge/bridge_builder.hpp"
#include "hakoniwa/pdu/bridge/bridge_types.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

namespace hakoniwa::pdu::bridge::test {

namespace {
std::filesystem::path test_config_root() {
    if (const char* config_dir = std::getenv("HAKO_TEST_CONFIG_DIR"); config_dir && *config_dir) {
        return std::filesystem::path(config_dir);
    }
    return std::filesystem::path(TEST_CONFIG_DIR);
}

std::string config_path(const std::string& filename) {
    return (test_config_root() / "loader" / filename).string();
}
} // namespace

TEST(BridgeLoaderTest, LoadsImmediateConfig) {
    std::string error_message;
    auto config_ = hakoniwa::pdu::bridge::parse(config_path("bridge-immediate.json"), error_message);
    BridgeConfig config = *config_;

    EXPECT_EQ(config.version, "2.0.0");
    ASSERT_EQ(config.transferPolicies.size(), 1U);
    EXPECT_EQ(config.transferPolicies.at("immediate_policy").type, "immediate");
    EXPECT_FALSE(config.transferPolicies.at("immediate_policy").intervalMs.has_value());
    EXPECT_EQ(config.nodes.size(), 1U);
    ASSERT_TRUE(config.endpoints_config_path.has_value());
    EXPECT_EQ(*config.endpoints_config_path, "endpoints-immediate.json");
    EXPECT_EQ(config.pduKeyGroups.at("group1").front().id, "Robot1.pos");
}

TEST(BridgeLoaderTest, LoadsThrottleConfig) {
    std::string error_message;
    auto config_ = hakoniwa::pdu::bridge::parse(config_path("bridge-throttle.json"), error_message);
    BridgeConfig config = *config_;

    EXPECT_EQ(config.transferPolicies.at("throttle_policy").type, "throttle");
    ASSERT_TRUE(config.transferPolicies.at("throttle_policy").intervalMs.has_value());
    EXPECT_EQ(config.connections.size(), 1U);
    ASSERT_TRUE(config.endpoints_config_path.has_value());
    EXPECT_EQ(*config.endpoints_config_path, "endpoints-throttle.json");
}

TEST(BridgeLoaderTest, LoadsTickerConfig) {
    std::string error_message;
    auto config_ = hakoniwa::pdu::bridge::parse(config_path("bridge-ticker.json"), error_message);
    BridgeConfig config = *config_;

    EXPECT_EQ(config.transferPolicies.at("ticker_policy").type, "ticker");
    ASSERT_TRUE(config.transferPolicies.at("ticker_policy").intervalMs.has_value());
    EXPECT_EQ(*config.transferPolicies.at("ticker_policy").intervalMs, 50);
    EXPECT_EQ(config.connections.front().transferPdus.front().policyId, "ticker_policy");
    ASSERT_TRUE(config.endpoints_config_path.has_value());
    EXPECT_EQ(*config.endpoints_config_path, "endpoints-ticker.json");
}

TEST(BridgeLoaderTest, LoadsTickerConfig2) {
    std::string error_message;
    auto config_ = hakoniwa::pdu::bridge::parse(config_path("bridge-ticker2.json"), error_message);
    BridgeConfig config = *config_;

    EXPECT_EQ(config.version, "2.0.0");
    
    EXPECT_EQ(config.nodes.size(), 1U);
    EXPECT_EQ(config.nodes[0].id, "node1");

    ASSERT_TRUE(config.endpoints_config_path.has_value());
    EXPECT_EQ(*config.endpoints_config_path, "endpoints-ticker2.json");

    EXPECT_EQ(config.connections.size(), 1U);
    EXPECT_EQ(config.connections[0].id, "conn1");
    EXPECT_EQ(config.connections[0].nodeId, "node1");
    EXPECT_EQ(config.connections[0].source.endpointId, "ep1");
    EXPECT_EQ(config.connections[0].destinations.size(), 1U);
    EXPECT_EQ(config.connections[0].destinations[0].endpointId, "ep2");
    EXPECT_EQ(config.connections[0].transferPdus.size(), 1U);
    EXPECT_EQ(config.connections[0].transferPdus[0].pduKeyGroupId, "pdu_group1");
    EXPECT_EQ(config.connections[0].transferPdus[0].policyId, "ticker1");

    EXPECT_EQ(config.pduKeyGroups.size(), 1U);
    EXPECT_TRUE(config.pduKeyGroups.count("pdu_group1"));
    EXPECT_EQ(config.pduKeyGroups.at("pdu_group1").size(), 1U);
    EXPECT_EQ(config.pduKeyGroups.at("pdu_group1")[0].id, "Robot1.camera");
}

TEST(BridgeLoaderTest, LoadsThrottleConfig2) {
    std::string error_message;
    auto config_ = hakoniwa::pdu::bridge::parse(config_path("bridge-throttle2.json"), error_message);
    BridgeConfig config = *config_;

    EXPECT_EQ(config.version, "2.0.0");
    
    EXPECT_EQ(config.transferPolicies.size(), 1U);
    EXPECT_TRUE(config.transferPolicies.count("throttle1"));
    const auto& policy = config.transferPolicies.at("throttle1");
    EXPECT_EQ(policy.type, "throttle");
    EXPECT_EQ(*policy.intervalMs, 10);

    EXPECT_EQ(config.nodes.size(), 1U);
    EXPECT_EQ(config.nodes[0].id, "node1");

    ASSERT_TRUE(config.endpoints_config_path.has_value());
    EXPECT_EQ(*config.endpoints_config_path, "endpoints-throttle2.json");

    EXPECT_EQ(config.connections.size(), 1U);
    EXPECT_EQ(config.connections[0].id, "conn1");
    EXPECT_EQ(config.connections[0].nodeId, "node1");
    EXPECT_EQ(config.connections[0].source.endpointId, "ep1");
    EXPECT_EQ(config.connections[0].destinations.size(), 1U);
    EXPECT_EQ(config.connections[0].destinations[0].endpointId, "ep2");
    EXPECT_EQ(config.connections[0].transferPdus.size(), 1U);
    EXPECT_EQ(config.connections[0].transferPdus[0].pduKeyGroupId, "pdu_group1");
    EXPECT_EQ(config.connections[0].transferPdus[0].policyId, "throttle1");

    EXPECT_EQ(config.pduKeyGroups.size(), 1U);
    EXPECT_TRUE(config.pduKeyGroups.count("pdu_group1"));
    EXPECT_EQ(config.pduKeyGroups.at("pdu_group1").size(), 1U);
    EXPECT_EQ(config.pduKeyGroups.at("pdu_group1")[0].id, "Robot1.camera");
}

TEST(BridgeLoaderTest, LoadsMultipleConfig) {
    std::string error_message;
    auto config_ = hakoniwa::pdu::bridge::parse(config_path("bridge-multiple.json"), error_message);
    BridgeConfig config = *config_;

    EXPECT_EQ(config.version, "2.0.0");
    
    EXPECT_EQ(config.transferPolicies.size(), 2U);
    EXPECT_TRUE(config.transferPolicies.count("immediate"));
    EXPECT_TRUE(config.transferPolicies.count("throttle1"));

    EXPECT_EQ(config.nodes.size(), 2U);
    EXPECT_EQ(config.nodes[0].id, "node1");
    EXPECT_EQ(config.nodes[1].id, "node2");

    ASSERT_TRUE(config.endpoints_config_path.has_value());
    EXPECT_EQ(*config.endpoints_config_path, "endpoints-multiple.json");

    EXPECT_EQ(config.connections.size(), 2U);
    EXPECT_EQ(config.connections[0].id, "conn1");
    EXPECT_EQ(config.connections[0].nodeId, "node1");
    EXPECT_EQ(config.connections[0].source.endpointId, "ep1");
    EXPECT_EQ(config.connections[0].destinations.size(), 1U);
    EXPECT_EQ(config.connections[0].destinations[0].endpointId, "ep2");
    EXPECT_EQ(config.connections[0].transferPdus.size(), 1U);
    EXPECT_EQ(config.connections[0].transferPdus[0].pduKeyGroupId, "pdu_group1");
    EXPECT_EQ(config.connections[0].transferPdus[0].policyId, "immediate");
    EXPECT_EQ(config.connections[1].id, "conn2");
    EXPECT_EQ(config.connections[1].nodeId, "node2");
    EXPECT_EQ(config.connections[1].source.endpointId, "ep3");
    EXPECT_EQ(config.connections[1].destinations.size(), 1U);
    EXPECT_EQ(config.connections[1].destinations[0].endpointId, "ep4");
    EXPECT_EQ(config.connections[1].transferPdus.size(), 1U);
    EXPECT_EQ(config.connections[1].transferPdus[0].pduKeyGroupId, "pdu_group2");
    EXPECT_EQ(config.connections[1].transferPdus[0].policyId, "throttle1");


    EXPECT_EQ(config.pduKeyGroups.size(), 2U);
    EXPECT_TRUE(config.pduKeyGroups.count("pdu_group1"));
    EXPECT_EQ(config.pduKeyGroups.at("pdu_group1").size(), 1U);
    EXPECT_EQ(config.pduKeyGroups.at("pdu_group1")[0].id, "Robot1.camera");
    EXPECT_TRUE(config.pduKeyGroups.count("pdu_group2"));
    EXPECT_EQ(config.pduKeyGroups.at("pdu_group2").size(), 1U);
    EXPECT_EQ(config.pduKeyGroups.at("pdu_group2")[0].id, "Robot2.camera");
}

TEST(BridgeLoaderTest, LoadsInvalidConfig) {
    std::string error_message;
    auto config_ = hakoniwa::pdu::bridge::parse(config_path("bridge-invalid.json"), error_message);
    ASSERT_FALSE(config_.has_value());
}

} // namespace hakoniwa::pdu::bridge::test
