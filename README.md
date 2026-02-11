# hakoniwa-pdu-bridge

`hakoniwa-pdu-bridge` is a logical transfer component that focuses on **controlling the timing of data flow** between PDU (Protocol Data Unit) channels.

The core design is to **separate the decision of when to transfer** from **how to communicate** (TCP/UDP/SHM, etc.). This bridge only decides when to transfer and delegates actual I/O to `hakoniwa-pdu-endpoint`.

---

## What this is / isn't

**This is:**
- A transfer layer that declares logical PDU flows
- A definition of which PDUs flow where, under which transfer policy model

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
- **Policy**: `immediate` / `throttle` / `ticker` transfer policies.
- **TimeSource (injected)**: `ITimeSource` provided by the caller.
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

### Helper scripts

```bash
# Build only
./build.bash

# Build + install to /usr/local/hakoniwa
./install.bash

# Uninstall files installed by install.bash
./uninstall.bash
```

### Install layout

`install.bash` uses `/usr/local/hakoniwa` as the install prefix.

Installed files:
- Headers: `/usr/local/hakoniwa/include/hakoniwa/pdu/bridge/*`
- Library: `/usr/local/hakoniwa/lib/libhakoniwa_pdu_bridge_lib.a`
- CMake package:
  - `/usr/local/hakoniwa/lib/cmake/hakoniwa_pdu_bridge/hakoniwa_pdu_bridgeConfig.cmake`
  - `/usr/local/hakoniwa/lib/cmake/hakoniwa_pdu_bridge/hakoniwa_pdu_bridgeConfigVersion.cmake`
  - `/usr/local/hakoniwa/lib/cmake/hakoniwa_pdu_bridge/hakoniwa_pdu_bridgeTargets.cmake`

### Hakoniwa core install notes

- Headers: `/usr/local/hakoniwa/include/hakoniwa`
- Libraries: `/usr/local/hakoniwa/lib`
- `hakoniwa-pdu-endpoint` default search prefix: `/usr/local/hakoniwa`
  - Header auto-detect target: `hakoniwa/pdu/endpoint.hpp`
  - Library auto-detect target: `libhakoniwa_pdu_endpoint.*`

If shared libraries are not found at runtime, add `LD_LIBRARY_PATH` (Linux) or `DYLD_LIBRARY_PATH` (macOS).

---

## Run

Note: `hakoniwa-pdu-bridge` is a reference daemon that wires the bridge library with a `real` time source.
Library integrators can provide their own `ITimeSource` and execution loop.

```bash
./build/hakoniwa-pdu-bridge <bridge.json> <delta_time_step_usec> <endpoint_container.json> [node_name]
```

- `bridge.json`: config for this repository
- `delta_time_step_usec`: tick interval used by the reference daemon's real-time loop (microseconds)
- `endpoint_container.json`: config for endpoint loader (`hakoniwa-pdu-endpoint`)
- `node_name`: optional, default `node1`
`delta_time_step_usec` only affects the reference daemon sleep interval; policy decisions still read time via the injected `ITimeSource` in the library.

### Example (single node, local)

```bash
./build/hakoniwa-pdu-bridge \
  config/tutorials/bridge-immediate.json \
  1000 \
  config/tutorials/endpoint_container.json \
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

Notes:
- These are integration configs under `test/config/tcp/`.
- The same `endpoints.json` contains both `node1` and `node2`; each daemon selects its node by `node_name`.

---

## Quickstart (1-minute, local)

This is the fastest way to see data flowing on a single machine.

Before running, validate that `bridge.json` and `endpoint_container.json` are consistent:
This catches `endpointId` drift across `bridge.json` and `endpoint_container.json`.
```bash
python3 tools/check_bridge_config.py config/tutorials/bridge-immediate.json --endpoint-container config/tutorials/endpoint_container.json
```

Terminal 1 (bridge daemon):
```bash
./build/hakoniwa-pdu-bridge \
  config/tutorials/bridge-immediate.json \
  1000 \
  config/tutorials/endpoint_container.json \
  node1
```

Terminal 2 (reader):
```bash
build/examples/bridge_reader \
  config/tutorials/endpoint/reader.json \
  Drone \
  pos \
  10
```

Terminal 3 (writer):
```bash
build/examples/bridge_writer \
  config/tutorials/endpoint/writer.json \
  Drone \
  pos \
  10
```

Expected output:
- writer prints `sent seq=...`
- reader prints `recv bytes=... text="ts=... seq=..."`

If it fails, check these first:
- `Bridge build failed: ... endpoint not found`: ensure `endpointId` in `bridge.json` exists in `endpoint_container.json` for the selected `node_name`
- `Failed to open endpoint`: verify `endpoint.json` and its `config_path` resolution
- `PDU size is 0`: check `robot`/`pdu` names against the endpoint `pdu_def_path`
- `No data arrives on reader`: confirm endpoint directions (`in`/`out`) and port conflicts in `config/tutorials/comm/`

## Troubleshooting

- `PDU size is 0`: the `robot`/`pdu` pair does not exist in the endpoint `pdu_def_path` JSON.
- `Failed to open endpoint`: `endpoint.json` path or `config_path` inside it is wrong.
- `Bridge build failed: ... endpoint not found`: the `endpointId` in `bridge.json` does not exist in `endpoint_container.json` for the selected `node_name`.
- No data arrives on reader: check endpoint directions (`in`/`out`) and that ports in `config/tutorials/comm/` are not used by other processes.
- Schema validation fails: install `jsonschema` for the Python checker or use `ajv` for the JSON schema.

## FAQ

- Q: Why is there no `time_source_type` in `bridge.json`? A: The time source is provided by the caller. The library uses the injected `ITimeSource` for policy decisions, and the sample daemon creates a `real` time source and sleeps each loop.
- Q: Why are there two config files? A: `bridge.json` declares logical transfers; `endpoint_container.json` declares concrete endpoints and transport details.
- Q: What does `atomic: true` guarantee? A: The group transfers only after all PDUs in the group have updated; it does not guarantee identical generation timestamps.

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
- `endpoints` / `wireLinks` are accepted by the schema but are not used by the current implementation.
- `endpoints_config_path` is optional and points to an endpoint container JSON file (used by `tools/check_bridge_config.py`).

### Schema validation

```bash
ajv validate -s config/schema/bridge-schema.json -d bridge.json
```

---

## Endpoint container config

`endpoint_container.json` is the **EndpointContainer** config read by `hakoniwa-pdu-endpoint`. It is a list grouped by `nodeId`.

Example (`config/tutorials/endpoint_container.json`):

```json
[
  {
    "nodeId": "node1",
    "endpoints": [
      { "id": "n1-epSrc", "mode": "local", "config_path": "endpoint/bridge-src.json", "direction": "in" },
      { "id": "n1-epDst", "mode": "local", "config_path": "endpoint/bridge-dst.json", "direction": "out" }
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

Example config: `config/tutorials/bridge-immediate-atomic.json`.

---

## Time source (why)

Transfer policies such as `throttle` and `ticker` require a clock.
However, the bridge must not decide **which** clock to use.

The bridge is a policy engine, not a scheduler.
Simulations may run on real time, virtual time, or externally synchronized time.
Selecting the clock is an integration concern, not a transfer concern.

Therefore, the time source is **injected by the caller**.

What this means:
- the library reads time via `ITimeSource` only for policy decisions
- the library itself never sleeps and does not drive the execution loop
- `immediate` is event-driven and ignores the time source
- the sample daemon provides a `real` time source and sleeps each loop, but that is only one caller choice

---

## Runtime delegation (epoch)

At owner switching boundaries, old and new owners may send concurrently, so receivers must discard stale epochs. This is handled in `TransferPdu` and not in policy logic.

---

## Minimal example

```json
{
  "version": "2.0.0",
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
      "source": { "endpointId": "n1-to-n2-src" },
      "destinations": [{ "endpointId": "n1-to-n2-dst" }],
      "transferPdus": [
        { "pduKeyGroupId": "drone_data", "policyId": "immediate_policy" }
      ]
    },
    {
      "id": "node2_from_node1_conn",
      "nodeId": "node2",
      "source": { "endpointId": "n2-from-n1-src" },
      "destinations": [{ "endpointId": "n2-from-n1-dst" }],
      "transferPdus": [
        { "pduKeyGroupId": "drone_data", "policyId": "immediate_policy" }
      ]
    }
  ]
}
```

---

## Design philosophy

This repository implements bridge-side logic only:
- transfer timing is handled here
- transport and endpoint I/O are handled by `hakoniwa-pdu-endpoint`

Responsibility boundaries:
- this bridge decides **when** to transfer
- endpoints decide **how** to communicate (TCP/UDP/SHM/etc.)
- the caller provides the time source and drives the execution loop

The time source is injected to allow integration-specific control over time.
The bridge reads time via `ITimeSource` only for policy decisions, and never sleeps.

## Target users

This component is intended for:
- developers building distributed simulations
- integrators managing multi-node PDU flows
- system architects who need explicit control over transfer timing

It is not intended as a general-purpose messaging library.

## Why declarative configuration

Transfer timing and flow are expressed in `bridge.json`:
- data flow logic is visible in configuration
- timing policies are not hidden in application code
- integration changes do not require recompilation
- delivery/timing assumptions remain explicit and reviewable

The bridge treats transfer behavior as configuration, not embedded logic.
`bridge.json` expresses logical timing/flow, while `endpoint_container.json` expresses concrete transport wiring, keeping assumptions explicit and reviewable.

---

## Further reading

- Transfer policy tutorials: `docs/tutorials/`
- Bridge schema: `config/schema/bridge-schema.json`
- Endpoint configuration: see `hakoniwa-pdu-endpoint`
