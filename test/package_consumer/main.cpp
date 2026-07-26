#include "hakoniwa/pdu/bridge/bridge_builder.hpp"

int main()
{
    auto result = hakoniwa::pdu::bridge::build(
        "missing-bridge-config.json",
        "package-consumer",
        {},
        {});
    (void)result;
    return 0;
}
