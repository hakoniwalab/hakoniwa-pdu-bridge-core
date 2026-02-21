#include "hakoniwa/pdu/bridge/monitor_cli_utils.hpp"
#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/endpoint_types.hpp"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <streambuf>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;

constexpr const char* kDefaultControlRobot = "BridgeControl";
constexpr int kDefaultControlRequestChannel = 1;
constexpr int kDefaultControlResponseChannel = 2;

std::atomic<bool> g_stop_requested{false};

class LinePrefixFilterBuf : public std::streambuf {
public:
    LinePrefixFilterBuf(std::streambuf* dest, std::string drop_prefix)
        : dest_(dest), drop_prefix_(std::move(drop_prefix))
    {
    }

protected:
    int overflow(int ch) override
    {
        if (ch == traits_type::eof()) {
            return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
        }
        buffer_.push_back(static_cast<char>(ch));
        if (ch == '\n') {
            flush_line_();
        }
        return ch;
    }

    int sync() override
    {
        if (!buffer_.empty()) {
            flush_line_();
        }
        return dest_->pubsync();
    }

private:
    void flush_line_()
    {
        if (buffer_.rfind(drop_prefix_, 0) != 0) {
            (void)dest_->sputn(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        }
        buffer_.clear();
    }

    std::streambuf* dest_;
    std::string drop_prefix_;
    std::string buffer_;
};

void signal_handler(int)
{
    g_stop_requested.store(true, std::memory_order_relaxed);
}

void print_usage(const char* argv0)
{
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " <endpoint.json> health\n"
        << "  " << argv0 << " <endpoint.json> connections\n"
        << "  " << argv0 << " <endpoint.json> sessions\n"
        << "  " << argv0 << " <endpoint.json> list_pdus <connection_id>\n"
        << "  " << argv0 << " <endpoint.json> subscribe <connection_id> [immediate|throttle|ticker] [interval_ms]\n"
        << "  " << argv0 << " <endpoint.json> unsubscribe <session_id>\n"
        << "  " << argv0 << " <endpoint.json> tail <connection_id> [immediate|throttle|ticker] [interval_ms] [duration_sec]\n";
}

std::vector<std::byte> to_bytes(const std::string& s)
{
    std::vector<std::byte> out(s.size());
    if (!s.empty()) {
        std::memcpy(out.data(), s.data(), s.size());
    }
    return out;
}

std::string now_epoch_usec_string()
{
    const auto now = std::chrono::system_clock::now();
    const auto usec = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    return std::to_string(usec);
}

class MonitorClient {
public:
    explicit MonitorClient(const std::string& endpoint_config)
        : endpoint_(std::make_shared<hakoniwa::pdu::Endpoint>("bridge_monitor_cli", HAKO_PDU_ENDPOINT_DIRECTION_INOUT)),
          endpoint_config_(endpoint_config)
    {
    }

    ~MonitorClient()
    {
        shutdown();
    }

    bool initialize()
    {
        auto err = endpoint_->open(endpoint_config_);
        if (err != HAKO_PDU_ERR_OK) {
            std::cerr << "Failed to open endpoint: " << static_cast<int>(err) << std::endl;
            return false;
        }

        endpoint_->subscribe_on_recv_callback(control_response_key_, [this](const hakoniwa::pdu::PduResolvedKey&, std::span<const std::byte> data) {
            const std::string text(reinterpret_cast<const char*>(data.data()), data.size());
            json res = json::parse(text, nullptr, false);
            if (res.is_discarded()) {
                std::cerr << "Invalid JSON response: " << text << std::endl;
                return;
            }
            std::lock_guard<std::mutex> lock(response_mtx_);
            latest_response_ = std::move(res);
            has_response_ = true;
        });

        err = endpoint_->start();
        if (err != HAKO_PDU_ERR_OK) {
            std::cerr << "Failed to start endpoint: " << static_cast<int>(err) << std::endl;
            return false;
        }
        initialized_ = true;
        return true;
    }

    void shutdown()
    {
        if (!initialized_) {
            return;
        }
        (void)endpoint_->stop();
        (void)endpoint_->close();
        initialized_ = false;
    }

    std::optional<json> request(json req, int timeout_ms = 2000)
    {
        if (!initialized_) {
            return std::nullopt;
        }
        {
            std::lock_guard<std::mutex> lock(response_mtx_);
            has_response_ = false;
            latest_response_ = json();
        }

        req["request_id"] = now_epoch_usec_string();
        const std::string payload = req.dump();
        const auto started = std::chrono::steady_clock::now();
        HakoPduErrorType send_err = HAKO_PDU_ERR_OK;
        bool sent = false;
        while (true) {
            send_err = endpoint_->send(control_request_key_, to_bytes(payload));
            if (send_err == HAKO_PDU_ERR_OK) {
                sent = true;
                break;
            }
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started);
            if (elapsed_ms.count() >= timeout_ms) {
                break;
            }
            endpoint_->process_recv_events();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (!sent) {
            std::cerr << "Failed to send request: " << static_cast<int>(send_err) << std::endl;
            return std::nullopt;
        }

        while (true) {
            endpoint_->process_recv_events();
            {
                std::lock_guard<std::mutex> lock(response_mtx_);
                if (has_response_) {
                    return latest_response_;
                }
            }
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started);
            if (elapsed_ms.count() >= timeout_ms) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        std::cerr << "Timed out waiting for response" << std::endl;
        return std::nullopt;
    }

    void subscribe_tail_channel(const hakoniwa::pdu::PduResolvedKey& key)
    {
        endpoint_->subscribe_on_recv_callback(key, [this](const hakoniwa::pdu::PduResolvedKey& k, std::span<const std::byte> data) {
            const std::string pdu_name = hakoniwa::pdu::bridge::monitor_cli::resolve_pdu_name(
                endpoint_->get_pdu_name(k), k.robot, k.channel_id, pdu_name_map_);
            const std::string epoch = hakoniwa::pdu::bridge::monitor_cli::try_get_epoch(data);
            std::cout << hakoniwa::pdu::bridge::monitor_cli::format_tail_line(
                std::stoll(now_epoch_usec_string()), k, data.size(), pdu_name, epoch)
                      << std::endl;
        });
    }

    void put_pdu_name(const std::string& robot, int channel_id, const std::string& pdu_name)
    {
        pdu_name_map_[robot + ":" + std::to_string(channel_id)] = pdu_name;
    }

    std::optional<int> resolve_channel_id(const std::string& robot, const std::string& pdu_name) const
    {
        if (robot.empty() || pdu_name.empty()) {
            return std::nullopt;
        }
        const hakoniwa::pdu::PduKey key{robot, pdu_name};
        const auto channel_id = endpoint_->get_pdu_channel_id(key);
        if (channel_id < 0) {
            return std::nullopt;
        }
        return static_cast<int>(channel_id);
    }

    void pump_once()
    {
        endpoint_->process_recv_events();
    }

private:
    std::shared_ptr<hakoniwa::pdu::Endpoint> endpoint_;
    std::string endpoint_config_;
    bool initialized_{false};

    hakoniwa::pdu::PduResolvedKey control_request_key_{kDefaultControlRobot, kDefaultControlRequestChannel};
    hakoniwa::pdu::PduResolvedKey control_response_key_{kDefaultControlRobot, kDefaultControlResponseChannel};

    std::mutex response_mtx_;
    bool has_response_{false};
    json latest_response_;

    std::unordered_map<std::string, std::string> pdu_name_map_;
};

int print_error_if_any(const json& res)
{
    const auto message = hakoniwa::pdu::bridge::monitor_cli::make_control_error_message(res);
    if (!message.has_value()) {
        return 0;
    }
    std::cerr << *message << std::endl;
    return 1;
}

std::optional<json> request_or_die(MonitorClient& client, const json& req)
{
    auto res = client.request(req);
    if (!res.has_value()) {
        return std::nullopt;
    }
    if (print_error_if_any(*res) != 0) {
        return std::nullopt;
    }
    return res;
}

void print_health(const json& res)
{
    const auto health = hakoniwa::pdu::bridge::monitor_cli::parse_health(res);
    if (!health.has_value()) {
        std::cerr << "Invalid health response" << std::endl;
        return;
    }
    std::cout
        << "[health]\n"
        << "  running: " << health->running << "\n"
        << "  uptime_usec: " << health->uptime_usec << "\n"
        << "  last_error: \"" << health->last_error << "\""
        << std::endl;
}

void print_connections(const json& res)
{
    const auto rows = hakoniwa::pdu::bridge::monitor_cli::parse_connections(res);
    if (!rows.has_value()) {
        std::cerr << "Invalid connections response" << std::endl;
        return;
    }
    std::cout << "[connections] count=" << rows->size() << std::endl;
    for (const auto& c : *rows) {
        std::cout
            << "- connection_id: " << c.connection_id
            << ", node_id: " << c.node_id
            << ", active: " << c.active
            << ", epoch: " << c.epoch
            << ", epoch_validation: " << c.epoch_validation
            << std::endl;
    }
}

void print_sessions(const json& res)
{
    const auto rows = hakoniwa::pdu::bridge::monitor_cli::parse_sessions(res);
    if (!rows.has_value()) {
        std::cerr << "Invalid sessions response" << std::endl;
        return;
    }
    std::cout << "[sessions] count=" << rows->size() << std::endl;
    for (const auto& s : *rows) {
        std::cout
            << "- session_id: " << s.session_id
            << ", connection_id: " << s.connection_id
            << ", policy: " << s.policy_type
            << ", state: " << s.state
            << std::endl;
    }
}

void print_pdus(const json& res)
{
    const auto rows = hakoniwa::pdu::bridge::monitor_cli::parse_pdus(res);
    if (!rows.has_value()) {
        std::cerr << "Invalid pdus response" << std::endl;
        return;
    }
    std::cout << "[pdus] count=" << rows->size() << std::endl;
    for (const auto& p : *rows) {
        std::cout
            << "- connection_id: " << p.connection_id
            << ", robot: " << p.robot
            << ", pdu_name: " << p.pdu_name
            << ", channel_id: " << p.channel_id
            << std::endl;
    }
}

json make_subscribe_request(const std::string& connection_id, const std::string& policy, int interval_ms)
{
    json req{{"type", "subscribe"}, {"connection_id", connection_id}};
    if (!policy.empty()) {
        json p{{"type", policy}};
        if ((policy == "throttle" || policy == "ticker") && interval_ms > 0) {
            p["interval_ms"] = interval_ms;
        }
        req["policy"] = std::move(p);
    }
    return req;
}

int run_tail(
    MonitorClient& client,
    const std::string& connection_id,
    const std::string& policy,
    int interval_ms,
    int duration_sec)
{
    auto pdus_res = request_or_die(client, json{{"type", "list_pdus"}, {"connection_id", connection_id}});
    if (!pdus_res.has_value()) {
        return 1;
    }

    int subscribed_count = 0;
    for (const auto& p : (*pdus_res)["pdus"]) {
        const auto robot = p.value("robot", std::string());
        int channel_id = p.value("channel_id", -1);
        const auto pdu_name = p.value("pdu_name", std::string());
        if (channel_id < 0) {
            const auto resolved = client.resolve_channel_id(robot, pdu_name);
            if (resolved.has_value()) {
                channel_id = *resolved;
            }
        }
        if (robot.empty() || channel_id < 0) {
            std::cerr
                << "Skipping monitor target: unresolved channel"
                << " robot=" << robot
                << " pdu_name=" << pdu_name
                << std::endl;
            continue;
        }
        client.put_pdu_name(robot, channel_id, pdu_name);
        client.subscribe_tail_channel(hakoniwa::pdu::PduResolvedKey{robot, channel_id});
        ++subscribed_count;
    }
    if (subscribed_count == 0) {
        std::cerr << "No monitor targets were resolved from list_pdus" << std::endl;
        return 1;
    }

    auto sub_res = request_or_die(client, make_subscribe_request(connection_id, policy, interval_ms));
    if (!sub_res.has_value()) {
        return 1;
    }
    if (!sub_res->contains("session_id") || !(*sub_res)["session_id"].is_string()) {
        std::cerr << "Invalid subscribed response" << std::endl;
        return 1;
    }
    const std::string session_id = (*sub_res)["session_id"].get<std::string>();
    std::cout << "tail subscribed session_id=" << session_id << std::endl;

    const auto started = std::chrono::steady_clock::now();
    while (!g_stop_requested.load(std::memory_order_relaxed)) {
        client.pump_once();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (duration_sec > 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - started);
            if (elapsed.count() >= duration_sec) {
                break;
            }
        }
    }

    auto unsub_res = client.request(json{{"type", "unsubscribe"}, {"session_id", session_id}});
    if (!unsub_res.has_value()) {
        std::cerr << "Failed to unsubscribe: timeout" << std::endl;
        return 1;
    }
    if (hakoniwa::pdu::bridge::monitor_cli::is_error_response(*unsub_res)) {
        (void)print_error_if_any(*unsub_res);
        return 1;
    }
    std::cout << "tail unsubscribed session_id=" << session_id << std::endl;
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    static LinePrefixFilterBuf debug_filter(std::cout.rdbuf(), "DEBUG:");
    std::cout.rdbuf(&debug_filter);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string endpoint_config_path = argv[1];
    const std::string command = argv[2];

    MonitorClient client(endpoint_config_path);
    if (!client.initialize()) {
        return 1;
    }

    if (command == "health") {
        auto res = request_or_die(client, json{{"type", "health"}});
        if (!res.has_value()) {
            return 1;
        }
        print_health(*res);
        return 0;
    }

    if (command == "connections") {
        auto res = request_or_die(client, json{{"type", "list_connections"}});
        if (!res.has_value()) {
            return 1;
        }
        print_connections(*res);
        return 0;
    }

    if (command == "sessions") {
        auto res = request_or_die(client, json{{"type", "list_sessions"}});
        if (!res.has_value()) {
            return 1;
        }
        print_sessions(*res);
        return 0;
    }

    if (command == "list_pdus") {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }
        const std::string connection_id = argv[3];
        auto res = request_or_die(client, json{{"type", "list_pdus"}, {"connection_id", connection_id}});
        if (!res.has_value()) {
            return 1;
        }
        print_pdus(*res);
        return 0;
    }

    if (command == "subscribe") {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }
        const std::string connection_id = argv[3];
        const std::string policy = (argc >= 5) ? argv[4] : "";
        const int interval_ms = (argc >= 6) ? std::max(1, std::atoi(argv[5])) : 0;
        auto res = request_or_die(client, make_subscribe_request(connection_id, policy, interval_ms));
        if (!res.has_value()) {
            return 1;
        }
        std::cout << "session_id=" << res->value("session_id", std::string()) << std::endl;
        return 0;
    }

    if (command == "unsubscribe") {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }
        const std::string session_id = argv[3];
        auto res = request_or_die(client, json{{"type", "unsubscribe"}, {"session_id", session_id}});
        if (!res.has_value()) {
            return 1;
        }
        std::cout << "ok" << std::endl;
        return 0;
    }

    if (command == "tail") {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }
        const std::string connection_id = argv[3];
        const std::string policy = (argc >= 5) ? argv[4] : "";
        const int interval_ms = (argc >= 6) ? std::max(1, std::atoi(argv[5])) : 0;
        const int duration_sec = (argc >= 7) ? std::max(0, std::atoi(argv[6])) : 0;
        return run_tail(client, connection_id, policy, interval_ms, duration_sec);
    }

    print_usage(argv[0]);
    return 1;
}
