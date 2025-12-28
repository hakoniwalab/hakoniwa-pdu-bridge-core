#pragma once

#include "hakoniwa/pdu/bridge/transfer_pdu.hpp"
#include "hakoniwa/pdu/bridge/time_source.hpp" // Include for ITimeSource
#include <vector>
#include <memory>
#include <chrono>
#include <string>

namespace hako::pdu::bridge {

class BridgeConnection {
public:
    BridgeConnection(const std::string& node_id) : node_id_(node_id) {}

    const std::string& getNodeId() const { return node_id_; }

    void add_transfer_pdu(std::unique_ptr<TransferPdu> pdu);

    void step(const std::shared_ptr<ITimeSource>& time_source);

private:
    std::string node_id_;
    std::vector<std::unique_ptr<TransferPdu>> transfer_pdus_;
};

} // namespace hako::pdu::bridge


