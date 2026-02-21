#include "hakoniwa/pdu/bridge/bridge_builder.hpp"
#include "hakoniwa/pdu/bridge/bridge_monitor_runtime.hpp"
#include "hakoniwa/pdu/bridge/ondemand_control_handler.hpp"
#include "hakoniwa/time_source/time_source_factory.hpp"
#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>

namespace hakoniwa::pdu::bridge::test {

namespace {
std::filesystem::path test_config_root() {
    if (const char* config_dir = std::getenv("HAKO_TEST_CONFIG_DIR"); config_dir && *config_dir) {
        return std::filesystem::path(config_dir);
    }
    return std::filesystem::path(TEST_CONFIG_DIR);
}

std::string config_path(const std::string& filename, const std::string& subdir = "core_flow") {
    return (test_config_root() / subdir / filename).string();
}
}

TEST(OnDemandControlHandlerTest, HealthAndSessionLifecycle) {
    auto endpoint_container = std::make_shared<hakoniwa::pdu::EndpointContainer>("node1", config_path("endpoints.json"));
    ASSERT_EQ(endpoint_container->initialize(), HAKO_PDU_ERR_OK);
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source =
        hakoniwa::time_source::create_time_source("real", 1000);

    auto result = hakoniwa::pdu::bridge::build(config_path("bridge-core-flow-test.json"), "node1", time_source, endpoint_container);
    ASSERT_TRUE(result.ok()) << result.error_message;
    std::shared_ptr<BridgeCore> core(std::move(result.core));
    ASSERT_TRUE(core != nullptr);

    ASSERT_EQ(endpoint_container->start_all(), HAKO_PDU_ERR_OK);
    core->start();

    auto runtime = std::make_shared<BridgeMonitorRuntime>(core);
    OnDemandControlHandler handler(runtime);

    auto health_res = handler.handle_request({{"type", "health"}, {"request_id", "r1"}});
    ASSERT_EQ(health_res.at("type"), "health");
    ASSERT_TRUE(health_res.contains("health"));
    ASSERT_TRUE(health_res.at("health").at("running").get<bool>());
    ASSERT_EQ(health_res.at("request_id"), "r1");

    auto sub_res = handler.handle_request({
        {"type", "subscribe"},
        {"request_id", "r2"},
        {"connection_id", "conn1"},
        {"policy", {{"type", "throttle"}, {"interval_ms", 100}}}
    });
    ASSERT_EQ(sub_res.at("type"), "subscribed");
    ASSERT_TRUE(sub_res.contains("session_id"));
    const std::string sid = sub_res.at("session_id").get<std::string>();
    ASSERT_FALSE(sid.empty());

    auto list_res = handler.handle_request({{"type", "list_sessions"}});
    ASSERT_EQ(list_res.at("type"), "sessions");
    ASSERT_TRUE(list_res.contains("sessions"));
    ASSERT_EQ(list_res.at("sessions").size(), 1);
    ASSERT_EQ(list_res.at("sessions").at(0).at("session_id").get<std::string>(), sid);

    auto conns_res = handler.handle_request({{"type", "list_connections"}});
    ASSERT_EQ(conns_res.at("type"), "connections");
    ASSERT_TRUE(conns_res.contains("connections"));
    ASSERT_EQ(conns_res.at("connections").size(), 1);
    ASSERT_EQ(conns_res.at("connections").at(0).at("connection_id").get<std::string>(), "conn1");

    auto pdus_res = handler.handle_request({{"type", "list_pdus"}, {"connection_id", "conn1"}});
    ASSERT_EQ(pdus_res.at("type"), "pdus");
    ASSERT_EQ(pdus_res.at("connection_id").get<std::string>(), "conn1");
    ASSERT_TRUE(pdus_res.contains("pdus"));
    ASSERT_GE(pdus_res.at("pdus").size(), 1);

    auto unsub_res = handler.handle_request({{"type", "unsubscribe"}, {"session_id", sid}});
    ASSERT_EQ(unsub_res.at("type"), "ok");

    auto list_after = handler.handle_request({{"type", "list_sessions"}});
    ASSERT_EQ(list_after.at("sessions").size(), 0);

    auto sub_all = handler.handle_request({
        {"type", "subscribe"},
        {"connection_id", "conn1"}
    });
    ASSERT_EQ(sub_all.at("type"), "subscribed");
    ASSERT_TRUE(sub_all.contains("session_id"));
    auto list_all = handler.handle_request({{"type", "list_sessions"}});
    ASSERT_EQ(list_all.at("sessions").size(), 1);
    ASSERT_TRUE(list_all.at("sessions").at(0).contains("filters"));
    ASSERT_GE(list_all.at("sessions").at(0).at("filters").size(), 1);
}

TEST(OnDemandControlHandlerTest, InvalidRequestReturnsError) {
    auto endpoint_container = std::make_shared<hakoniwa::pdu::EndpointContainer>("node1", config_path("endpoints.json"));
    ASSERT_EQ(endpoint_container->initialize(), HAKO_PDU_ERR_OK);
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source =
        hakoniwa::time_source::create_time_source("real", 1000);

    auto result = hakoniwa::pdu::bridge::build(config_path("bridge-core-flow-test.json"), "node1", time_source, endpoint_container);
    ASSERT_TRUE(result.ok()) << result.error_message;
    std::shared_ptr<BridgeCore> core(std::move(result.core));
    ASSERT_TRUE(core != nullptr);
    ASSERT_EQ(endpoint_container->start_all(), HAKO_PDU_ERR_OK);
    core->start();

    auto runtime = std::make_shared<BridgeMonitorRuntime>(core);
    OnDemandControlHandler handler(runtime);

    auto bad_type = handler.handle_request({{"type", "unknown"}});
    ASSERT_EQ(bad_type.at("type"), "error");
    ASSERT_EQ(bad_type.at("code"), "UNSUPPORTED");

    auto bad_sub = handler.handle_request({{"type", "subscribe"}, {"connection_id", "conn1"}});
    ASSERT_EQ(bad_sub.at("type"), "subscribed");
    ASSERT_TRUE(bad_sub.contains("session_id"));

    auto bad_filter = handler.handle_request({
        {"type", "subscribe"},
        {"connection_id", "conn1"},
        {"filters", nlohmann::json::array({ {{"robot", "Drone"}} })}
    });
    ASSERT_EQ(bad_filter.at("type"), "error");
    ASSERT_EQ(bad_filter.at("code"), "UNSUPPORTED");

    auto bad_list_pdus = handler.handle_request({{"type", "list_pdus"}, {"connection_id", "unknown"}});
    ASSERT_EQ(bad_list_pdus.at("type"), "error");
    ASSERT_EQ(bad_list_pdus.at("code"), "NOT_FOUND");
}

TEST(OnDemandControlHandlerTest, ReadOnlyControlRejectsWriteLikeRequests) {
    auto endpoint_container = std::make_shared<hakoniwa::pdu::EndpointContainer>("node1", config_path("endpoints.json"));
    ASSERT_EQ(endpoint_container->initialize(), HAKO_PDU_ERR_OK);
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source =
        hakoniwa::time_source::create_time_source("real", 1000);

    auto result = hakoniwa::pdu::bridge::build(config_path("bridge-core-flow-test.json"), "node1", time_source, endpoint_container);
    ASSERT_TRUE(result.ok()) << result.error_message;
    std::shared_ptr<BridgeCore> core(std::move(result.core));
    ASSERT_TRUE(core != nullptr);
    ASSERT_EQ(endpoint_container->start_all(), HAKO_PDU_ERR_OK);
    core->start();

    auto runtime = std::make_shared<BridgeMonitorRuntime>(core);
    OnDemandControlHandler handler(runtime);

    auto before = handler.handle_request({{"type", "list_connections"}});
    ASSERT_EQ(before.at("type"), "connections");
    ASSERT_EQ(before.at("connections").size(), 1);
    ASSERT_TRUE(before.at("connections").at(0).at("active").get<bool>());
    const auto before_epoch = before.at("connections").at(0).at("epoch").get<int>();

    auto write_like_1 = handler.handle_request({
        {"type", "set_connection_active"},
        {"connection_id", "conn1"},
        {"active", false}
    });
    ASSERT_EQ(write_like_1.at("type"), "error");
    ASSERT_EQ(write_like_1.at("code"), "UNSUPPORTED");

    auto write_like_2 = handler.handle_request({
        {"type", "increment_connection_epoch"},
        {"connection_id", "conn1"}
    });
    ASSERT_EQ(write_like_2.at("type"), "error");
    ASSERT_EQ(write_like_2.at("code"), "UNSUPPORTED");

    auto after = handler.handle_request({{"type", "list_connections"}});
    ASSERT_EQ(after.at("type"), "connections");
    ASSERT_EQ(after.at("connections").size(), 1);
    ASSERT_TRUE(after.at("connections").at(0).at("active").get<bool>());
    ASSERT_EQ(after.at("connections").at(0).at("epoch").get<int>(), before_epoch);
}

TEST(OnDemandControlHandlerTest, AuthorizerCanDenyRequest) {
    auto endpoint_container = std::make_shared<hakoniwa::pdu::EndpointContainer>("node1", config_path("endpoints.json"));
    ASSERT_EQ(endpoint_container->initialize(), HAKO_PDU_ERR_OK);
    std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source =
        hakoniwa::time_source::create_time_source("real", 1000);

    auto result = hakoniwa::pdu::bridge::build(config_path("bridge-core-flow-test.json"), "node1", time_source, endpoint_container);
    ASSERT_TRUE(result.ok()) << result.error_message;
    std::shared_ptr<BridgeCore> core(std::move(result.core));
    ASSERT_TRUE(core != nullptr);
    ASSERT_EQ(endpoint_container->start_all(), HAKO_PDU_ERR_OK);
    core->start();

    auto runtime = std::make_shared<BridgeMonitorRuntime>(core);
    OnDemandControlHandler handler(runtime);
    handler.set_authorizer([](const nlohmann::json&, const std::shared_ptr<hakoniwa::pdu::Endpoint>&) {
        return false;
    });

    auto denied = handler.handle_request({{"type", "health"}});
    ASSERT_EQ(denied.at("type"), "error");
    ASSERT_EQ(denied.at("code"), "PERMISSION_DENIED");
}

} // namespace hakoniwa::pdu::bridge::test
