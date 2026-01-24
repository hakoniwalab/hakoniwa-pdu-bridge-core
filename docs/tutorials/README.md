# Tutorials

These tutorials explain how to use each transfer policy with the existing bridge daemon (`src/daemon.cpp`).

## Common setup

All examples assume the local endpoint configs under `config/sample/endpoint/` and the endpoint container file `test/config/core_flow/endpoints.json`.

The bridge daemon runs as:

```bash
./build/hakoniwa-pdu-bridge <bridge.json> <delta_time_step_usec> <endpoint_container.json> [node_name]
```

Notes:
- `time_source_type` in `bridge.json` is ignored by the current implementation. `delta_time_step_usec` is used with a fixed `real` time source.
- `endpoints` and `wireLinks` are ignored by the current implementation, but are kept in examples to satisfy the schema.
- `config_path` values are resolved relative to the `bridge.json` location.
- The bridge daemon uses an endpoint container; the writer/reader examples open endpoint configs directly.
- The tutorial configs use TCP loopback (`127.0.0.1`) on ports `9000` and `9100`.

Ready-to-run configs:
- `config/tutorials/bridge-immediate.json`
- `config/tutorials/bridge-throttle.json`
- `config/tutorials/bridge-ticker.json`
- `config/tutorials/endpoint_container.json`

## Sample programs

You typically need:
- a **writer** that sends PDUs into the source endpoint
- a **reader** that receives PDUs from the destination endpoint

Below are minimal C++ examples using `EndpointContainer`.

### Build examples

```bash
cmake -S . -B build -DHAKO_PDU_BRIDGE_BUILD_EXAMPLES=ON
cmake --build build
```

This builds:
- `build/examples/bridge_writer`
- `build/examples/bridge_reader`

### Writer example (source)

```cpp
#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/endpoint_types.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <ctime>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

int main()
{
    const std::string endpoint_config_path = "config/tutorials/endpoint/writer.json";
    const hakoniwa::pdu::PduKey pdu_key{"Drone", "pos"};

    hakoniwa::pdu::Endpoint endpoint("bridge_writer", HAKO_PDU_ENDPOINT_DIRECTION_OUT);
    if (endpoint.open(endpoint_config_path) != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to open endpoint" << std::endl;
        return 1;
    }
    if (endpoint.start() != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to start endpoint" << std::endl;
        return 1;
    }
    const std::size_t pdu_size = endpoint.get_pdu_size(pdu_key);
    if (pdu_size == 0) {
        std::cerr << "PDU size is 0" << std::endl;
        return 1;
    }

    std::vector<std::byte> buffer(pdu_size);
    std::uint32_t seq = 0;
    for (;;) {
        std::memset(buffer.data(), 0, buffer.size());
        const auto now = std::chrono::system_clock::now();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() % 1000;
        const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#if defined(_WIN32)
        localtime_s(&tm_buf, &now_time);
#else
        localtime_r(&now_time, &tm_buf);
#endif
        char time_text[64];
        std::strftime(time_text, sizeof(time_text), "%Y-%m-%dT%H:%M:%S", &tm_buf);
        std::snprintf(
            reinterpret_cast<char*>(buffer.data()),
            buffer.size(),
            "ts=%s.%03lld seq=%u",
            time_text,
            static_cast<long long>(millis),
            seq);
        (void)endpoint.send(pdu_key, std::span<const std::byte>(buffer));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ++seq;
    }
}
```

### Reader example (destination)

```cpp
#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/endpoint_types.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

int main()
{
    const std::string endpoint_config_path = "config/tutorials/endpoint/reader.json";
    const hakoniwa::pdu::PduKey pdu_key{"Drone", "pos"};

    hakoniwa::pdu::Endpoint endpoint("bridge_reader", HAKO_PDU_ENDPOINT_DIRECTION_IN);
    if (endpoint.open(endpoint_config_path) != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to open endpoint" << std::endl;
        return 1;
    }
    const std::size_t pdu_size = endpoint.get_pdu_size(pdu_key);
    if (pdu_size == 0) {
        std::cerr << "PDU size is 0" << std::endl;
        return 1;
    }

    const auto channel_id = endpoint.get_pdu_channel_id(pdu_key);
    if (channel_id < 0) {
        std::cerr << "Failed to resolve channel" << std::endl;
        return 1;
    }
    const hakoniwa::pdu::PduResolvedKey resolved_key{"Drone", channel_id};
    endpoint.subscribe_on_recv_callback(
        resolved_key,
        [pdu_size](const hakoniwa::pdu::PduResolvedKey&, std::span<const std::byte> data) {
            const std::size_t size = std::min(pdu_size, data.size());
            const auto begin = reinterpret_cast<const char*>(data.data());
            const auto end = begin + size;
            const auto nul = std::find(begin, end, '\0');
            std::string text(begin, nul);
            std::cout << "received bytes=" << size << " text=\"" << text << "\"" << std::endl;
        });

    if (endpoint.start() != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to start endpoint" << std::endl;
        return 1;
    }

    if (endpoint.start() != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to start endpoint" << std::endl;
        return 1;
    }

    for (;;) {
        endpoint.process_recv_events();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
```

## Run flow (daemon + writer + reader)

Use the ready-to-run config `config/tutorials/bridge-immediate.json` and the endpoint container in `config/tutorials/endpoint_container.json`.

### Immediate (default example)

Terminal 1 (bridge daemon):
```bash
./build/hakoniwa-pdu-bridge \
  config/tutorials/bridge-immediate.json \
  1000 \
  config/tutorials/endpoint_container.json \
  node1
```

Terminal 2 (reader, destination):
```bash
build/examples/bridge_reader \
  config/tutorials/endpoint/reader.json \
  Drone \
  pos \
  10
```

Terminal 3 (writer, source):
```bash
build/examples/bridge_writer \
  config/tutorials/endpoint/writer.json \
  Drone \
  pos \
  3000
```

Expected output:
- `bridge_writer` prints `sent seq=...`
- `bridge_reader` prints `recv bytes=... text="ts=... seq=..."`

### Throttle (interval 100ms)

Terminal 1 (bridge daemon):
```bash
./build/hakoniwa-pdu-bridge \
  config/tutorials/bridge-throttle.json \
  1000 \
  config/tutorials/endpoint_container.json \
  node1
```

Terminal 2 (reader, destination):
```bash
build/examples/bridge_reader \
  config/tutorials/endpoint/reader.json \
  Drone \
  pos \
  10
```

Terminal 3 (writer, source):
```bash
build/examples/bridge_writer \
  config/tutorials/endpoint/writer.json \
  Drone \
  pos \
  10
```

Expected output:
- `bridge_writer` prints `sent seq=...`
- `bridge_reader` prints `recv bytes=... text="ts=... seq=..."`
- Transfer happens at most once per 100ms even if the writer is faster.

### Ticker (interval 100ms)

Terminal 1 (bridge daemon):
```bash
./build/hakoniwa-pdu-bridge \
  config/tutorials/bridge-ticker.json \
  5000 \
  config/tutorials/endpoint_container.json \
  node1
```

Terminal 2 (reader, destination):
```bash
build/examples/bridge_reader \
  config/tutorials/endpoint/reader.json \
  Drone \
  pos \
  10
```

Terminal 3 (writer, source):
```bash
build/examples/bridge_writer \
  config/tutorials/endpoint/writer.json \
  Drone \
  pos \
  10
```

Expected output:
- `bridge_writer` prints `sent seq=...`
- `bridge_reader` prints `recv bytes=... text="ts=... seq=..."`
- Transfer occurs every 100ms even if the writer only sends every 5 seconds.

## Policy-specific tutorials

- `immediate`: `docs/tutorials/immediate.md`
- `throttle`: `docs/tutorials/throttle.md`
- `ticker`: `docs/tutorials/ticker.md`
