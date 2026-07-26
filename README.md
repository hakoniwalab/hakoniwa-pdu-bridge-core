# hakoniwa-pdu-bridge

[![Manifest Build (Ubuntu)](https://github.com/hakoniwalab/hakoniwa-pdu-bridge-core/actions/workflows/manifest-build-ubuntu.yml/badge.svg)](https://github.com/hakoniwalab/hakoniwa-pdu-bridge-core/actions/workflows/manifest-build-ubuntu.yml)
[![Manifest Build (macOS)](https://github.com/hakoniwalab/hakoniwa-pdu-bridge-core/actions/workflows/manifest-build-macos.yml/badge.svg)](https://github.com/hakoniwalab/hakoniwa-pdu-bridge-core/actions/workflows/manifest-build-macos.yml)
[![Manifest Build (Windows)](https://github.com/hakoniwalab/hakoniwa-pdu-bridge-core/actions/workflows/manifest-build-windows.yml/badge.svg)](https://github.com/hakoniwalab/hakoniwa-pdu-bridge-core/actions/workflows/manifest-build-windows.yml)
[![Package Contract](https://github.com/hakoniwalab/hakoniwa-pdu-bridge-core/actions/workflows/package-contract.yml/badge.svg)](https://github.com/hakoniwalab/hakoniwa-pdu-bridge-core/actions/workflows/package-contract.yml)

`hakoniwa-pdu-bridge` is a logical transfer component for controlling **when PDU data flows** between channels.

The bridge separates transfer policy from transport implementation:

- Bridge decides **when** data moves.
- `hakoniwa-pdu-endpoint` decides **how** data moves over TCP, UDP, WebSocket, SHM, storage, and other endpoint types.
- The caller supplies the time source and execution loop used by transfer policies.

This repository provides a Core-free Bridge library and standalone runtime, plus an optional Hakoniwa callback integration.

## Architecture

```text
Bridge Core library
  -> hakoniwa-pdu-endpoint
  -> no Hakoniwa Core requirement

Standalone bridge / monitor
  -> Bridge Core library
  -> no Hakoniwa Core requirement

Hakoniwa-integrated web bridge
  -> internal callback Bridge variant
  -> hakoniwa_pdu_endpoint::core_callback
  -> Hakoniwa Core callback/assets runtime
```

Main responsibilities:

- `BridgeCore`: owns connections and drives `cyclic_trigger()`.
- `BridgeConnection`: binds one source endpoint to one or more destinations.
- `TransferPdu` / `TransferAtomicPduGroup`: performs logical transfer.
- Transfer policies: `immediate`, `throttle`, and `ticker`.
- `EndpointContainer`: endpoint creation and I/O delegated to `hakoniwa-pdu-endpoint`.
- Monitor CLI: runtime inspection such as `health`, `connections`, `list_pdus`, and `tail`.

The Bridge library does not implement transport protocols, persistent queues, retry guarantees, or endpoint JSON loading.

## Supported build model

The preferred build flow is manifest-driven and is the same on Linux, macOS, and Windows.

```text
hakoniwa-build.yaml
        |
        v
python tools/hako.py doctor
        |
        v
python tools/hako.py configure
        |
        v
python tools/hako.py build
        |
        v
python tools/hako.py test
```

Default `hakoniwa-build.yaml`:

```yaml
version: 1

build:
  type: Release
  dir: build
  parallel: 0

components:
  library: true
  standalone_app: true
  hakoniwa_app: false
  monitor: true

validation:
  tests: true
  examples: false
  integration_tcp: false

paths:
  pdu_endpoint_root: ""
  hakoniwa_core_root: ""
  vcpkg_root: ""
```

The default configuration is intentionally Core-free. Set `components.hakoniwa_app: true` only when building the Hakoniwa-integrated callback application.

Detailed design: `docs/build-architecture.md`.

## Prerequisites

For the normal Core-free build:

- C++20 toolchain
- CMake 3.16+
- Python 3
- Boost headers
- installed or resolvable `hakoniwa-pdu-endpoint`

Platform-specific toolchain details are handled by the resolver:

- Linux: GCC/Clang
- macOS: AppleClang
- Windows: MSVC/vswhere, with vcpkg support where needed

Hakoniwa Core is required only for `hakoniwa_app`.

## Build with the manifest

Run the preflight first:

```bash
python3 tools/hako.py doctor
```

Inspect the resolved configuration without building:

```bash
python3 tools/hako.py configure --dry-run
```

Configure, build, and test:

```bash
python3 tools/hako.py configure
python3 tools/hako.py build
python3 tools/hako.py test
```

On Windows, use `python` instead of `python3` when that is the configured Python command.

Resolved build diagnostics are written to:

```text
.hako/resolved-build.yaml
.hako/cmake-args.txt
```

## Direct CMake build

Direct CMake remains supported for maintainers and advanced integrations.

A typical Core-free build is:

```bash
cmake -S . -B build \
  -DHAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP=OFF
cmake --build build
```

A Hakoniwa-integrated build requires installed Hakoniwa Core and a Core-enabled Endpoint package containing the callback variant:

```bash
cmake -S . -B build-hakoniwa \
  -DCMAKE_PREFIX_PATH="/path/to/endpoint;/path/to/hakoniwa-core" \
  -DHAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE=ON \
  -DHAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP=ON
cmake --build build-hakoniwa --target hakoniwa-pdu-web-bridge
```

The integrated web bridge uses the Endpoint callback package target rather than the polling/shakoc frontend.

## Installed CMake package

Install Bridge to a prefix:

```bash
cmake -S . -B build \
  -DCMAKE_INSTALL_PREFIX=/path/to/install
cmake --build build
cmake --install build
```

A downstream CMake consumer should use the installed package contract:

```cmake
find_package(hakoniwa_pdu_bridge CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app
  PRIVATE
    hakoniwa_pdu_bridge::bridge
)
```

Preferred target:

```text
hakoniwa_pdu_bridge::bridge
```

Compatibility target retained for existing consumers such as `hakoniwa-conductor-pro`:

```text
hakoniwa_pdu_bridge::hakoniwa_pdu_bridge_lib
```

The Bridge package resolves `hakoniwa-pdu-endpoint` through its CMake package instead of recreating Endpoint include/library paths manually.

## Package-contract validation

The package-contract CI verifies composition from installed packages, not only in-repository builds.

It covers:

1. Core-free Endpoint install
2. Bridge install
3. external consumers of both Bridge target names
4. Hakoniwa Core PRO install
5. Core-enabled Endpoint callback/polling variants
6. Hakoniwa-integrated Bridge build using `core_callback`

The workflow runs on Ubuntu x64, native Linux ARM64, macOS, and Windows x64.

This downstream test is intentional: exported CMake target defects can remain invisible when each repository is built only by itself.

## Runtime binaries

Depending on selected components, the build provides:

```text
hakoniwa-pdu-bridge
hakoniwa-pdu-web-bridge
hakoniwa-pdu-bridge-monitor
```

`hakoniwa-pdu-bridge` is the standalone reference daemon. It supplies a real-time execution loop while the Bridge library itself remains scheduler-independent.

## Standalone bridge

Usage:

```bash
./build/hakoniwa-pdu-bridge \
  <bridge.json> \
  <delta_time_step_usec> \
  <endpoint_container.json> \
  [node_name]
```

Example:

```bash
./build/hakoniwa-pdu-bridge \
  config/tutorials/bridge-immediate.json \
  1000 \
  config/tutorials/endpoint_container.json \
  node1
```

Two-node TCP example:

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

`delta_time_step_usec` controls the standalone daemon's loop sleep. Transfer policies still read time through the injected `ITimeSource`.

## Hakoniwa web bridge

`hakoniwa-pdu-web-bridge` is the Hakoniwa callback integration used for WebSocket bridging.

It:

- registers as a Hakoniwa callback asset
- uses Endpoint SHM callback support
- transfers once per simulation step
- uses `hakoniwa_callback` time for policy evaluation
- optionally applies real-time sleep for wall-clock pacing

Default configuration root:

```text
config/web_bridge
```

Important files:

```text
config/web_bridge/bridge/bridge.json
config/web_bridge/endpoint/endpoint_container.json
config/web_bridge/pdu/drone-pdudef.json
```

Default WebSocket endpoint:

```text
ws://127.0.0.1:8765
```

Example:

```bash
./build/hakoniwa-pdu-web-bridge \
  --asset-name WebBridge \
  --node-name web_bridge_node1 \
  --delta-time-step-usec 20000
```

Useful options:

```text
--config-root <path>
--bridge-config <path>
--endpoint-container <path>
--asset-config <path>
--enable-ondemand
--ondemand-mux-config <path>
--node-name <name>
--asset-name <name>
--delta-time-step-usec <usec>
--disable-real-sleep
```

Additional managed config sets:

- `config/web_bridge_fleets/`: fleet-oriented SHM -> WebSocket visualization traffic
- `config/web_bridge_game/`: minimal WebSocket -> SHM game-command bridge

## Quickstart

Validate Bridge/Endpoint configuration consistency first:

```bash
python3 tools/check_bridge_config.py \
  config/tutorials/bridge-immediate.json \
  --endpoint-container config/tutorials/endpoint_container.json
```

Start the bridge:

```bash
./build/hakoniwa-pdu-bridge \
  config/tutorials/bridge-immediate.json \
  1000 \
  config/tutorials/endpoint_container.json \
  node1
```

Then run the tutorial writer and reader from separate terminals:

```bash
build/examples/bridge_writer \
  config/tutorials/endpoint/writer.json \
  Drone pos 10
```

```bash
build/examples/bridge_reader \
  config/tutorials/endpoint/reader.json \
  Drone pos 10
```

Expected behavior:

- writer prints `sent seq=...`
- reader prints received payloads

## Monitor CLI

The monitor client provides on-demand runtime introspection.

```bash
build/hakoniwa-pdu-bridge-monitor <monitor_endpoint.json> health
build/hakoniwa-pdu-bridge-monitor <monitor_endpoint.json> connections
build/hakoniwa-pdu-bridge-monitor <monitor_endpoint.json> sessions
build/hakoniwa-pdu-bridge-monitor <monitor_endpoint.json> list_pdus <connection_id>
build/hakoniwa-pdu-bridge-monitor <monitor_endpoint.json> tail <connection_id> throttle 100
```

Tutorial:

```text
docs/tutorials/monitor.md
```

## Bridge configuration

`bridge.json` follows:

```text
config/schema/bridge-schema.json
```

Main top-level fields:

- `version`
- `transferPolicies`
- `nodes`
- `pduKeyGroups`
- `connections`

`bridge.json` describes logical timing and transfer flow. `endpoint_container.json` describes concrete endpoint/transport wiring.

Validate configuration with:

```bash
python3 tools/check_bridge_config.py path/to/bridge.json
python3 tools/check_bridge_config.py \
  path/to/bridge.json \
  --endpoint-container path/to/endpoint_container.json
```

## Transfer policies

Supported policy types:

- `immediate`: transfer when source data updates
- `throttle`: transfer updates while enforcing a minimum interval
- `ticker`: send the latest value on a fixed interval

When `immediate` uses `atomic: true`, all PDUs in the same transfer group are emitted only after the full group has updated. Include `hako_msgs/SimTime` when the frame needs an explicit simulation-time signal.

## Time-source model

The Bridge library is a policy engine, not a scheduler.

Therefore:

- the library receives an `ITimeSource` from its caller
- the library does not choose real vs virtual vs Hakoniwa time
- the library itself does not sleep
- `throttle` and `ticker` evaluate their timing against the injected source
- the standalone daemon provides real time
- the Hakoniwa web bridge provides callback simulation time

## Tests

Normal test flow:

```bash
python3 tools/hako.py test
```

Direct CMake flow:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Set `HAKO_TEST_CONFIG_DIR` to override the test config root when needed.

## CI model

Two CI layers validate different guarantees.

### Manifest Build

Ubuntu, macOS, and Windows workflows exercise the user-facing manifest build path with Core disabled.

### Package Contract

The package-contract workflow exercises installed CMake packages and the optional Hakoniwa callback integration, including native Linux ARM64.

All workflows use per-ref concurrency so superseded runs are cancelled automatically. Documentation-only changes do not trigger the heavy build workflows.

## Design philosophy

Responsibility boundaries are intentional:

```text
Bridge
  -> when to transfer

Endpoint
  -> how to communicate

Caller / runtime
  -> which clock and execution loop to use
```

This separation keeps transfer policy visible and testable while allowing transport and runtime implementations to evolve independently.

## Further reading

- Build architecture: `docs/build-architecture.md`
- Transfer tutorials: `docs/tutorials/`
- Monitor tutorial: `docs/tutorials/monitor.md`
- Bridge schema: `config/schema/bridge-schema.json`
- Endpoint implementation: `hakoniwalab/hakoniwa-pdu-endpoint`
