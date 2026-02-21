#include "hakoniwa/pdu/bridge/ondemand_control_handler.hpp"
#include "hakoniwa/pdu/bridge/bridge_monitor_runtime.hpp"
#include <iostream>

namespace hakoniwa::pdu::bridge {

nlohmann::json OnDemandControlHandler::make_error_(
    const nlohmann::json& req, const char* code, const char* message, int hako_error) const
{
    nlohmann::json res{
        {"type", "error"},
        {"code", code},
        {"message", message},
    };
    if (req.contains("request_id") && req["request_id"].is_string()) {
        res["request_id"] = req["request_id"];
    }
    if (hako_error >= 0) {
        res["hako_error"] = hako_error;
    }
    std::string req_type = "unknown";
    if (req.contains("type") && req["type"].is_string()) {
        req_type = req["type"].get<std::string>();
    }
    std::cerr << "[monitor][error] control request failed"
              << " type=" << req_type
              << " code=" << code
              << " message=" << message
              << std::endl;
    return res;
}

const char* OnDemandControlHandler::to_state_string_(MonitorSessionState state)
{
    switch (state) {
    case MonitorSessionState::Created: return "Created";
    case MonitorSessionState::Active: return "Active";
    case MonitorSessionState::Draining: return "Draining";
    case MonitorSessionState::Closed: return "Closed";
    }
    return "Closed";
}

nlohmann::json OnDemandControlHandler::handle_request(
    const nlohmann::json& req,
    const std::shared_ptr<hakoniwa::pdu::Endpoint>& session_endpoint) const
{
    if (!runtime_) {
        return make_error_(req, "INTERNAL_ERROR", "bridge core is null");
    }
    if (!req.is_object()) {
        return make_error_(req, "INVALID_REQUEST", "request must be an object", HAKO_PDU_ERR_INVALID_ARGUMENT);
    }
    if (!req.contains("type") || !req["type"].is_string()) {
        return make_error_(req, "INVALID_REQUEST", "type is required", HAKO_PDU_ERR_INVALID_ARGUMENT);
    }
    if (authorizer_ && !authorizer_(req, session_endpoint)) {
        return make_error_(req, "PERMISSION_DENIED", "request is not authorized", HAKO_PDU_ERR_UNSUPPORTED);
    }

    const std::string type = req["type"].get<std::string>();

    if (type == "health") {
        const auto health = runtime_->get_health();
        nlohmann::json body{
            {"running", health.running},
            {"uptime_usec", health.uptime_usec},
            {"last_error", health.last_error}
        };
        nlohmann::json res{
            {"type", "health"},
            {"health", body}
        };
        if (req.contains("request_id") && req["request_id"].is_string()) {
            res["request_id"] = req["request_id"];
        }
        return res;
    }

    if (type == "list_sessions" || type == "list") {
        const auto infos = runtime_->list_monitor_infos();
        nlohmann::json sessions = nlohmann::json::array();
        for (const auto& info : infos) {
            nlohmann::json filters = nlohmann::json::array();
            for (const auto& f : info.filters) {
                nlohmann::json one{{"robot", f.robot}};
                if (f.channel_id.has_value()) {
                    one["channel_id"] = *f.channel_id;
                }
                if (f.pdu_name.has_value()) {
                    one["pdu_name"] = *f.pdu_name;
                }
                filters.push_back(std::move(one));
            }
            nlohmann::json policy{{"type", info.policy.type}};
            if (info.policy.interval_ms > 0) {
                policy["interval_ms"] = info.policy.interval_ms;
            }
            sessions.push_back({
                {"session_id", info.session_id},
                {"connection_id", info.connection_id},
                {"filters", filters},
                {"policy", policy},
                {"state", to_state_string_(info.state)}
            });
        }
        nlohmann::json res{
            {"type", "sessions"},
            {"sessions", sessions}
        };
        if (req.contains("request_id") && req["request_id"].is_string()) {
            res["request_id"] = req["request_id"];
        }
        return res;
    }

    if (type == "list_connections") {
        const auto connections_info = runtime_->list_connections();
        nlohmann::json connections = nlohmann::json::array();
        for (const auto& conn : connections_info) {
            connections.push_back({
                {"connection_id", conn.connection_id},
                {"node_id", conn.node_id},
                {"active", conn.active},
                {"epoch", static_cast<int>(conn.epoch)},
                {"epoch_validation", conn.epoch_validation}
            });
        }
        nlohmann::json res{
            {"type", "connections"},
            {"connections", connections}
        };
        if (req.contains("request_id") && req["request_id"].is_string()) {
            res["request_id"] = req["request_id"];
        }
        return res;
    }

    if (type == "list_pdus") {
        if (!req.contains("connection_id") || !req["connection_id"].is_string()) {
            return make_error_(req, "INVALID_REQUEST", "connection_id is required", HAKO_PDU_ERR_INVALID_ARGUMENT);
        }
        const auto connection_id = req["connection_id"].get<std::string>();
        const auto pdus_info = runtime_->list_pdus(connection_id);
        if (!pdus_info.has_value()) {
            return make_error_(req, "NOT_FOUND", "connection not found", HAKO_PDU_ERR_NO_ENTRY);
        }
        nlohmann::json pdus = nlohmann::json::array();
        for (const auto& pdu : *pdus_info) {
            nlohmann::json one{
                {"connection_id", pdu.connection_id},
                {"robot", pdu.robot},
                {"pdu_name", pdu.pdu_name}
            };
            if (pdu.channel_id.has_value()) {
                one["channel_id"] = *pdu.channel_id;
            }
            pdus.push_back(std::move(one));
        }
        nlohmann::json res{
            {"type", "pdus"},
            {"connection_id", connection_id},
            {"pdus", pdus}
        };
        if (req.contains("request_id") && req["request_id"].is_string()) {
            res["request_id"] = req["request_id"];
        }
        return res;
    }

    if (type == "unsubscribe") {
        if (!req.contains("session_id") || !req["session_id"].is_string()) {
            return make_error_(req, "INVALID_REQUEST", "session_id is required", HAKO_PDU_ERR_INVALID_ARGUMENT);
        }
        const std::string session_id = req["session_id"].get<std::string>();
        const bool ok = runtime_->detach_monitor(session_id);
        if (!ok) {
            return make_error_(req, "INTERNAL_ERROR", "failed to detach monitor");
        }
        nlohmann::json res{{"type", "ok"}};
        if (req.contains("request_id") && req["request_id"].is_string()) {
            res["request_id"] = req["request_id"];
        }
        return res;
    }

    if (type == "subscribe") {
        if (!req.contains("connection_id") || !req["connection_id"].is_string()) {
            return make_error_(req, "INVALID_REQUEST", "connection_id is required", HAKO_PDU_ERR_INVALID_ARGUMENT);
        }
        if (req.contains("filters") && !req["filters"].is_array()) {
            return make_error_(req, "INVALID_REQUEST", "filters must be an array", HAKO_PDU_ERR_INVALID_ARGUMENT);
        }
        MonitorSessionSpec spec;
        spec.connection_id = req["connection_id"].get<std::string>();
        if (req.contains("filters")) {
            if (!req["filters"].empty()) {
                return make_error_(req, "UNSUPPORTED", "filters are currently not supported", HAKO_PDU_ERR_UNSUPPORTED);
            }
        }
        if (req.contains("policy") && req["policy"].is_object()) {
            const auto& p = req["policy"];
            if (p.contains("type") && p["type"].is_string()) {
                spec.policy.type = p["type"].get<std::string>();
            }
            if (p.contains("interval_ms") && p["interval_ms"].is_number_integer()) {
                spec.policy.interval_ms = p["interval_ms"].get<int>();
            }
        }
        if (session_endpoint) {
            spec.destination_endpoint = session_endpoint;
        }

        auto session_id = runtime_->attach_monitor(spec);
        if (!session_id.has_value()) {
            return make_error_(req, "INVALID_REQUEST", "failed to attach monitor", HAKO_PDU_ERR_INVALID_CONFIG);
        }
        nlohmann::json res{
            {"type", "subscribed"},
            {"session_id", *session_id}
        };
        if (req.contains("request_id") && req["request_id"].is_string()) {
            res["request_id"] = req["request_id"];
        }
        return res;
    }

    return make_error_(req, "UNSUPPORTED", "unknown request type", HAKO_PDU_ERR_UNSUPPORTED);
}

} // namespace hakoniwa::pdu::bridge
