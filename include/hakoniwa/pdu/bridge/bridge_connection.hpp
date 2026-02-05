#pragma once

#include "hakoniwa/pdu/bridge/transfer_pdu.hpp"
#include "hakoniwa/time_source/time_source.hpp" // Include for ITimeSource
#include <vector>
#include <memory>
#include <chrono>
#include <string>

namespace hakoniwa::pdu::bridge {

class BridgeConnection {
public:
    BridgeConnection(const std::string& node_id, const std::string& connection_id)
        : node_id_(node_id), connection_id_(connection_id) {}

    const std::string& getNodeId() const { return node_id_; }
    const std::string& getConnectionId() const { return connection_id_; }

    void add_transfer_pdu(std::unique_ptr<ITransferPdu> pdu);

    void cyclic_trigger();

private:
    std::string node_id_;
    std::string connection_id_;
    std::vector<std::unique_ptr<ITransferPdu>> transfer_pdus_;
};

} // namespace hakoniwa::pdu::bridge

