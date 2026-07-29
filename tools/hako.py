#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Mapping


DEFAULT_CONFIG: Dict[str, Any] = {
    "version": 1,
    "build": {"type": "Release", "dir": "build", "parallel": 0},
    "components": {
        "library": True,
        "standalone_app": True,
        "hakoniwa_app": False,
        "monitor": True,
    },
    "validation": {"tests": True, "examples": False, "integration_tcp": False},
    "paths": {"pdu_endpoint_root": "", "hakoniwa_core_root": "", "vcpkg_root": ""},
}

VALID_BUILD_TYPES = {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"}


class ConfigError(RuntimeError):
    pass


def _strip_comment(text: str) -> str:
    quote: str | None = None
    escaped = False
    out: list[str] = []
    for ch in text:
        if escaped:
            out.append(ch)
            escaped = False
            continue
        if ch == "\\" and quote:
            out.append(ch)
            escaped = True
            continue
        if ch in {"'", '"'}:
            if quote is None:
                quote = ch
            elif quote == ch:
                quote = None
            out.append(ch)
            continue
        if ch == "#" and quote is None:
            break
        out.append(ch)
    return "".join(out).rstrip()


def _parse_scalar(text: str) -> Any:
    value = text.strip()
    if value == "":
        return {}
    lowered = value.lower()
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    if lowered in {"null", "~"}:
        return None
    if value.startswith(('"', "'")):
        if len(value) < 2 or value[-1] != value[0]:
            raise ConfigError(f"unterminated quoted scalar: {value}")
        if value[0] == '"':
            try:
                return json.loads(value)
            except json.JSONDecodeError as exc:
                raise ConfigError(f"invalid quoted scalar: {value}") from exc
        return value[1:-1].replace("''", "'")
    try:
        return int(value)
    except ValueError:
        return value


def load_simple_yaml(path: Path) -> Dict[str, Any]:
    root: Dict[str, Any] = {}
    stack: list[tuple[int, Dict[str, Any]]] = [(-1, root)]
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if "\t" in raw:
            raise ConfigError(f"{path}:{lineno}: tabs are not allowed")
        line = _strip_comment(raw)
        if not line.strip():
            continue
        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)
        if stripped.startswith("-"):
            raise ConfigError(f"{path}:{lineno}: sequences are not supported in build manifest v1")
        if ":" not in stripped:
            raise ConfigError(f"{path}:{lineno}: expected 'key: value'")
        key, raw_value = stripped.split(":", 1)
        key = key.strip()
        if not key:
            raise ConfigError(f"{path}:{lineno}: empty key")
        while stack and indent <= stack[-1][0]:
            stack.pop()
        if not stack:
            raise ConfigError(f"{path}:{lineno}: invalid indentation")
        parent = stack[-1][1]
        if key in parent:
            raise ConfigError(f"{path}:{lineno}: duplicate key: {key}")
        parsed = _parse_scalar(raw_value)
        parent[key] = parsed
        if isinstance(parsed, dict):
            stack.append((indent, parsed))
    return root


def _merge_known(defaults: Mapping[str, Any], overrides: Mapping[str, Any], prefix: str = "") -> Dict[str, Any]:
    unknown = sorted(set(overrides) - set(defaults))
    if unknown:
        raise ConfigError(f"unknown key(s) under {prefix or 'root'}: {', '.join(unknown)}")
    result: Dict[str, Any] = {}
    for key, default_value in defaults.items():
        value = overrides.get(key, default_value)
        path = f"{prefix}.{key}" if prefix else key
        if isinstance(default_value, Mapping):
            if not isinstance(value, Mapping):
                raise ConfigError(f"{path} must be a mapping")
            result[key] = _merge_known(default_value, value, path)
        else:
            result[key] = value
    return result


def resolve_config(raw: Mapping[str, Any]) -> Dict[str, Any]:
    cfg = _merge_known(DEFAULT_CONFIG, raw)
    if cfg["version"] != 1:
        raise ConfigError("version must be 1")
    if cfg["build"]["type"] not in VALID_BUILD_TYPES:
        raise ConfigError(f"build.type must be one of: {', '.join(sorted(VALID_BUILD_TYPES))}")
    if not isinstance(cfg["build"]["dir"], str) or not cfg["build"]["dir"].strip():
        raise ConfigError("build.dir must be a non-empty string")
    parallel = cfg["build"]["parallel"]
    if not isinstance(parallel, int) or isinstance(parallel, bool) or parallel < 0:
        raise ConfigError("build.parallel must be a non-negative integer")
    for section, keys in {
        "components": ["library", "standalone_app", "hakoniwa_app", "monitor"],
        "validation": ["tests", "examples", "integration_tcp"],
    }.items():
        for key in keys:
            if not isinstance(cfg[section][key], bool):
                raise ConfigError(f"{section}.{key} must be true or false")
    for key in ["pdu_endpoint_root", "hakoniwa_core_root", "vcpkg_root"]:
        if not isinstance(cfg["paths"][key], str):
            raise ConfigError(f"paths.{key} must be a string")
    if not cfg["components"]["library"] and any(
        cfg["components"][name] for name in ("standalone_app", "hakoniwa_app", "monitor")
    ):
        raise ConfigError("components.library=false is incompatible with enabled applications/monitor")
    if cfg["validation"]["integration_tcp"] and not cfg["components"]["standalone_app"]:
        raise ConfigError("validation.integration_tcp=true requires components.standalone_app=true")
    return cfg


def _host_platform() -> tuple[str, str]:
    if sys.platform == "win32":
        os_name = "windows"
    elif sys.platform == "darwin":
        os_name = "macos"
    elif sys.platform.startswith("linux"):
        os_name = "linux"
    else:
        os_name = sys.platform
    machine = platform.machine().lower()
    arch = {"amd64": "x64", "x86_64": "x64", "arm64": "arm64", "aarch64": "arm64"}.get(machine, machine or "unknown")
    return os_name, arch


def _candidate_roots(configured: str, env_names: list[str], defaults: list[Path]) -> list[Path]:
    values: list[str] = [configured]
    values.extend(os.environ.get(name, "") for name in env_names)
    values.extend(str(path) for path in defaults)
    result: list[Path] = []
    seen: set[Path] = set()
    for value in values:
        if not value:
            continue
        path = Path(value).expanduser().resolve()
        if path not in seen:
            seen.add(path)
            result.append(path)
    return result


def _find_endpoint_root(cfg: Mapping[str, Any], repo_root: Path) -> Path | None:
    candidates = _candidate_roots(
        cfg["paths"]["pdu_endpoint_root"],
        ["HAKO_PDU_ENDPOINT_ROOT", "HAKO_PDU_ENDPOINT_PREFIX"],
        [repo_root.parent / "hakoniwa-pdu-endpoint", Path("/usr/local/hakoniwa")],
    )
    for root in candidates:
        include_ok = (root / "include" / "hakoniwa" / "pdu" / "endpoint.hpp").exists()
        source_include_ok = (root / "include" / "hakoniwa" / "pdu" / "endpoint.hpp").exists()
        lib_dir = root / "lib"
        bin_dir = root / "bin"
        has_lib = any(lib_dir.glob("*hakoniwa_pdu_endpoint*")) if lib_dir.exists() else False
        has_dll = any(bin_dir.glob("*hakoniwa_pdu_endpoint*")) if bin_dir.exists() else False
        if include_ok and (has_lib or has_dll):
            return root
        if source_include_ok and (root / "CMakeLists.txt").exists():
            # A source checkout is still useful as a user hint, but CMake currently expects an installed prefix.
            continue
    return None


def _find_core_root(cfg: Mapping[str, Any]) -> Path | None:
    candidates = _candidate_roots(
        cfg["paths"]["hakoniwa_core_root"],
        ["HAKONIWA_CORE_ROOT"],
        [Path("/usr/local/hakoniwa")],
    )
    for root in candidates:
        if (root / "include" / "hako_asset.h").exists() or (root / "include" / "hakoniwa" / "hako_asset.h").exists():
            return root
    return None


def _find_vcpkg_root(cfg: Mapping[str, Any], repo_root: Path) -> Path | None:
    candidates = _candidate_roots(
        cfg["paths"]["vcpkg_root"],
        ["VCPKG_ROOT", "VCPKG_INSTALLATION_ROOT"],
        [repo_root.parent / "vcpkg"],
    )
    for root in candidates:
        if (root / "scripts" / "buildsystems" / "vcpkg.cmake").exists():
            return root
    return None


def _find_vswhere() -> Path | None:
    found = shutil.which("vswhere.exe") or shutil.which("vswhere")
    if found:
        return Path(found).resolve()
    program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    candidate = Path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    return candidate if candidate.exists() else None


@dataclass
class BuildContext:
    repo_root: Path
    manifest_path: Path
    cfg: Dict[str, Any]
    platform_name: str
    arch: str
    build_dir: Path
    endpoint_root: Path | None
    core_root: Path | None
    vcpkg_root: Path | None

    @property
    def cmake_args(self) -> list[str]:
        cfg = self.cfg
        args = [
            f"-DCMAKE_BUILD_TYPE={cfg['build']['type']}",
            f"-DHAKO_PDU_BRIDGE_BUILD_STANDALONE_APP={'ON' if cfg['components']['standalone_app'] else 'OFF'}",
            f"-DHAKO_PDU_BRIDGE_BUILD_HAKONIWA_APP={'ON' if cfg['components']['hakoniwa_app'] else 'OFF'}",
            f"-DHAKO_PDU_BRIDGE_BUILD_MONITOR={'ON' if cfg['components']['monitor'] else 'OFF'}",
            f"-DHAKO_PDU_BRIDGE_BUILD_TESTS={'ON' if cfg['validation']['tests'] else 'OFF'}",
            f"-DHAKO_PDU_BRIDGE_BUILD_EXAMPLES={'ON' if cfg['validation']['examples'] else 'OFF'}",
            f"-DHAKO_PDU_BRIDGE_ENABLE_HAKONIWA_CORE={'ON' if cfg['components']['hakoniwa_app'] else 'OFF'}",
        ]
        if self.endpoint_root:
            args.append(f"-DHAKO_PDU_ENDPOINT_PREFIX={self.endpoint_root}")
        if self.core_root:
            args.append(f"-DHAKO_PDU_BRIDGE_HAKONIWA_CORE_ROOT={self.core_root}")
        dependency_prefixes = [
            str(root)
            for root in (self.endpoint_root, self.core_root)
            if root is not None
        ]
        if dependency_prefixes:
            # The Endpoint package uses find_dependency(hakoniwa-core). Passing
            # only the Bridge-specific Core variable is therefore insufficient
            # when Core is installed outside the platform's default prefix.
            args.append(f"-DCMAKE_PREFIX_PATH={';'.join(dependency_prefixes)}")
        if self.vcpkg_root:
            args.append(f"-DCMAKE_TOOLCHAIN_FILE={self.vcpkg_root / 'scripts' / 'buildsystems' / 'vcpkg.cmake'}")
            if self.platform_name == "windows":
                args.append(f"-DVCPKG_TARGET_TRIPLET={self.arch}-windows")
        return args


def create_context(manifest: Path, repo_root: Path) -> BuildContext:
    cfg = resolve_config(load_simple_yaml(manifest))
    platform_name, arch = _host_platform()
    build_dir = Path(cfg["build"]["dir"])
    if not build_dir.is_absolute():
        build_dir = (repo_root / build_dir).resolve()
    return BuildContext(
        repo_root=repo_root,
        manifest_path=manifest,
        cfg=cfg,
        platform_name=platform_name,
        arch=arch,
        build_dir=build_dir,
        endpoint_root=_find_endpoint_root(cfg, repo_root),
        core_root=_find_core_root(cfg) if cfg["components"]["hakoniwa_app"] else None,
        vcpkg_root=_find_vcpkg_root(cfg, repo_root),
    )


def doctor(ctx: BuildContext) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []
    if not shutil.which("cmake"):
        errors.append("CMake was not found on PATH")
    if ctx.platform_name == "windows" and not (_find_vswhere() or shutil.which("cl.exe")):
        errors.append("Visual Studio C++ tools were not found (vswhere.exe/cl.exe unavailable)")
    if not ctx.endpoint_root:
        errors.append(
            "hakoniwa-pdu-endpoint install prefix was not found; set paths.pdu_endpoint_root or HAKO_PDU_ENDPOINT_ROOT"
        )
    if ctx.cfg["components"]["hakoniwa_app"] and not ctx.core_root:
        errors.append(
            "Hakoniwa Core was requested but not found; set paths.hakoniwa_core_root or HAKONIWA_CORE_ROOT"
        )
    if ctx.platform_name == "windows" and not ctx.vcpkg_root:
        warnings.append("vcpkg was not found; set paths.vcpkg_root or VCPKG_ROOT if endpoint dependencies require it")
    return errors, warnings


def _yaml_scalar(value: Any) -> str:
    if value is True:
        return "true"
    if value is False:
        return "false"
    if value is None:
        return "null"
    if isinstance(value, int):
        return str(value)
    return json.dumps(str(value), ensure_ascii=False)


def dump_yaml(data: Mapping[str, Any], indent: int = 0) -> str:
    lines: list[str] = []
    prefix = " " * indent
    for key, value in data.items():
        if isinstance(value, Mapping):
            lines.append(f"{prefix}{key}:")
            lines.append(dump_yaml(value, indent + 2).rstrip())
        elif isinstance(value, list):
            lines.append(f"{prefix}{key}:")
            for item in value:
                lines.append(f"{prefix}  - {_yaml_scalar(item)}")
        else:
            lines.append(f"{prefix}{key}: {_yaml_scalar(value)}")
    return "\n".join(lines) + "\n"


def write_resolved(ctx: BuildContext) -> Path:
    out_dir = ctx.repo_root / ".hako"
    out_dir.mkdir(parents=True, exist_ok=True)
    record = {
        "version": 1,
        "manifest": str(ctx.manifest_path),
        "platform": {"os": ctx.platform_name, "arch": ctx.arch},
        "build": {"type": ctx.cfg["build"]["type"], "dir": str(ctx.build_dir)},
        "components": dict(ctx.cfg["components"]),
        "validation": dict(ctx.cfg["validation"]),
        "resolved_paths": {
            "pdu_endpoint_root": str(ctx.endpoint_root) if ctx.endpoint_root else "",
            "hakoniwa_core_root": str(ctx.core_root) if ctx.core_root else "",
            "vcpkg_root": str(ctx.vcpkg_root) if ctx.vcpkg_root else "",
        },
        "cmake_args": ctx.cmake_args,
    }
    out_path = out_dir / "resolved-build.yaml"
    out_path.write_text(dump_yaml(record), encoding="utf-8")
    (out_dir / "cmake-args.txt").write_text("\n".join(ctx.cmake_args) + "\n", encoding="utf-8")
    return out_path


def print_summary(ctx: BuildContext, errors: list[str], warnings: list[str]) -> None:
    print("Hakoniwa PDU Bridge build configuration")
    print(f"  Platform       : {ctx.platform_name}-{ctx.arch}")
    print(f"  Build type     : {ctx.cfg['build']['type']}")
    print(f"  Build directory: {ctx.build_dir}")
    print(f"  Library        : {'ON' if ctx.cfg['components']['library'] else 'OFF'}")
    print(f"  Standalone app : {'ON' if ctx.cfg['components']['standalone_app'] else 'OFF'}")
    print(f"  Hakoniwa app   : {'ON' if ctx.cfg['components']['hakoniwa_app'] else 'OFF'}")
    print(f"  Monitor        : {'ON' if ctx.cfg['components']['monitor'] else 'OFF'}")
    print(f"  Tests          : {'ON' if ctx.cfg['validation']['tests'] else 'OFF'}")
    print(f"  Examples       : {'ON' if ctx.cfg['validation']['examples'] else 'OFF'}")
    print(f"  Endpoint root  : {ctx.endpoint_root or 'not resolved'}")
    print(f"  Hakoniwa Core  : {ctx.core_root if ctx.cfg['components']['hakoniwa_app'] else 'not requested'}")
    print(f"  vcpkg          : {ctx.vcpkg_root or 'not resolved'}")
    if errors:
        print("\nDoctor errors:")
        for item in errors:
            print(f"  - {item}")
    if warnings:
        print("\nDoctor warnings:")
        for item in warnings:
            print(f"  - {item}")


def _run(command: list[str], *, cwd: Path) -> None:
    print(">", " ".join(command))
    subprocess.run(command, cwd=cwd, check=True)


def configure(ctx: BuildContext) -> None:
    ctx.build_dir.mkdir(parents=True, exist_ok=True)
    _run(["cmake", "-S", str(ctx.repo_root), "-B", str(ctx.build_dir), *ctx.cmake_args], cwd=ctx.repo_root)


def build(ctx: BuildContext) -> None:
    configure(ctx)
    command = ["cmake", "--build", str(ctx.build_dir), "--config", ctx.cfg["build"]["type"]]
    if ctx.cfg["build"]["parallel"]:
        command += ["--parallel", str(ctx.cfg["build"]["parallel"])]
    _run(command, cwd=ctx.repo_root)


def test(ctx: BuildContext) -> None:
    if ctx.cfg["validation"]["tests"]:
        _run(
            ["ctest", "--test-dir", str(ctx.build_dir), "-C", ctx.cfg["build"]["type"], "--output-on-failure"],
            cwd=ctx.repo_root,
        )
    if ctx.cfg["validation"]["integration_tcp"]:
        raise ConfigError("validation.integration_tcp is reserved for the cross-platform TCP smoke test and is not implemented yet")


def _command_output(command: list[str], cwd: Path) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip() if result.returncode == 0 else "unknown"


def _cmake_cache_value(build_dir: Path, key: str) -> str:
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file():
        return "unknown"
    prefix = f"{key}:"
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(prefix) and "=" in line:
            return line.split("=", 1)[1] or "unknown"
    return "unknown"


def _read_dependency_receipt(prefix: Path, component_id: str) -> Dict[str, Any]:
    path = prefix / "share" / "hakoniwa" / "receipts" / f"{component_id}.yaml"
    if not path.is_file():
        return {
            "version": "unknown",
            "source_revision": "unknown",
            "build_limits": {},
        }
    result: Dict[str, Any] = {"build_limits": {}}
    section = ""
    for raw in path.read_text(encoding="utf-8").splitlines():
        if raw and not raw.startswith(" ") and raw.endswith(":"):
            section = raw[:-1]
            continue
        if not raw.startswith("  ") or raw.startswith("    ") or ":" not in raw:
            continue
        key, value = raw.strip().split(":", 1)
        parsed = _parse_scalar(value)
        if section == "component" and key in {"version", "source_revision"}:
            result[key] = parsed
        elif section == "build_limits":
            result["build_limits"][key] = parsed
    if not result.get("version") or not result.get("source_revision"):
        raise ConfigError(f"incomplete dependency receipt: {path}")
    return result


def _bridge_artifacts(install_dir: Path) -> list[tuple[Path, str]]:
    artifacts: list[tuple[Path, str]] = []
    fixed = (
        (Path("lib/cmake/hakoniwa_pdu_bridge"), "cmake-package"),
        (
            Path("share/hakoniwa-pdu-bridge/config/web_bridge_fleets"),
            "config-format",
        ),
    )
    for relative, kind in fixed:
        if (install_dir / relative).exists():
            artifacts.append((relative, kind))
    for child in ("bin", "lib"):
        parent = install_dir / child
        if not parent.is_dir():
            continue
        for installed in parent.iterdir():
            if installed.is_file() and (
                "hakoniwa-pdu-bridge" in installed.name
                or (
                    installed.name.startswith("hakoniwa-pdu-")
                    and "bridge" in installed.name
                )
                or "hakoniwa_pdu_bridge" in installed.name
                or installed.name == "run-web-bridge.bash"
            ):
                kind = "executable" if child == "bin" else "library"
                artifacts.append((installed.relative_to(install_dir), kind))
    return sorted(set(artifacts), key=lambda item: item[0].as_posix())


def write_receipt(ctx: BuildContext, install_dir: Path) -> Path:
    receipt_root = install_dir / "share" / "hakoniwa" / "receipts"
    resolved_relative = (
        Path("share")
        / "hakoniwa"
        / "receipts"
        / "resolved"
        / "hakoniwa-pdu-bridge-core.yaml"
    )
    (install_dir / resolved_relative).parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(
        ctx.repo_root / ".hako" / "resolved-build.yaml",
        install_dir / resolved_relative,
    )

    artifacts = _bridge_artifacts(install_dir)
    if not any(kind == "cmake-package" for _, kind in artifacts):
        raise ConfigError(f"installed Bridge CMake package not found under: {install_dir}")

    dependencies: Dict[str, Dict[str, Any]] = {}
    if ctx.endpoint_root:
        dependencies["hakoniwa-pdu-endpoint"] = _read_dependency_receipt(
            ctx.endpoint_root,
            "hakoniwa-pdu-endpoint",
        )
    if ctx.core_root:
        dependencies["hakoniwa-core-pro"] = _read_dependency_receipt(
            ctx.core_root,
            "hakoniwa-core-pro",
        )
    build_limits = (
        dependencies.get("hakoniwa-core-pro", {}).get("build_limits", {})
        or dependencies.get("hakoniwa-pdu-endpoint", {}).get("build_limits", {})
    )
    compiler = _cmake_cache_value(ctx.build_dir, "CMAKE_CXX_COMPILER")
    revision = _command_output(["git", "rev-parse", "HEAD"], ctx.repo_root)
    capabilities = {
        "library": ctx.cfg["components"]["library"],
        "standalone_app": ctx.cfg["components"]["standalone_app"],
        "hakoniwa_app": ctx.cfg["components"]["hakoniwa_app"],
        "web_bridge": ctx.cfg["components"]["hakoniwa_app"],
        "monitor": ctx.cfg["components"]["monitor"],
        "web_bridge_fleets_config_format": True,
        "cmake_package": True,
    }
    lines = [
        "schema_version: 1",
        "component:",
        "  id: hakoniwa-pdu-bridge-core",
        "  version: 1.0.0",
        f"  source_revision: {_yaml_scalar(revision)}",
        "platform:",
        f"  os: {_yaml_scalar(ctx.platform_name)}",
        f"  architecture: {_yaml_scalar(ctx.arch)}",
        f"  toolchain: {_yaml_scalar(compiler)}",
        "install:",
        f"  prefix: {_yaml_scalar(install_dir)}",
        "capabilities:",
    ]
    for key, value in capabilities.items():
        lines.append(f"  {key}: {_yaml_scalar(value)}")
    if build_limits:
        lines.append("build_limits:")
        for key, value in build_limits.items():
            lines.append(f"  {key}: {_yaml_scalar(value)}")
    else:
        lines.append("build_limits: {}")
    if dependencies:
        lines.append("dependencies:")
        for component_id, dependency in dependencies.items():
            lines.extend(
                [
                    f"  {component_id}:",
                    f"    version: {_yaml_scalar(dependency['version'])}",
                    f"    source_revision: {_yaml_scalar(dependency['source_revision'])}",
                ]
            )
            dependency_limits = dependency["build_limits"]
            if dependency_limits:
                lines.append("    build_limits:")
                for key, value in dependency_limits.items():
                    lines.append(f"      {key}: {_yaml_scalar(value)}")
            else:
                lines.append("    build_limits: {}")
    else:
        lines.append("dependencies: {}")
    lines.append("artifacts:")
    for path, kind in artifacts:
        lines.extend(
            [
                f"  - path: {_yaml_scalar(path.as_posix())}",
                f"    kind: {kind}",
            ]
        )
    lines.append(f"resolved_manifest: {_yaml_scalar(resolved_relative.as_posix())}")
    receipt_path = receipt_root / "hakoniwa-pdu-bridge-core.yaml"
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    receipt_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return receipt_path


def install(ctx: BuildContext, install_dir: Path) -> None:
    if not (ctx.build_dir / "CMakeCache.txt").is_file():
        raise ConfigError(
            f"configured build tree not found: {ctx.build_dir}; run hako.py build first"
        )
    command = ["cmake", "--install", str(ctx.build_dir), "--prefix", str(install_dir)]
    if ctx.platform_name == "windows":
        command.extend(["--config", ctx.cfg["build"]["type"]])
    _run(command, cwd=ctx.repo_root)
    receipt = write_receipt(ctx, install_dir)
    print(f"Component Receipt: {receipt}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="OS-independent Hakoniwa PDU Bridge build configurator")
    parser.add_argument("command", choices=["doctor", "configure", "build", "test", "install"])
    parser.add_argument("--config", default=None, help="build manifest (default: repository root/hakoniwa-build.yaml)")
    parser.add_argument("--install-dir", default=None, help="explicit local install prefix (required by install)")
    parser.add_argument("--dry-run", action="store_true", help="resolve and print without running build commands")
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parents[1]
    manifest = repo_root / "hakoniwa-build.yaml" if args.config is None else Path(args.config)
    if args.config is not None and not manifest.is_absolute():
        manifest = (Path.cwd() / manifest).resolve()
    if not manifest.exists():
        raise ConfigError(f"build manifest not found: {manifest}")

    ctx = create_context(manifest, repo_root)
    errors, warnings = doctor(ctx)
    print_summary(ctx, errors, warnings)
    resolved = write_resolved(ctx)
    print(f"\nResolved configuration: {resolved}")

    if args.command == "doctor":
        return 1 if errors else 0
    if args.command in {"build", "test", "install"} and errors:
        raise ConfigError("doctor found blocking prerequisites; fix them before building/testing")
    if args.command == "configure" and not shutil.which("cmake"):
        raise ConfigError("CMake was not found on PATH")
    if args.dry_run:
        return 0
    if args.command == "configure":
        configure(ctx)
    elif args.command == "build":
        build(ctx)
    elif args.command == "test":
        test(ctx)
    elif args.command == "install":
        if not args.install_dir:
            raise ConfigError("install requires --install-dir")
        install_dir = Path(args.install_dir)
        if not install_dir.is_absolute():
            install_dir = (Path.cwd() / install_dir).resolve()
        install(ctx, install_dir)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ConfigError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(2)
    except subprocess.CalledProcessError as exc:
        print(f"ERROR: command failed with exit code {exc.returncode}", file=sys.stderr)
        raise SystemExit(exc.returncode or 1)
