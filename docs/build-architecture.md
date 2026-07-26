# Build Architecture

## Purpose

`hakoniwa-pdu-bridge-core` uses a manifest-driven build flow so users can select Bridge capabilities without having to know repository-specific CMake flags or platform-specific dependency layouts.

The user-facing source of truth is `hakoniwa-build.yaml`.

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

The same logical build model is used on Linux, macOS, and Windows. Platform-specific compiler, dependency, and toolchain handling stays below the resolver.

## Current component model

The repository has three architectural layers.

```text
hakoniwa-pdu-bridge-core
|
+-- Bridge Core library
|    +-- dependency: hakoniwa-pdu-endpoint base package target
|
+-- standalone bridge / monitor
|    +-- dependency: Bridge Core library
|    +-- no Hakoniwa Core requirement
|
+-- Hakoniwa-integrated web bridge
     +-- internal callback Bridge variant
     +-- dependency: hakoniwa-pdu-endpoint::core_callback
     +-- dependency: Hakoniwa Core callback/assets runtime
```

The Bridge Core library owns logical transfer behavior such as routing, transfer-policy evaluation, atomic groups, and cyclic triggering. Transport I/O is delegated to `hakoniwa-pdu-endpoint`.

Hakoniwa Core is not an unconditional Bridge dependency. It is pulled in only for applications that explicitly integrate with the Hakoniwa runtime.

## Installed CMake package contract

The preferred installed target is:

```cmake
find_package(hakoniwa_pdu_bridge CONFIG REQUIRED)

target_link_libraries(my_app
  PRIVATE
    hakoniwa_pdu_bridge::bridge
)
```

For compatibility with existing consumers, the package also exposes:

```text
hakoniwa_pdu_bridge::hakoniwa_pdu_bridge_lib
```

The compatibility target is intentionally retained for consumers such as `hakoniwa-conductor-pro`.

The installed Bridge package resolves `hakoniwa-pdu-endpoint` through its CMake package contract rather than reconstructing Endpoint include/library paths manually.

## Endpoint dependency boundary

### Core-free Bridge consumers

The public Bridge package uses the base Endpoint target:

```text
hakoniwa_pdu_bridge::bridge
        |
        v
hakoniwa_pdu_endpoint::hakoniwa_pdu_endpoint
```

This path is intended for normal library, standalone bridge, monitor, TCP, UDP, WebSocket, storage, and other non-Hakoniwa-Core uses.

### Hakoniwa callback integration

The Hakoniwa-integrated web bridge uses an internal callback Bridge target:

```text
hakoniwa-pdu-web-bridge
        |
        v
hakoniwa_pdu_bridge_core_callback
        |
        v
hakoniwa_pdu_endpoint::core_callback
        |
        v
hakoniwa-core::assets
```

The internal callback Bridge target is not exported as the public package contract. This prevents the public Bridge library from inheriting Hakoniwa Core dependencies.

The polling/shakoc frontend remains a separate concern. `hakoniwa-conductor-pro`, for example, keeps its own polling/shakoc dependency and consumes the Bridge package independently.

## Manifest v1

The default manifest is deliberately small.

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

### `components.library`

Builds the Bridge Core library. It requires `hakoniwa-pdu-endpoint`, but not Hakoniwa Core.

### `components.standalone_app`

Builds the standalone bridge executable around the Bridge Core library and a normal real-time execution loop. It does not require Hakoniwa Core.

### `components.hakoniwa_app`

Builds the Hakoniwa-integrated web bridge. Selecting this capability enables Hakoniwa Core dependency resolution and uses the Endpoint callback variant.

### `components.monitor`

Builds the monitor CLI. The monitor remains Core-free unless a future monitor capability explicitly requires Hakoniwa runtime integration.

## Dependency resolution

The resolver derives dependencies from selected capabilities.

```text
library
  -> hakoniwa-pdu-endpoint

standalone_app
  -> library

monitor
  -> library

hakoniwa_app
  -> internal callback Bridge variant
  -> hakoniwa-pdu-endpoint::core_callback
  -> Hakoniwa Core
```

The resolver rejects inconsistent selections before invoking CMake.

## Doctor and observability

`doctor` performs prerequisite checks relevant to the selected manifest only.

Resolution artifacts are written under `.hako/`:

```text
.hako/resolved-build.yaml
.hako/cmake-args.txt
```

These files expose the resolved component selection, dependency roots, platform details, and generated CMake arguments.

## Direct CMake compatibility

The manifest tool is an orchestration layer, not a replacement for CMake. Direct CMake use remains supported for maintainers and advanced integrations.

The recommended consumer boundary, however, is the installed CMake package contract rather than manually reconstructing include and library paths.

## Platform model

The logical build architecture is OS-neutral.

- Linux: GCC/Clang, native x64 and ARM64 package-consumer validation.
- macOS: AppleClang and native CMake package resolution.
- Windows: MSVC/vswhere and optional vcpkg dependency resolution.

Hakoniwa Core remains static-first on Windows. The Bridge build must not require converting Core libraries back to DLLs.

## CI responsibilities

Two CI layers validate different contracts.

### Manifest Build

The Manifest Build workflows validate the user-facing `tools/hako.py` flow with a Core-free Endpoint/Bridge configuration on Ubuntu, macOS, and Windows.

### Package Contract

The Package Contract workflow validates installed-package composition:

1. build and install a Core-free Endpoint package;
2. build and install the Bridge package;
3. build external consumers using both the preferred and compatibility Bridge target names;
4. build and install Hakoniwa Core PRO;
5. build and install Core-enabled Endpoint variants;
6. build the Hakoniwa-integrated Bridge using `core_callback`.

This downstream-consumer validation is intentional. It catches exported CMake contract defects that may not appear when each repository is built only in isolation.

## Design principles

1. **Bridge Core stays Core-free.** Hakoniwa Core belongs only to integrations that use Hakoniwa runtime APIs.
2. **The manifest describes intent.** Users choose capabilities, not platform-specific CMake flags.
3. **Installed packages are the composition boundary.** Cross-repository integrations should consume exported CMake targets.
4. **Callback and polling frontends remain explicit.** New Hakoniwa callback integrations use the Endpoint callback variant rather than an all-in dependency graph.
5. **Windows remains static-first for Hakoniwa Core.** Cross-platform support must not reintroduce DLL handling as a prerequisite.
6. **Resolution is observable.** `.hako/` artifacts make generated build decisions inspectable.
7. **External consumer tests are part of the architecture contract.** A package is not considered complete merely because its own repository builds successfully.
