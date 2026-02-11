# Tutorials

These tutorials explain how to use each transfer policy with the existing bridge daemon (`src/daemon.cpp`).

## Common setup

All examples assume the tutorial configs under `config/tutorials/` and the endpoint container file `config/tutorials/endpoint_container.json`.

The bridge daemon runs as:

```bash
./build/hakoniwa-pdu-bridge <bridge.json> <delta_time_step_usec> <endpoint_container.json> [node_name]
```

Notes:
- `endpoints` and `wireLinks` are accepted by the schema but ignored by the current implementation.
- `config_path` values are resolved relative to the `bridge.json` location.
- The bridge daemon uses an endpoint container; the writer/reader examples open endpoint configs directly.
- The tutorial configs use TCP loopback (`127.0.0.1`) on ports `9000` and `9100`.

Ready-to-run configs:
- `config/tutorials/bridge-immediate.json`
- `config/tutorials/bridge-immediate-atomic.json`
- `config/tutorials/bridge-throttle.json`
- `config/tutorials/bridge-ticker.json`
- `config/tutorials/endpoint_container.json`

## Sample programs

You typically need:
- a **writer** that sends PDUs into the source endpoint
- a **reader** that receives PDUs from the destination endpoint

The source code is in:
- `examples/bridge_writer.cpp`
- `examples/bridge_reader.cpp`

### Build examples

```bash
cmake -S . -B build -DHAKO_PDU_BRIDGE_BUILD_EXAMPLES=ON
cmake --build build
```

This builds:
- `build/examples/bridge_writer`
- `build/examples/bridge_reader`

### Usage

Writer:
```bash
build/examples/bridge_writer <endpoint.json> <robot> <pdu> [interval_ms]
```

Reader:
```bash
build/examples/bridge_reader <endpoint.json> <robot> <pdu> [interval_ms]
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

### Immediate (atomic)

Use `config/tutorials/bridge-immediate-atomic.json` and run the same steps as the default example.
The atomic group waits until all PDUs in the group have updated before a transfer.

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
  5000
```

Expected output:
- `bridge_writer` prints `sent seq=...`
- `bridge_reader` prints `recv bytes=... text="ts=... seq=..."`
- Transfer occurs every 100ms even if the writer only sends every 5 seconds.

## Policy-specific tutorials

- `immediate`: `docs/tutorials/immediate.md`
- `throttle`: `docs/tutorials/throttle.md`
- `ticker`: `docs/tutorials/ticker.md`

## Advanced example (two nodes, TCP)

The integration configs under `test/config/tcp/` connect two nodes over TCP.

Node 1:
```bash
./build/hakoniwa-pdu-bridge \
  test/config/tcp/bridge.json \
  1000 \
  test/config/tcp/endpoints.json \
  node1
```

Node 2:
```bash
./build/hakoniwa-pdu-bridge \
  test/config/tcp/bridge.json \
  1000 \
  test/config/tcp/endpoints.json \
  node2
```
