# hakoniwa-pdu-bridge

`hakoniwa-pdu-bridge` is a logical transfer component that focuses on **controlling the timing of data flow** between PDU (Protocol Data Unit) channels.

The core design is to **separate the decision of when to transfer** from **how to communicate** (TCP/UDP/SHM, etc.). This bridge only decides when to transfer and delegates actual I/O to `hakoniwa-pdu-endpoint`.

---

## What this is / isn't

**This is:**
- A transfer layer that declares logical PDU flows
- A definition of which PDUs flow where, under which time model

**This is NOT:**
- A transport implementation (TCP/UDP/WebSocket/Zenoh/SHM, etc.)
- Delivery guarantees, retries, or persistent queues
- An endpoint JSON loader (handled by `hakoniwa-pdu-endpoint`)

---

## Architecture

### Main components

- **BridgeDaemon**: entry point that builds and runs `BridgeCore`.
- **BridgeCore**: holds `BridgeConnection`s and drives `cyclic_trigger()`.
- **BridgeConnection**: binds a source to destinations and holds `TransferPdu`.
- **TransferPdu / TransferAtomicPduGroup**: transfers a single PDU or an atomic PDU group.
- **Policy**: `immediate` / `throttle` / `ticker` time models.
- **TimeSource**: `real` / `virtual` / `hakoniwa` (see note below).
- **EndpointContainer**: endpoint creation and management (`hakoniwa-pdu-endpoint`).

### Data flow (high level)

1. `BridgeDaemon` loads `bridge.json` and builds `BridgeCore`.
2. `BridgeCore` manages `BridgeConnection`s and their `TransferPdu`s.
3. `TransferPdu` applies policy decisions to move data from src to dst endpoints.

---

## Build

### Dependencies

- C++20 compiler (GCC/Clang)
- CMake 3.16+
- Hakoniwa core library (installed under `/usr/local/hakoniwa`)
- Installed `hakoniwa-pdu-endpoint` library and headers

### hakoniwa-pdu-endpoint install layout

This project expects the following layout for `hakoniwa-pdu-endpoint`:

```
<prefix>/
  include/hakoniwa/pdu/endpoint.hpp
  lib/libhakoniwa_pdu_endpoint.(a|so|dylib)
```

Default prefix is `/usr/local/hakoniwa`. You can override it with:

```bash
cmake -S . -B build -DHAKO_PDU_ENDPOINT_PREFIX=/path/to/prefix
```

If your layout is non-standard, set these explicitly:

```bash
cmake -S . -B build \
  -DHAKO_PDU_ENDPOINT_INCLUDE_DIR=/path/to/include \
  -DHAKO_PDU_ENDPOINT_LIBRARY=/path/to/libhakoniwa_pdu_endpoint.so
```

Note: `hakoniwa-pdu-endpoint` depends on Hakoniwa core libs (`assets`, `shakoc`). Ensure they are in your library path, typically:

```
/usr/local/hakoniwa/lib
```

### Steps

```bash
# 1. out-of-source build
cmake -S . -B build \
  -DHAKO_PDU_ENDPOINT_PREFIX=/usr/local/hakoniwa

# 2. build
cmake --build build
```

The binary `build/hakoniwa-pdu-bridge` will be generated.

To build the example programs:

```bash
cmake -S . -B build -DHAKO_PDU_BRIDGE_BUILD_EXAMPLES=ON
cmake --build build
```

### Hakoniwa core install notes

- Headers: `/usr/local/hakoniwa/include/hakoniwa`
- Libraries: `/usr/local/hakoniwa/lib`
- `hakoniwa-pdu-endpoint` default search prefix: `/usr/local/hakoniwa`
  - Header auto-detect target: `hakoniwa/pdu/endpoint.hpp`
  - Library auto-detect target: `libhakoniwa_pdu_endpoint.*`

If shared libraries are not found at runtime, add `LD_LIBRARY_PATH` (Linux) or `DYLD_LIBRARY_PATH` (macOS).

---

## Run

```bash
./build/hakoniwa-pdu-bridge <bridge.json> <delta_time_step_usec> <endpoint_container.json> [node_name]
```

- `bridge.json`: config for this repository
- `delta_time_step_usec`: time step for the time source (microseconds)
- `endpoint_container.json`: config for endpoint loader (`hakoniwa-pdu-endpoint`)
- `node_name`: optional, default `node1`

### Example (single node, local)

```bash
./build/hakoniwa-pdu-bridge \
  config/sample/simple-bridge.json \
  1000 \
  test/config/core_flow/endpoints.json \
  node1
```

### Example (two nodes, TCP)

```bash
# node1
./build/hakoniwa-pdu-bridge \
  test/config/tcp/bridge.json \
  1000 \
  test/config/tcp/endpoints.json \
  node1

# node2
./build/hakoniwa-pdu-bridge \
  test/config/tcp/bridge.json \
  1000 \
  test/config/tcp/endpoints.json \
  node2
```

---

## Tests

Use GTest. After building, run `ctest`.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

You can override the config root with `HAKO_TEST_CONFIG_DIR`.

```bash
HAKO_TEST_CONFIG_DIR=/path/to/test/config ctest --test-dir build
```

---

## Config check tool

There is a helper script to validate `bridge.json` and check referenced paths.

```bash
python3 tools/check_bridge_config.py path/to/bridge.json
python3 tools/check_bridge_config.py path/to/bridge.json --endpoint-container path/to/endpoint_container.json
```

Notes:
- Schema validation requires the `jsonschema` Python package.
- Path checks ensure `config_path` entries exist on disk (resolved relative to each JSON file).

---

## Tutorials

Policy-specific tutorials live under `docs/tutorials/`:

- `docs/tutorials/README.md`
- `docs/tutorials/immediate.md`
- `docs/tutorials/throttle.md`
- `docs/tutorials/ticker.md`

---

## Bridge configuration

`bridge.json` must follow `config/schema/bridge-schema.json`.

Required top-level fields:
- `version` (currently `2.0.0`)
- `transferPolicies`
- `nodes`
- `pduKeyGroups`
- `connections`

Constraints:
- IDs must match `^[A-Za-z][A-Za-z0-9_\-\.]*$`
- `throttle` and `ticker` require `intervalMs`
- `immediate` must not specify `intervalMs`

Notes:
- `time_source_type` can be in `bridge.json`, but the current implementation uses CLI `delta_time_step_usec` and a fixed `real` time source.
- `endpoints` / `wireLinks` are not used by the current implementation.

### Schema validation

```bash
ajv validate -s config/schema/bridge-schema.json -d bridge.json
```

---

## Endpoint container config

`endpoint_container.json` is the **EndpointContainer** config read by `hakoniwa-pdu-endpoint`. It is a list grouped by `nodeId`.

Example (`test/config/core_flow/endpoints.json`):

```json
[
  {
    "nodeId": "node1",
    "endpoints": [
      { "id": "n1-epSrc", "mode": "local", "config_path": "../../../config/sample/endpoint/n1-epSrc.json", "direction": "out" },
      { "id": "n1-epDst", "mode": "local", "config_path": "../../../config/sample/endpoint/n1-epDst.json", "direction": "in" }
    ]
  }
]
```

`config_path` is resolved **relative to the endpoint_container.json file**.

---

## Transfer policies (overview)

- **immediate**: transfer on update (lowest latency)
- **throttle**: follow updates but enforce a minimum interval
- **ticker**: send the latest value on a fixed interval, even without updates

### immediate (atomic)

If `immediate` has `atomic: true`, all PDUs in the same `transferPdus` group are treated as one frame.

- transfer only after all target PDUs have been updated
- frame time `T_frame` is the time observed by the bridge
- does not guarantee identical generation timestamps for each PDU

**Important:** When using `atomic: true`, include `hako_msgs/SimTime` to signal time.

---

## Time source

`time_source_type` defines the time base used by `throttle`/`ticker`.

- `real`: system wall-clock time
- `virtual`: externally provided virtual time
- `hakoniwa`: synchronized with Hakoniwa core time

**Current implementation:** uses `real` with CLI `delta_time_step_usec` and ignores `time_source_type`.

---

## Runtime delegation (epoch)

At owner switching boundaries, old and new owners may send concurrently, so receivers must discard stale epochs. This is handled in `TransferPdu` and not in policy logic.

---

## Minimal example

```json
{
  "version": "2.0.0",
  "time_source_type": "virtual",
  "transferPolicies": {
    "immediate_policy": { "type": "immediate" }
  },
  "nodes": [
    { "id": "node1" },
    { "id": "node2" }
  ],
  "pduKeyGroups": {
    "drone_data": [
      { "id": "Drone.pos", "robot_name": "Drone", "pdu_name": "pos" },
      { "id": "Drone.status", "robot_name": "Drone", "pdu_name": "status" }
    ]
  },
  "connections": [
    {
      "id": "node1_to_node2_conn",
      "nodeId": "node1",
      "source": { "endpointId": "n1-src" },
      "destinations": [{ "endpointId": "n1-dst" }],
      "transferPdus": [
        { "pduKeyGroupId": "drone_data", "policyId": "immediate_policy" }
      ]
    },
    {
      "id": "node2_from_node1_conn",
      "nodeId": "node2",
      "source": { "endpointId": "n2-src" },
      "destinations": [{ "endpointId": "n2-dst" }],
      "transferPdus": [
        { "pduKeyGroupId": "drone_data", "policyId": "immediate_policy" }
      ]
    }
  ]
}
```
