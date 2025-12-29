#pragma once

#include "hakoniwa/pdu/bridge/bridge_core.hpp"
#include "hakoniwa/pdu/endpoint.hpp"
#include <memory>
#include <map>
#include <string>

namespace hakoniwa::pdu::bridge {

struct BridgeBuildResult {
    std::unique_ptr<BridgeCore> core;
    std::map<std::string, std::shared_ptr<hakoniwa::pdu::Endpoint>> endpoints;
};

} // namespace hakoniwa::pdu::bridge
