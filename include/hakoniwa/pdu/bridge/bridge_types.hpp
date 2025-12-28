#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <nlohmann/json.hpp> // Added for from_json functions

namespace hako::pdu::bridge {

// from transferPolicies
struct TransferPolicy {
    std::string type;
    std::optional<int> intervalMs;
};

// from nodes
struct Node {
    std::string id;
};

// from endpoints
struct EndpointDefinition {
    std::string id;
    std::string mode;
    std::string config_path; // New field
};

struct NodeEndpoints {
    std::string nodeId;
    std::vector<EndpointDefinition> endpoints;
};

// from wireLinks
struct WireLink {
    std::string from;
    std::string to;
};

// from pduKeyGroups
struct PduKey {
    std::string id;
    std::string robot_name;
    std::string pdu_name;
};

// from connections
struct ConnectionSource {
    std::string endpointId;
};

struct ConnectionDestination {
    std::string endpointId;
};

struct TransferPduConfig {
    std::string pduKeyGroupId;
    std::string policyId;
};

struct Connection {
    std::string id;
    std::string nodeId;
    ConnectionSource source;
    std::vector<ConnectionDestination> destinations;
    std::vector<TransferPduConfig> transferPdus;
};

// Root Configuration Object
struct BridgeConfig {
    std::string version;
    std::string time_source_type; // New field
    std::map<std::string, TransferPolicy> transferPolicies;
    std::vector<Node> nodes;
    std::vector<NodeEndpoints> endpoints;
    std::vector<WireLink> wireLinks;
    std::map<std::string, std::vector<PduKey>> pduKeyGroups;
    std::vector<Connection> connections;
};

// JSON parsing helpers for BridgeConfig DTOs (moved from bridge_loader.cpp)
inline void from_json(const nlohmann::json& j, TransferPolicy& p) {
    j.at("type").get_to(p.type);
    if (j.contains("intervalMs")) {
        p.intervalMs = j.at("intervalMs").get<int>();
    }
}
inline void from_json(const nlohmann::json& j, Node& n) {
    j.at("id").get_to(n.id);
}
inline void from_json(const nlohmann::json& j, EndpointDefinition& e) {
    j.at("id").get_to(e.id);
    j.at("mode").get_to(e.mode);
    j.at("config_path").get_to(e.config_path);
}
inline void from_json(const nlohmann::json& j, NodeEndpoints& n) {
    j.at("nodeId").get_to(n.nodeId);
    j.at("endpoints").get_to(n.endpoints);
}
inline void from_json(const nlohmann::json& j, WireLink& w) {
    j.at("from").get_to(w.from);
    j.at("to").get_to(w.to);
}
inline void from_json(const nlohmann::json& j, PduKey& p) {
    j.at("id").get_to(p.id);
    j.at("robot_name").get_to(p.robot_name);
    j.at("pdu_name").get_to(p.pdu_name);
}
inline void from_json(const nlohmann::json& j, ConnectionSource& s) {
    j.at("endpointId").get_to(s.endpointId);
}
inline void from_json(const nlohmann::json& j, ConnectionDestination& d) {
    j.at("endpointId").get_to(d.endpointId);
}
inline void from_json(const nlohmann::json& j, TransferPduConfig& t) {
    j.at("pduKeyGroupId").get_to(t.pduKeyGroupId);
    j.at("policyId").get_to(t.policyId);
}
inline void from_json(const nlohmann::json& j, Connection& c) {
    j.at("id").get_to(c.id);
    j.at("nodeId").get_to(c.nodeId);
    j.at("source").get_to(c.source);
    j.at("destinations").get_to(c.destinations);
    j.at("transferPdus").get_to(c.transferPdus);
}
inline void from_json(const nlohmann::json& j, BridgeConfig& b) {
    j.at("version").get_to(b.version);
    j.at("time_source_type").get_to(b.time_source_type); // Parse new field
    j.at("transferPolicies").get_to(b.transferPolicies);
    j.at("nodes").get_to(b.nodes);
    j.at("endpoints").get_to(b.endpoints);
    if (j.contains("wireLinks")) {
        j.at("wireLinks").get_to(b.wireLinks);
    }
    j.at("pduKeyGroups").get_to(b.pduKeyGroups);
    j.at("connections").get_to(b.connections);
}


} // namespace hako::pdu::bridge

