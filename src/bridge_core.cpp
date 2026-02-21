#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/bridge/bridge_monitor_runtime.hpp"
#include "hakoniwa/pdu/bridge/policy/immediate_policy.hpp"
#include "hakoniwa/pdu/bridge/policy/throttle_policy.hpp"
#include "hakoniwa/pdu/bridge/policy/ticker_policy.hpp"
#include "hakoniwa/pdu/bridge/transfer_pdu.hpp"
#include "hakoniwa/time_source/time_source.hpp" // For ITimeSource
#include <thread>
#include <chrono>
#include <algorithm>

namespace hakoniwa::pdu::bridge {

BridgeCore::BridgeCore(const std::string& node_name, std::shared_ptr<hakoniwa::time_source::ITimeSource> time_source, std::shared_ptr<hakoniwa::pdu::EndpointContainer> endpoint_container) 
    : node_name_(node_name), is_running_(false), time_source_(time_source), endpoint_container_(endpoint_container) {
    endpoint_ids_ = endpoint_container_->list_endpoint_ids();
}

void BridgeCore::add_connection(std::unique_ptr<BridgeConnection> connection) {
    if (connection) {
        connection_transferable_pdus_.try_emplace(connection->getConnectionId());
    }
    connections_.push_back(std::move(connection));
}

void BridgeCore::register_connection_transfer_pdu_key(
    const std::string& connection_id,
    const std::string& robot,
    const std::string& pdu_name)
{
    if (connection_id.empty() || robot.empty() || pdu_name.empty()) {
        return;
    }
    auto& keys = connection_transferable_pdus_[connection_id];
    const auto key = std::make_pair(robot, pdu_name);
    const auto exists = std::find(keys.begin(), keys.end(), key) != keys.end();
    if (!exists) {
        keys.push_back(key);
    }
}

void BridgeCore::start() {
    if (is_running_.exchange(true)) {
        // Already running in another thread.
        return;
    }
    std::lock_guard<std::mutex> lock(state_mtx_);
    started_time_usec_ = time_source_ ? time_source_->get_microseconds() : 0;
    last_error_.clear();
}

bool BridgeCore::cyclic_trigger() {
    if (!is_running_) {
        #ifdef ENABLE_DEBUG_MESSAGES
        std::cout << "DEBUG: BridgeCore not running, skipping cyclic_trigger." << std::endl;
        #endif
        // Not running, so do nothing.
        return false;
    }
    // Trigger recv events for hakoniwa polling shm endpoints
    for (const auto& endpoint_id : endpoint_ids_) {
        auto endpoint = endpoint_container_->ref(endpoint_id);
        if (endpoint) {
            endpoint->process_recv_events();
        }
    }
    // Trigger cyclic transfers
    #ifdef ENABLE_DEBUG_MESSAGES
    std::cout << "connections_ size: " << connections_.size() << std::endl;
    #endif
    for (auto& connection : connections_) {
        connection->cyclic_trigger();
    }
    std::shared_ptr<BridgeMonitorRuntime> runtime;
    {
        std::lock_guard<std::mutex> lock(monitor_runtime_mtx_);
        runtime = monitor_runtime_;
    }
    if (runtime) {
        runtime->process_control_plane_once();
    }
    #ifdef ENABLE_DEBUG_MESSAGES
    std::cout << "DEBUG: BridgeCore cyclic_trigger completed." << std::endl;
    #endif
    return true;
}

void BridgeCore::stop() {
    is_running_ = false;
    std::shared_ptr<BridgeMonitorRuntime> runtime;
    {
        std::lock_guard<std::mutex> lock(monitor_runtime_mtx_);
        runtime = monitor_runtime_;
    }
    if (runtime) {
        runtime->shutdown();
    }
}

void BridgeCore::attach_monitor_runtime(std::shared_ptr<BridgeMonitorRuntime> monitor_runtime)
{
    std::lock_guard<std::mutex> lock(monitor_runtime_mtx_);
    monitor_runtime_ = std::move(monitor_runtime);
}

void BridgeCore::detach_monitor_runtime()
{
    std::shared_ptr<BridgeMonitorRuntime> runtime;
    {
        std::lock_guard<std::mutex> lock(monitor_runtime_mtx_);
        runtime = std::move(monitor_runtime_);
    }
    if (runtime) {
        runtime->shutdown();
    }
}

bool BridgeCore::set_connection_active(const std::string& connection_id, bool is_active) {
    for (auto& connection : connections_) {
        if (connection->getConnectionId() == connection_id) {
            connection->set_active(is_active);
            return true;
        }
    }
    return false;
}

bool BridgeCore::get_connection_epoch(const std::string& connection_id, uint8_t& out_epoch) const {
    for (const auto& connection : connections_) {
        if (connection->getConnectionId() == connection_id) {
            out_epoch = connection->get_epoch();
            return true;
        }
    }
    return false;
}

bool BridgeCore::increment_connection_epoch(const std::string& connection_id) {
    for (auto& connection : connections_) {
        if (connection->getConnectionId() == connection_id) {
            connection->increment_epoch();
            return true;
        }
    }
    return false;
}

std::optional<ResolvedMonitorSelection> BridgeCore::resolve_monitor_selection(
    const std::string& connection_id,
    const std::vector<MonitorFilter>& filters,
    std::string& error) const
{
    auto* connection = const_cast<BridgeCore*>(this)->find_connection_mutable_(connection_id);
    return resolve_monitor_keys_(connection, filters, error);
}

ITransferPdu* BridgeCore::create_monitor_transfer(
    const std::string& connection_id,
    const hakoniwa::pdu::bridge::PduKey& monitor_key,
    const MonitorPolicy& policy,
    const std::shared_ptr<hakoniwa::pdu::Endpoint>& destination_endpoint,
    std::string& error)
{
    if (!is_running_.load()) {
        error = "BUSY: bridge is not running";
        return nullptr;
    }
    if (connection_id.empty()) {
        error = "INVALID_REQUEST: connection_id is required";
        return nullptr;
    }
    if (!has_connection_(connection_id)) {
        error = "NOT_FOUND: connection not found";
        return nullptr;
    }
    if (!is_supported_monitor_policy_(policy)) {
        error = "UNSUPPORTED: monitor policy is not supported";
        return nullptr;
    }
    auto* connection = find_connection_mutable_(connection_id);
    if (!connection) {
        error = "NOT_FOUND: connection not found";
        return nullptr;
    }
    auto src_endpoint = connection->get_source_endpoint();
    if (!src_endpoint || !destination_endpoint) {
        error = "INTERNAL_ERROR: source/destination endpoint missing";
        return nullptr;
    }
    auto policy_instance = create_monitor_policy_(policy);
    if (!policy_instance) {
        error = "UNSUPPORTED: monitor policy is not supported";
        return nullptr;
    }
    auto transfer = std::make_unique<TransferPdu>(
        monitor_key,
        policy_instance,
        time_source_,
        src_endpoint,
        destination_endpoint);
    return connection->add_monitor_transfer_pdu(std::move(transfer));
}

void BridgeCore::deactivate_monitor_transfer(ITransferPdu* transfer)
{
    if (transfer) {
        transfer->set_active(false);
    }
}

bool BridgeCore::remove_monitor_transfer(ITransferPdu* transfer)
{
    if (!transfer) {
        return false;
    }
    for (auto& connection : connections_) {
        if (connection && connection->remove_transfer_pdu(transfer)) {
            return true;
        }
    }
    return false;
}

BridgeHealthDto BridgeCore::get_health() const
{
    BridgeHealthDto health;
    health.running = is_running_.load();
    const uint64_t now = time_source_ ? time_source_->get_microseconds() : 0;
    std::lock_guard<std::mutex> lock(state_mtx_);
    health.uptime_usec = (started_time_usec_ > 0 && now >= started_time_usec_) ? (now - started_time_usec_) : 0;
    health.last_error = last_error_;
    return health;
}

std::vector<ConnectionStateDto> BridgeCore::list_connections() const
{
    std::vector<ConnectionStateDto> out;
    out.reserve(connections_.size());
    for (const auto& connection : connections_) {
        ConnectionStateDto dto;
        dto.connection_id = connection->getConnectionId();
        dto.node_id = connection->getNodeId();
        dto.active = connection->is_active();
        dto.epoch = connection->get_epoch();
        dto.epoch_validation = connection->epoch_validation_enabled();
        out.push_back(std::move(dto));
    }
    std::sort(out.begin(), out.end(), [](const ConnectionStateDto& a, const ConnectionStateDto& b) {
        return a.connection_id < b.connection_id;
    });
    return out;
}

std::optional<std::vector<PduStateDto>> BridgeCore::list_pdus(const std::string& connection_id) const
{
    const auto* connection = find_connection_(connection_id);
    if (!connection) {
        return std::nullopt;
    }
    const auto* allowed = find_transferable_pdus_(connection_id);
    if (!allowed) {
        return std::vector<PduStateDto>{};
    }
    std::vector<PduStateDto> out;
    out.reserve(allowed->size());
    auto src_endpoint = connection->get_source_endpoint();
    for (const auto& [robot, pdu_name] : *allowed) {
        PduStateDto dto;
        dto.connection_id = connection_id;
        dto.robot = robot;
        dto.pdu_name = pdu_name;
        if (src_endpoint) {
            const int ch = src_endpoint->get_pdu_channel_id({robot, pdu_name});
            if (ch >= 0) {
                dto.channel_id = ch;
            }
        }
        out.push_back(std::move(dto));
    }
    return out;
}

std::optional<ConnectionStateDto> BridgeCore::get_connection(const std::string& connection_id) const
{
    for (const auto& connection : connections_) {
        if (connection->getConnectionId() == connection_id) {
            ConnectionStateDto dto;
            dto.connection_id = connection->getConnectionId();
            dto.node_id = connection->getNodeId();
            dto.active = connection->is_active();
            dto.epoch = connection->get_epoch();
            dto.epoch_validation = connection->epoch_validation_enabled();
            return dto;
        }
    }
    return std::nullopt;
}

bool BridgeCore::has_connection_(const std::string& connection_id) const
{
    return find_connection_(connection_id) != nullptr;
}

const BridgeConnection* BridgeCore::find_connection_(const std::string& connection_id) const
{
    for (const auto& connection : connections_) {
        if (connection->getConnectionId() == connection_id) {
            return connection.get();
        }
    }
    return nullptr;
}

BridgeConnection* BridgeCore::find_connection_mutable_(const std::string& connection_id)
{
    for (auto& connection : connections_) {
        if (connection->getConnectionId() == connection_id) {
            return connection.get();
        }
    }
    return nullptr;
}

bool BridgeCore::is_supported_monitor_policy_(const MonitorPolicy& policy) const
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

std::shared_ptr<IPduTransferPolicy> BridgeCore::create_monitor_policy_(const MonitorPolicy& policy) const
{
    const std::string type = policy.type.empty() ? "throttle" : policy.type;
    if (type == "immediate") {
        return std::make_shared<ImmediatePolicy>(false);
    }
    if (type == "throttle") {
        const int interval_ms = policy.type.empty() ? 100 : policy.interval_ms;
        if (interval_ms <= 0) {
            return nullptr;
        }
        return std::make_shared<ThrottlePolicy>(static_cast<uint64_t>(interval_ms) * 1000);
    }
    if (type == "ticker") {
        if (policy.interval_ms <= 0) {
            return nullptr;
        }
        return std::make_shared<TickerPolicy>(static_cast<uint64_t>(policy.interval_ms) * 1000);
    }
    return nullptr;
}

std::optional<ResolvedMonitorSelection> BridgeCore::resolve_monitor_keys_(
    BridgeConnection* connection,
    const std::vector<MonitorFilter>& filters,
    std::string& error) const
{
    if (!connection) {
        error = "NOT_FOUND: connection not found";
        return std::nullopt;
    }
    auto src_endpoint = connection->get_source_endpoint();
    if (!src_endpoint) {
        error = "INTERNAL_ERROR: source endpoint missing";
        return std::nullopt;
    }

    if (!filters.empty()) {
        error = "UNSUPPORTED: monitor filters are currently not supported";
        return std::nullopt;
    }

    ResolvedMonitorSelection out;
    const auto* allowed = find_transferable_pdus_(connection->getConnectionId());
    if (!allowed || allowed->empty()) {
        error = "INVALID_REQUEST: connection has no transferable pdus";
        return std::nullopt;
    }
    out.keys.reserve(allowed->size());
    out.filters.reserve(allowed->size());
    for (const auto& [robot, pdu_name] : *allowed) {
        out.keys.push_back({
            .id = robot + "." + pdu_name + ".monitor",
            .robot_name = robot,
            .pdu_name = pdu_name,
        });
        MonitorFilter f;
        f.robot = robot;
        f.pdu_name = pdu_name;
        const int channel = src_endpoint->get_pdu_channel_id({robot, pdu_name});
        if (channel >= 0) {
            f.channel_id = channel;
        }
        out.filters.push_back(std::move(f));
    }
    return out;
}

const std::vector<std::pair<std::string, std::string>>* BridgeCore::find_transferable_pdus_(
    const std::string& connection_id) const
{
    const auto it = connection_transferable_pdus_.find(connection_id);
    if (it == connection_transferable_pdus_.end()) {
        return nullptr;
    }
    return &it->second;
}

} // namespace hakoniwa::pdu::bridge
