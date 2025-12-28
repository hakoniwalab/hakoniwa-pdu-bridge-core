#include "hakoniwa/pdu/bridge/bridge_loader.hpp"
#include "hakoniwa/pdu/bridge/bridge_types.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace hako::pdu::bridge::test {

namespace {
std::string config_path(const std::string& filename) {
    return (std::filesystem::path(TEST_CONFIG_DIR) / filename).string();
}
} // namespace

TEST(BridgeLoaderTest, LoadsImmediateConfig) {
    BridgeConfig config = BridgeLoader::load_config(config_path("bridge-immediate.json"));

    EXPECT_EQ(config.version, "2.0.0");
    EXPECT_EQ(config.time_source_type, "virtual");
    ASSERT_EQ(config.transferPolicies.size(), 1U);
    EXPECT_EQ(config.transferPolicies.at("immediate_policy").type, "immediate");
    EXPECT_FALSE(config.transferPolicies.at("immediate_policy").intervalMs.has_value());
    EXPECT_EQ(config.nodes.size(), 1U);
    EXPECT_EQ(config.endpoints.size(), 1U);
    EXPECT_EQ(config.pduKeyGroups.at("group1").front().id, "Robot1.pos");
}

TEST(BridgeLoaderTest, LoadsThrottleConfig) {
    BridgeConfig config = BridgeLoader::load_config(config_path("bridge-throttle.json"));

    EXPECT_EQ(config.transferPolicies.at("throttle_policy").type, "throttle");
    ASSERT_TRUE(config.transferPolicies.at("throttle_policy").intervalMs.has_value());
    EXPECT_EQ(*config.transferPolicies.at("throttle_policy").intervalMs, 100);
    EXPECT_EQ(config.connections.size(), 1U);
}

TEST(BridgeLoaderTest, LoadsTickerConfig) {
    BridgeConfig config = BridgeLoader::load_config(config_path("bridge-ticker.json"));

    EXPECT_EQ(config.transferPolicies.at("ticker_policy").type, "ticker");
    ASSERT_TRUE(config.transferPolicies.at("ticker_policy").intervalMs.has_value());
    EXPECT_EQ(*config.transferPolicies.at("ticker_policy").intervalMs, 50);
    EXPECT_EQ(config.connections.front().transferPdus.front().policyId, "ticker_policy");
}

TEST(BridgeLoaderTest, LoadsTickerConfig2) {
    BridgeConfig config = BridgeLoader::load_config(config_path("bridge-ticker2.json"));

    EXPECT_EQ(config.version, "2.0.0");
    EXPECT_EQ(config.time_source_type, "virtual");
    EXPECT_EQ(config.time_source_config.msec, 10);
    
    EXPECT_EQ(config.transferPolicies.size(), 1U);
    EXPECT_TRUE(config.transferPolicies.count("ticker1"));
    const auto& policy = config.transferPolicies.at("ticker1");
    EXPECT_EQ(policy.type, "ticker");
    EXPECT_EQ(policy.interval_ticks, 5);
    EXPECT_EQ(policy.priority, 1);

    EXPECT_EQ(config.nodes.size(), 1U);
    EXPECT_EQ(config.nodes[0].id, "node1");

    EXPECT_EQ(config.endpoints.size(), 1U);
    EXPECT_EQ(config.endpoints[0].nodeId, "node1");
    EXPECT_EQ(config.endpoints[0].endpoints.size(), 2U);
    EXPECT_EQ(config.endpoints[0].endpoints[0].id, "ep1");
    EXPECT_EQ(config.endpoints[0].endpoints[1].id, "ep2");

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
    BridgeConfig config = BridgeLoader::load_config(config_path("bridge-throttle2.json"));

    EXPECT_EQ(config.version, "2.0.0");
    EXPECT_EQ(config.time_source_type, "real");
    
    EXPECT_EQ(config.transferPolicies.size(), 1U);
    EXPECT_TRUE(config.transferPolicies.count("throttle1"));
    const auto& policy = config.transferPolicies.at("throttle1");
    EXPECT_EQ(policy.type, "throttle");
    EXPECT_EQ(policy.interval_msec, 10);
    EXPECT_EQ(policy.priority, 2);

    EXPECT_EQ(config.nodes.size(), 1U);
    EXPECT_EQ(config.nodes[0].id, "node1");

    EXPECT_EQ(config.endpoints.size(), 1U);
    EXPECT_EQ(config.endpoints[0].nodeId, "node1");
    EXPECT_EQ(config.endpoints[0].endpoints.size(), 2U);
    EXPECT_EQ(config.endpoints[0].endpoints[0].id, "ep1");
    EXPECT_EQ(config.endpoints[0].endpoints[1].id, "ep2");

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
    BridgeConfig config = BridgeLoader::load_config(config_path("bridge-multiple.json"));

    EXPECT_EQ(config.version, "2.0.0");
    EXPECT_EQ(config.time_source_type, "real");
    
    EXPECT_EQ(config.transferPolicies.size(), 2U);
    EXPECT_TRUE(config.transferPolicies.count("immediate"));
    EXPECT_TRUE(config.transferPolicies.count("throttle1"));

    EXPECT_EQ(config.nodes.size(), 2U);
    EXPECT_EQ(config.nodes[0].id, "node1");
    EXPECT_EQ(config.nodes[1].id, "node2");

    EXPECT_EQ(config.endpoints.size(), 2U);
    EXPECT_EQ(config.endpoints[0].nodeId, "node1");
    EXPECT_EQ(config.endpoints[0].endpoints.size(), 2U);
    EXPECT_EQ(config.endpoints[0].endpoints[0].id, "ep1");
    EXPECT_EQ(config.endpoints[0].endpoints[1].id, "ep2");
    EXPECT_EQ(config.endpoints[1].nodeId, "node2");
    EXPECT_EQ(config.endpoints[1].endpoints.size(), 2U);
    EXPECT_EQ(config.endpoints[1].endpoints[0].id, "ep3");
    EXPECT_EQ(config.endpoints[1].endpoints[1].id, "ep4");

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
    ASSERT_THROW(BridgeLoader::load_config(config_path("bridge-invalid.json")), std::runtime_error);
}

} // namespace hako::pdu::bridge::test
