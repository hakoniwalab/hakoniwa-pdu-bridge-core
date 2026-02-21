#include "hakoniwa/pdu/bridge/bridge_monitor_runtime.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>

namespace hakoniwa::pdu::bridge {

namespace {
bool is_supported_monitor_policy(const MonitorPolicy& policy)
{
    if (policy.type.empty()) {
        return true;
    }
    if (policy.type == "immediate") {
        return true;
    }
    if ((policy.type == "throttle") || (policy.type == "ticker")) {
        return policy.interval_ms > 0;
    }
    return false;
}

void log_monitor_event(const std::string& message)
{
    std::clog << "[monitor][event] " << message << std::endl;
}

void log_monitor_error(const std::string& message)
{
    std::cerr << "[monitor][error] " << message << std::endl;
}
} // namespace

BridgeMonitorRuntime::BridgeMonitorRuntime(std::shared_ptr<IBridgeMonitorCore> core)
    : core_(std::move(core))
{
}

HakoPduErrorType BridgeMonitorRuntime::initialize(const BridgeMonitorRuntimeOptions& options)
{
    if (initialized_) {
        return HAKO_PDU_ERR_BUSY;
    }
    if (!core_) {
        return HAKO_PDU_ERR_INVALID_ARGUMENT;
    }

    options_ = options;
    control_req_key_ = {options_.control_robot, options_.control_request_channel};
    control_res_key_ = {options_.control_robot, options_.control_response_channel};

    if (!options_.enable_ondemand) {
        initialized_ = true;
        log_monitor_event("runtime initialized (on-demand disabled)");
        return HAKO_PDU_ERR_OK;
    }
    if (options_.ondemand_mux_config_path.empty()) {
        log_monitor_error("runtime init failed: ondemand mux config path is empty");
        return HAKO_PDU_ERR_INVALID_ARGUMENT;
    }

    ondemand_mux_ = std::make_unique<hakoniwa::pdu::EndpointCommMultiplexer>(
        "bridge_ondemand_mux",
        HAKO_PDU_ENDPOINT_DIRECTION_INOUT);
    auto err = ondemand_mux_->open(options_.ondemand_mux_config_path);
    if (err != HAKO_PDU_ERR_OK) {
        log_monitor_error("runtime init failed: mux open error=" + std::to_string(static_cast<int>(err)));
        ondemand_mux_.reset();
        return err;
    }
    err = ondemand_mux_->start();
    if (err != HAKO_PDU_ERR_OK) {
        log_monitor_error("runtime init failed: mux start error=" + std::to_string(static_cast<int>(err)));
        (void)ondemand_mux_->close();
        ondemand_mux_.reset();
        return err;
    }

    control_handler_ = std::make_shared<OnDemandControlHandler>(shared_from_this());
    initialized_ = true;
    log_monitor_event(
        "runtime initialized (on-demand enabled, request=" + options_.control_robot + ":" +
        std::to_string(options_.control_request_channel) + ", response=" + options_.control_robot + ":" +
        std::to_string(options_.control_response_channel) + ")");
    return HAKO_PDU_ERR_OK;
}

void BridgeMonitorRuntime::process_control_plane_once()
{
    if (!initialized_ || !ondemand_mux_) {
        return;
    }
    auto batch = ondemand_mux_->take_endpoints();
    for (auto& ep_unique : batch) {
        auto session_ep = std::shared_ptr<hakoniwa::pdu::Endpoint>(std::move(ep_unique));
        auto weak_ep = std::weak_ptr<hakoniwa::pdu::Endpoint>(session_ep);

        session_ep->subscribe_on_recv_callback(
            control_req_key_,
            [this, weak_ep](const hakoniwa::pdu::PduResolvedKey&, std::span<const std::byte> data) {
                try {
                    auto ep = weak_ep.lock();
                    if (!ep || !control_handler_) {
                        return;
                    }
                    const std::string req_text(reinterpret_cast<const char*>(data.data()), data.size());
                    nlohmann::json req = nlohmann::json::parse(req_text, nullptr, false);
                    nlohmann::json res;
                    if (req.is_discarded()) {
                        log_monitor_error("control request parse failed");
                        res = {
                            {"type", "error"},
                            {"code", "INVALID_REQUEST"},
                            {"message", "request JSON parse failed"},
                            {"hako_error", HAKO_PDU_ERR_INVALID_JSON}
                        };
                    } else {
                        res = control_handler_->handle_request(req, ep);
                        if (res.contains("type") && res["type"].is_string()) {
                            const std::string t = res["type"].get<std::string>();
                            if (t == "subscribed" && res.contains("session_id") && res["session_id"].is_string()) {
                                log_monitor_event(
                                    "session subscribed endpoint=" + ep->get_name() +
                                    " session_id=" + res["session_id"].get<std::string>());
                                std::lock_guard<std::mutex> lock(session_mtx_);
                                endpoint_session_ids_[ep->get_name()].push_back(res["session_id"].get<std::string>());
                            } else if ((t == "ok") && req.contains("type") && req["type"].is_string() &&
                                       req["type"].get<std::string>() == "unsubscribe" &&
                                       req.contains("session_id") && req["session_id"].is_string()) {
                                log_monitor_event(
                                    "session unsubscribed endpoint=" + ep->get_name() +
                                    " session_id=" + req["session_id"].get<std::string>());
                                std::lock_guard<std::mutex> lock(session_mtx_);
                                auto it = endpoint_session_ids_.find(ep->get_name());
                                if (it != endpoint_session_ids_.end()) {
                                    auto& ids = it->second;
                                    const auto target = req["session_id"].get<std::string>();
                                    ids.erase(std::remove(ids.begin(), ids.end(), target), ids.end());
                                }
                            }
                        }
                    }

                    const auto bytes = to_bytes_(res.dump());
                    (void)ep->send(control_res_key_, bytes);
                } catch (const std::exception& e) {
                    log_monitor_error(std::string("control callback exception: ") + e.what());
                } catch (...) {
                    log_monitor_error("control callback exception: unknown");
                }
            });

        std::lock_guard<std::mutex> lock(session_mtx_);
        log_monitor_event("control session connected endpoint=" + session_ep->get_name());
        ondemand_sessions_.push_back(std::move(session_ep));
    }

    cleanup_disconnected_sessions_();
}

void BridgeMonitorRuntime::shutdown()
{
    if (!initialized_) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(session_mtx_);
        for (const auto& [_, session] : monitor_runtime_sessions_) {
            for (auto* transfer : session.transfers) {
                core_->deactivate_monitor_transfer(transfer);
                (void)core_->remove_monitor_transfer(transfer);
            }
        }
        monitor_runtime_sessions_.clear();
        monitor_sessions_.clear();
        endpoint_session_ids_.clear();
        for (auto& ep : ondemand_sessions_) {
            if (ep) {
                if (disconnected_control_endpoints_.find(ep->get_name()) != disconnected_control_endpoints_.end()) {
                    continue;
                }
                (void)ep->stop();
                (void)ep->close();
            }
        }
        disconnected_control_endpoints_.clear();
        ondemand_sessions_.clear();
    }
    if (ondemand_mux_) {
        (void)ondemand_mux_->stop();
        (void)ondemand_mux_->close();
        ondemand_mux_.reset();
    }
    control_handler_.reset();
    initialized_ = false;
    log_monitor_event("runtime shutdown completed");
}

std::optional<std::string> BridgeMonitorRuntime::attach_monitor(const MonitorSessionSpec& spec)
{
    if (!core_ || !core_->is_running()) {
        log_monitor_error("attach_monitor rejected: bridge is not running");
        return std::nullopt;
    }
    if (spec.connection_id.empty()) {
        log_monitor_error("attach_monitor rejected: connection_id is empty");
        return std::nullopt;
    }

    MonitorPolicy policy = spec.policy;
    if (policy.type.empty()) {
        policy.type = "throttle";
        policy.interval_ms = 100;
    }
    if (!is_supported_monitor_policy(policy)) {
        log_monitor_error("attach_monitor rejected: unsupported policy type=" + policy.type);
        return std::nullopt;
    }

    std::string error;
    auto selection = core_->resolve_monitor_selection(spec.connection_id, spec.filters, error);
    if (!selection.has_value()) {
        log_monitor_error("attach_monitor failed: " + error);
        return std::nullopt;
    }

    std::vector<ITransferPdu*> transfers;
    if (spec.destination_endpoint) {
        transfers.reserve(selection->keys.size());
        for (const auto& key : selection->keys) {
            auto* transfer = core_->create_monitor_transfer(
                spec.connection_id, key, policy, spec.destination_endpoint, error);
            if (!transfer) {
                log_monitor_error("attach_monitor failed: " + error);
                for (auto* created : transfers) {
                    core_->deactivate_monitor_transfer(created);
                    (void)core_->remove_monitor_transfer(created);
                }
                return std::nullopt;
            }
            transfers.push_back(transfer);
        }
    }

    MonitorSessionInfo info;
    info.connection_id = spec.connection_id;
    info.filters = selection->filters;
    info.policy = policy;
    info.state = MonitorSessionState::Active;

    std::lock_guard<std::mutex> lock(session_mtx_);
    const std::string session_id = "ms-" + std::to_string(next_monitor_session_id_++);
    info.session_id = session_id;
    monitor_sessions_.emplace(session_id, info);
    if (!transfers.empty()) {
        monitor_runtime_sessions_.emplace(session_id, MonitorSessionRuntime{transfers, spec.destination_endpoint});
    }
    log_monitor_event(
        "attach_monitor success connection_id=" + spec.connection_id +
        " session_id=" + session_id +
        " policy=" + policy.type);
    return session_id;
}

bool BridgeMonitorRuntime::detach_monitor(const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(session_mtx_);
    auto it = monitor_sessions_.find(session_id);
    if (it == monitor_sessions_.end()) {
        return true;
    }
    it->second.state = MonitorSessionState::Draining;
    auto runtime_it = monitor_runtime_sessions_.find(session_id);
    if (runtime_it != monitor_runtime_sessions_.end()) {
        for (auto* transfer : runtime_it->second.transfers) {
            core_->deactivate_monitor_transfer(transfer);
            (void)core_->remove_monitor_transfer(transfer);
        }
        monitor_runtime_sessions_.erase(runtime_it);
    }
    it->second.state = MonitorSessionState::Closed;
    monitor_sessions_.erase(it);
    log_monitor_event("detach_monitor success session_id=" + session_id);
    return true;
}

std::vector<MonitorSessionInfo> BridgeMonitorRuntime::list_monitor_infos() const
{
    std::lock_guard<std::mutex> lock(session_mtx_);
    std::vector<MonitorSessionInfo> out;
    out.reserve(monitor_sessions_.size());
    for (const auto& [_, info] : monitor_sessions_) {
        out.push_back(info);
    }
    std::sort(out.begin(), out.end(), [](const MonitorSessionInfo& a, const MonitorSessionInfo& b) {
        return a.session_id < b.session_id;
    });
    return out;
}

BridgeHealthDto BridgeMonitorRuntime::get_health() const
{
    return core_->get_health();
}

std::vector<ConnectionStateDto> BridgeMonitorRuntime::list_connections() const
{
    return core_->list_connections();
}

std::optional<std::vector<PduStateDto>> BridgeMonitorRuntime::list_pdus(const std::string& connection_id) const
{
    return core_->list_pdus(connection_id);
}

void BridgeMonitorRuntime::cleanup_disconnected_sessions_()
{
    std::vector<std::string> disconnected;
    {
        std::lock_guard<std::mutex> lock(session_mtx_);
        for (const auto& ep : ondemand_sessions_) {
            if (!ep) {
                continue;
            }
            if (disconnected_control_endpoints_.find(ep->get_name()) != disconnected_control_endpoints_.end()) {
                continue;
            }
            bool running = false;
            if (ep->is_running(running) != HAKO_PDU_ERR_OK || !running) {
                disconnected.push_back(ep->get_name());
                disconnected_control_endpoints_.insert(ep->get_name());
            }
        }
    }

    for (const auto& name : disconnected) {
        log_monitor_event("control session disconnected endpoint=" + name);
        detach_sessions_for_endpoint_(name);
    }
}

void BridgeMonitorRuntime::detach_sessions_for_endpoint_(const std::string& endpoint_name)
{
    std::vector<std::string> session_ids;
    {
        std::lock_guard<std::mutex> lock(session_mtx_);
        auto it = endpoint_session_ids_.find(endpoint_name);
        if (it == endpoint_session_ids_.end()) {
            return;
        }
        session_ids = std::move(it->second);
        endpoint_session_ids_.erase(it);
    }
    for (const auto& sid : session_ids) {
        (void)detach_monitor(sid);
    }
}

std::vector<std::byte> BridgeMonitorRuntime::to_bytes_(const std::string& text)
{
    std::vector<std::byte> bytes(text.size());
    std::memcpy(bytes.data(), text.data(), text.size());
    return bytes;
}

} // namespace hakoniwa::pdu::bridge
