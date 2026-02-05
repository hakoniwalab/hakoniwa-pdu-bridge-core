#include "hakoniwa/pdu/bridge/bridge_connection.hpp"
#include "hakoniwa/pdu/bridge/transfer_pdu.hpp"
#include <gtest/gtest.h>

#include <memory>

namespace hakoniwa::pdu::bridge::test {

namespace {

class DummyTransferPdu final : public ITransferPdu {
public:
    void cyclic_trigger() override { ++cyclic_count_; }
    void set_active(bool is_active) override { is_active_ = is_active; }
    void set_epoch(uint64_t epoch) override { last_epoch_ = epoch; }

    int cyclic_count() const { return cyclic_count_; }
    bool is_active() const { return is_active_; }
    uint64_t last_epoch() const { return last_epoch_; }

private:
    int cyclic_count_ = 0;
    bool is_active_ = true;
    uint64_t last_epoch_ = 0;
};

} // namespace

TEST(BridgeConnectionTest, PauseResumeDisablesTransferPdus) {
    BridgeConnection connection("node1", "conn1");

    auto pdu1 = std::make_unique<DummyTransferPdu>();
    auto pdu2 = std::make_unique<DummyTransferPdu>();
    DummyTransferPdu* pdu1_raw = pdu1.get();
    DummyTransferPdu* pdu2_raw = pdu2.get();

    connection.add_transfer_pdu(std::move(pdu1));
    connection.add_transfer_pdu(std::move(pdu2));

    connection.cyclic_trigger();
    EXPECT_EQ(pdu1_raw->cyclic_count(), 1);
    EXPECT_EQ(pdu2_raw->cyclic_count(), 1);

    connection.set_active(false);
    EXPECT_FALSE(pdu1_raw->is_active());
    EXPECT_FALSE(pdu2_raw->is_active());

    connection.cyclic_trigger();
    EXPECT_EQ(pdu1_raw->cyclic_count(), 1);
    EXPECT_EQ(pdu2_raw->cyclic_count(), 1);

    connection.set_active(true);
    EXPECT_TRUE(pdu1_raw->is_active());
    EXPECT_TRUE(pdu2_raw->is_active());

    connection.cyclic_trigger();
    EXPECT_EQ(pdu1_raw->cyclic_count(), 2);
    EXPECT_EQ(pdu2_raw->cyclic_count(), 2);
}

} // namespace hakoniwa::pdu::bridge::test
