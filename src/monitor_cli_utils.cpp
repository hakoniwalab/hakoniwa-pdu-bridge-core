#include "hakoniwa/pdu/bridge/monitor_cli_utils.hpp"
#include "hakoniwa/pdu/pdu_primitive_ctypes.h"

namespace hakoniwa::pdu::bridge::monitor_cli {

bool is_error_response(const nlohmann::json& res)
{
    return res.contains("type") && res["type"].is_string() && res["type"].get<std::string>() == "error";
}

std::optional<std::string> make_control_error_message(const nlohmann::json& res)
{
    if (!is_error_response(res)) {
        return std::nullopt;
    }
    std::string message = "Control error";
    if (res.contains("code") && res["code"].is_string()) {
        message += " code=" + res["code"].get<std::string>();
    }
    if (res.contains("message") && res["message"].is_string()) {
        message += " message=" + res["message"].get<std::string>();
    }
    if (res.contains("hako_error") && res["hako_error"].is_number_integer()) {
        message += " hako_error=" + std::to_string(res["hako_error"].get<int>());
    }
    return message;
}

std::optional<HealthView> parse_health(const nlohmann::json& res)
{
    if (!res.contains("health") || !res["health"].is_object()) {
        return std::nullopt;
    }
    const auto& h = res["health"];
    HealthView out;
    out.running = h.value("running", false);
    out.uptime_usec = h.value("uptime_usec", 0);
    out.last_error = h.value("last_error", std::string());
    return out;
}

std::optional<std::vector<ConnectionView>> parse_connections(const nlohmann::json& res)
{
    if (!res.contains("connections") || !res["connections"].is_array()) {
        return std::nullopt;
    }
    std::vector<ConnectionView> out;
    out.reserve(res["connections"].size());
    for (const auto& c : res["connections"]) {
        ConnectionView item;
        item.connection_id = c.value("connection_id", std::string());
        item.node_id = c.value("node_id", std::string());
        item.active = c.value("active", false);
        item.epoch = c.value("epoch", -1);
        item.epoch_validation = c.value("epoch_validation", false);
        out.push_back(std::move(item));
    }
    return out;
}

std::optional<std::vector<SessionView>> parse_sessions(const nlohmann::json& res)
{
    if (!res.contains("sessions") || !res["sessions"].is_array()) {
        return std::nullopt;
    }
    std::vector<SessionView> out;
    out.reserve(res["sessions"].size());
    for (const auto& s : res["sessions"]) {
        SessionView item;
        item.session_id = s.value("session_id", std::string());
        item.connection_id = s.value("connection_id", std::string());
        if (s.contains("policy") && s["policy"].is_object()) {
            item.policy_type = s["policy"].value("type", std::string("N/A"));
        } else {
            item.policy_type = "N/A";
        }
        item.state = s.value("state", std::string());
        out.push_back(std::move(item));
    }
    return out;
}

std::optional<std::vector<PduView>> parse_pdus(const nlohmann::json& res)
{
    if (!res.contains("connection_id") || !res.contains("pdus") || !res["pdus"].is_array()) {
        return std::nullopt;
    }
    const auto connection_id = res.value("connection_id", std::string());
    std::vector<PduView> out;
    out.reserve(res["pdus"].size());
    for (const auto& p : res["pdus"]) {
        PduView item;
        item.connection_id = connection_id;
        item.robot = p.value("robot", std::string());
        item.pdu_name = p.value("pdu_name", std::string());
        item.channel_id = p.value("channel_id", -1);
        out.push_back(std::move(item));
    }
    return out;
}

std::string resolve_pdu_name(
    const std::string& direct_name,
    const std::string& robot,
    int channel_id,
    const std::unordered_map<std::string, std::string>& fallback)
{
    if (!direct_name.empty()) {
        return direct_name;
    }
    const std::string id = robot + ":" + std::to_string(channel_id);
    const auto it = fallback.find(id);
    if (it != fallback.end()) {
        return it->second;
    }
    return "N/A";
}

std::string try_get_epoch(std::span<const std::byte> data)
{
    if (data.empty()) {
        return "N/A";
    }
    uint8_t epoch = 0;
    if (hako_pdu_get_epoch(static_cast<const void*>(data.data()), &epoch) != 0) {
        return "N/A";
    }
    return std::to_string(static_cast<int>(epoch));
}

std::string format_tail_line(
    int64_t timestamp_usec,
    const hakoniwa::pdu::PduResolvedKey& key,
    size_t payload_size,
    const std::string& pdu_name,
    const std::string& epoch)
{
    return
        "[monitor-data]\n"
        "  timestamp_usec: " + std::to_string(timestamp_usec) + "\n" +
        "  robot: " + key.robot + "\n" +
        "  channel_id: " + std::to_string(key.channel_id) + "\n" +
        "  pdu_name: " + pdu_name + "\n" +
        "  payload_size: " + std::to_string(payload_size) + "\n" +
        "  epoch: " + epoch;
}

} // namespace hakoniwa::pdu::bridge::monitor_cli
