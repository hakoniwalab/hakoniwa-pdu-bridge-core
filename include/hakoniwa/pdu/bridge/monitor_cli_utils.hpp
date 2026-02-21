#pragma once

#include "hakoniwa/pdu/endpoint_types.hpp"
#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace hakoniwa::pdu::bridge::monitor_cli {

struct HealthView {
    bool running{false};
    int64_t uptime_usec{0};
    std::string last_error;
};

struct ConnectionView {
    std::string connection_id;
    std::string node_id;
    bool active{false};
    int epoch{0};
    bool epoch_validation{false};
};

struct SessionView {
    std::string session_id;
    std::string connection_id;
    std::string policy_type;
    std::string state;
};

struct PduView {
    std::string connection_id;
    std::string robot;
    std::string pdu_name;
    int channel_id{-1};
};

bool is_error_response(const nlohmann::json& res);
std::optional<std::string> make_control_error_message(const nlohmann::json& res);

std::optional<HealthView> parse_health(const nlohmann::json& res);
std::optional<std::vector<ConnectionView>> parse_connections(const nlohmann::json& res);
std::optional<std::vector<SessionView>> parse_sessions(const nlohmann::json& res);
std::optional<std::vector<PduView>> parse_pdus(const nlohmann::json& res);

std::string resolve_pdu_name(
    const std::string& direct_name,
    const std::string& robot,
    int channel_id,
    const std::unordered_map<std::string, std::string>& fallback);

std::string try_get_epoch(std::span<const std::byte> data);

std::string format_tail_line(
    int64_t timestamp_usec,
    const hakoniwa::pdu::PduResolvedKey& key,
    size_t payload_size,
    const std::string& pdu_name,
    const std::string& epoch);

} // namespace hakoniwa::pdu::bridge::monitor_cli
