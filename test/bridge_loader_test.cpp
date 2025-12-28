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

} // namespace hako::pdu::bridge::test
