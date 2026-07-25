# Build Architecture

## Purpose

`hakoniwa-pdu-bridge-core` should support a build flow that is consistent across Linux, macOS, and Windows without requiring users to understand repository-specific CMake flags, shell scripts, or platform-specific dependency layouts.

The build architecture follows the same direction as `hakoniwa-pdu-endpoint`:

1. **intent manifest** — the user states which bridge capabilities and applications they want;
2. **doctor/configure/build tool** — resolves dependencies and translates that intent into the current platform's CMake/toolchain configuration.

The intended user-facing source of truth is `hakoniwa-build.yaml`.

This document also clarifies a dependency boundary that is currently blurred in the CMake configuration: the **Bridge Core library** should remain independent from Hakoniwa Core itself. Hakoniwa Core dependencies belong only to applications/features that actually integrate with Hakoniwa runtime APIs.

## Target component model

The repository should be treated as three layers rather than one monolithic build target.

```text
hakoniwa-pdu-bridge-core
|
+-- Bridge Core library
|    +-- dependency: hakoniwa-pdu-endpoint
|
+-- standalone bridge application
|    +-- dependency: Bridge Core library
|    +-- real-time execution loop
|    +-- no Hakoniwa Core requirement
|
+-- Hakoniwa-integrated bridge application
     +-- dependency: Bridge Core library
     +-- Hakoniwa callback/runtime integration
     +-- requires Hakoniwa Core
```

The Bridge Core library owns transfer logic such as routing, policy evaluation, and cyclic triggering. Transport I/O remains delegated to `hakoniwa-pdu-endpoint`.

The standalone application provides an executable loop around the library for configurations such as TCP-to-TCP bridging.

Hakoniwa-integrated applications add runtime integration such as callback assets, SHM access, or Hakoniwa-specific time sources. Those requirements must not become unconditional dependencies of the Bridge Core library.

## Current dependency mismatch

The current CMake configuration does not fully reflect the intended boundary.

Today:

- `hakoniwa_pdu_bridge_lib` links to `hakoniwa_pdu_endpoint`;
- the imported endpoint target is augmented with `assets` and `shakoc`;
- `hakoniwa-pdu-bridge` and `hakoniwa-pdu-web-bridge` also link `shakoc` directly;
- `/usr/local/hakoniwa` is embedded in several discovery and link paths.

This makes Hakoniwa Core-related libraries effectively part of builds that should only require the endpoint layer.

The manifest-driven build work should therefore be used to make the dependency graph explicit rather than simply adding Windows-specific build scripts.

## Design principles

### 1. The Bridge Core library has one primary external runtime dependency

The default Bridge Core build should require:

- C++20 toolchain;
- CMake;
- `hakoniwa-pdu-endpoint`;
- header-only/build-time dependencies such as `nlohmann_json` as required by the implementation.

Hakoniwa Core is not a default Bridge Core dependency.

### 2. Applications are optional capabilities

Executable applications should be selected independently from the library build.

A minimal build can produce only the library.

A common standalone build can produce the library plus the standalone bridge executable.

Hakoniwa-integrated applications are opt-in and pull in Hakoniwa Core only when selected.

### 3. The manifest describes intent, not CMake syntax

Users should select capabilities such as `standalone_app` or `hakoniwa_app`. They should not need to know implementation switches or platform-specific library names.

### 4. OS-specific behavior stays below the resolver

The normal workflow should be identical on all supported platforms:

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

The platform layer handles compiler discovery, library naming, runtime paths, and toolchain details.

Examples:

- Windows: MSVC/vswhere, optional vcpkg toolchain, DLL search directories;
- macOS: AppleClang and dylib discovery;
- Linux: GCC/Clang and shared-object discovery.

The logical build model should not change by OS.

### 5. Existing direct CMake use remains available

The configurator is an orchestration layer, not a replacement for CMake.

Existing maintainer/developer workflows may remain available while the manifest path becomes the recommended user-facing flow.

### 6. Resolution is observable

As with `hakoniwa-pdu-endpoint`, the resolver should write diagnostic artifacts such as:

```text
.hako/resolved-build.yaml
.hako/cmake-args.txt
```

These files should make selected applications, dependency paths, platform details, and generated CMake options visible.

## Manifest v1

The initial manifest should deliberately remain small.

Example:

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
  integration_tcp: true

paths:
  pdu_endpoint_root: ""
  hakoniwa_core_root: ""
  vcpkg_root: ""
```

### `components.library`

Builds the Bridge Core library.

This is the base component and requires `hakoniwa-pdu-endpoint`.

### `components.standalone_app`

Builds the standalone bridge executable using the Bridge Core library and a normal real-time execution loop.

This component must not require Hakoniwa Core merely because it is built in this repository.

### `components.hakoniwa_app`

Builds applications that integrate with Hakoniwa runtime facilities.

Selecting this component enables Hakoniwa Core dependency resolution.

The first implementation may map this capability to the existing web/callback bridge application, but the manifest name should describe the architectural capability rather than a historical executable name.

### `components.monitor`

Builds the monitor CLI independently from Hakoniwa Core unless the monitor implementation itself requires a Hakoniwa-specific feature.

## Dependency resolution

The resolver should derive requirements from selected components.

```text
library
  -> hakoniwa-pdu-endpoint

standalone_app
  -> library

monitor
  -> library

hakoniwa_app
  -> library
  -> Hakoniwa Core
```

The resolver should reject internally inconsistent selections before CMake runs.

For example, if an application requires a disabled base component, the resolver may either enable the base component automatically or fail with a precise diagnostic. The chosen rule should be deterministic and documented.

## Doctor

`doctor` is the preflight entry point.

It should inspect the manifest and report only prerequisites relevant to the requested build.

Example for a standalone TCP configuration:

```text
[OK] Python
[OK] CMake >= 3.16
[OK] C++20 compiler
[OK] hakoniwa-pdu-endpoint
[SKIP] Hakoniwa Core (not requested)
[SKIP] vcpkg (not required by resolved configuration)
```

Example for a Hakoniwa-integrated build:

```text
[OK] Python
[OK] CMake >= 3.16
[OK] C++20 compiler
[OK] hakoniwa-pdu-endpoint
[OK] Hakoniwa Core
[OK] assets
[OK] shakoc
```

On Windows, diagnostics should identify concrete missing prerequisites rather than surfacing a later linker error.

Example:

```text
[OK] Visual Studio / MSVC
[OK] CMake
[NG] hakoniwa-pdu-endpoint library not found
     searched: ...
     configure paths.pdu_endpoint_root or install the endpoint package first
```

`doctor` should also validate the manifest itself and detect impossible or unsupported combinations before configuration.

## Configure

`configure` resolves the manifest into a deterministic build model and translates it into CMake arguments.

Responsibilities include:

- detect the host platform and compiler;
- resolve `hakoniwa-pdu-endpoint` headers and library;
- resolve Hakoniwa Core only when requested;
- resolve optional Windows toolchain/runtime directories;
- explicitly enable or disable repository build targets;
- write `.hako/resolved-build.yaml`;
- write `.hako/cmake-args.txt`;
- invoke CMake unless `--dry-run` is selected.

The resolved configuration should be explicit enough that direct CMake defaults cannot silently change the requested build.

## Validation model

The first cross-platform acceptance target should deliberately avoid Hakoniwa Core.

```text
hakoniwa-build.yaml
  -> doctor
  -> configure
  -> build Bridge Core library
  -> build standalone bridge application
  -> run TCP-to-TCP integration test
```

This path should be supported on:

- Linux;
- macOS;
- Windows.

A successful standalone TCP path establishes the Windows baseline without coupling the first milestone to Hakoniwa Core portability.

Hakoniwa-integrated validation can be added as a separate capability matrix after the core path is stable.

## Windows strategy

Windows support should be a consequence of the architecture, not a parallel collection of PowerShell-only build logic.

The first Windows milestone is:

1. discover MSVC reliably;
2. discover an installed/built `hakoniwa-pdu-endpoint`;
3. build the Bridge Core library;
4. build the standalone bridge application;
5. run a TCP integration smoke test.

Hakoniwa Core, SHM, callback assets, and web bridge integration are later feature milestones unless they are already independently available on Windows.

## Migration strategy

### Phase 1: architecture and dependency cleanup

- add this architecture document;
- define the manifest schema;
- introduce `tools/hako.py doctor`;
- introduce `tools/hako.py configure --dry-run`;
- separate Bridge Core dependency requirements from Hakoniwa-integrated applications;
- keep existing build scripts/CMake entry points working.

### Phase 2: manifest-driven build

- add `hakoniwa-build.yaml`;
- generate explicit CMake target/feature selections;
- add `build` and `test` commands;
- add standalone TCP integration validation;
- validate Linux/macOS/Windows through the same logical flow.

### Phase 3: CI and optional application coverage

- migrate CI jobs to the manifest flow;
- add Hakoniwa Core-enabled application validation;
- reduce duplicated platform logic in shell/PowerShell scripts;
- add executable capability-matrix tests.

## Maintainer rule

When a new bridge application, transport-facing integration, or Hakoniwa runtime feature is added:

1. decide which component/capability owns the dependency;
2. do not add the dependency unconditionally to the Bridge Core library;
3. add the dependency rule to the resolver;
4. expose the resolved decision in `.hako/resolved-build.yaml`;
5. add a smoke/integration test for the new capability combination.

The key invariant is:

> The Bridge Core library is transport-orchestration logic built on `hakoniwa-pdu-endpoint`; Hakoniwa Core integration is an optional application/runtime capability, not an implicit property of the library.
