#pragma once

#include "hakoniwa/pdu/bridge/bridge_types.hpp"
#include "hakoniwa/pdu/endpoint.hpp"
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace hakoniwa::pdu::bridge {

class BridgeMonitorRuntime;

class OnDemandControlHandler {
public:
    explicit OnDemandControlHandler(std::shared_ptr<BridgeMonitorRuntime> runtime) : runtime_(std::move(runtime)) {}

    using Authorizer = std::function<bool(
        const nlohmann::json& req,
        const std::shared_ptr<hakoniwa::pdu::Endpoint>& session_endpoint)>;

    void set_authorizer(Authorizer authorizer) { authorizer_ = std::move(authorizer); }

    nlohmann::json handle_request(
        const nlohmann::json& req,
        const std::shared_ptr<hakoniwa::pdu::Endpoint>& session_endpoint = nullptr) const;

private:
    nlohmann::json make_error_(const nlohmann::json& req, const char* code, const char* message, int hako_error = -1) const;
    static const char* to_state_string_(MonitorSessionState state);

    std::shared_ptr<BridgeMonitorRuntime> runtime_;
    Authorizer authorizer_;
};

} // namespace hakoniwa::pdu::bridge
