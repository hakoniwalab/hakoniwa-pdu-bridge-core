#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path


def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        print(f"ERROR: file not found: {path}")
        return None
    except json.JSONDecodeError as exc:
        print(f"ERROR: invalid JSON: {path}: {exc}")
        return None


def validate_schema(instance, schema_path: Path) -> bool:
    try:
        import jsonschema  # type: ignore
    except Exception:
        print("ERROR: jsonschema is not available. Install with: pip install jsonschema")
        return False

    schema = load_json(schema_path)
    if schema is None:
        return False
    try:
        jsonschema.validate(instance=instance, schema=schema)
        return True
    except jsonschema.ValidationError as exc:
        print(f"ERROR: schema validation failed: {exc.message}")
        return False


def check_bridge_paths(bridge_path: Path, config: dict) -> bool:
    ok = True
    base_dir = bridge_path.parent
    endpoints = config.get("endpoints", [])
    for node in endpoints:
        for ep in node.get("endpoints", []):
            config_path = ep.get("config_path")
            if not config_path:
                continue
            resolved = (base_dir / config_path).resolve()
            if not resolved.is_file():
                print(f"ERROR: endpoint config_path not found: {config_path} (resolved: {resolved})")
                ok = False
    return ok


def check_endpoint_container_paths(container_path: Path) -> bool:
    ok = True
    base_dir = container_path.parent
    data = load_json(container_path)
    if data is None:
        return False
    if not isinstance(data, list):
        print("ERROR: endpoint_container.json must be a list")
        return False
    for node in data:
        endpoints = node.get("endpoints", []) if isinstance(node, dict) else []
        for ep in endpoints:
            config_path = ep.get("config_path") if isinstance(ep, dict) else None
            if not config_path:
                continue
            resolved = (base_dir / config_path).resolve()
            if not resolved.is_file():
                print(f"ERROR: endpoint_container config_path not found: {config_path} (resolved: {resolved})")
                ok = False
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate bridge.json with schema and check config paths.")
    parser.add_argument("bridge_json", type=Path, help="Path to bridge.json")
    parser.add_argument(
        "--schema",
        type=Path,
        default=Path("config/schema/bridge-schema.json"),
        help="Path to bridge JSON schema",
    )
    parser.add_argument(
        "--endpoint-container",
        type=Path,
        default=None,
        help="Optional endpoint_container.json for config_path checks",
    )
    args = parser.parse_args()

    bridge_data = load_json(args.bridge_json)
    if bridge_data is None:
        return 1

    ok = True
    if not validate_schema(bridge_data, args.schema):
        ok = False

    if not check_bridge_paths(args.bridge_json, bridge_data):
        ok = False

    endpoint_container_path = args.endpoint_container
    if endpoint_container_path is None:
        endpoints_config_path = bridge_data.get("endpoints_config_path")
        if isinstance(endpoints_config_path, str) and endpoints_config_path:
            endpoint_container_path = (args.bridge_json.parent / endpoints_config_path).resolve()
            if not endpoint_container_path.is_file():
                print(f"ERROR: endpoints_config_path not found: {endpoints_config_path} (resolved: {endpoint_container_path})")
                ok = False
                endpoint_container_path = None

    if endpoint_container_path:
        if not check_endpoint_container_paths(endpoint_container_path):
            ok = False

    if ok:
        print("OK: schema and path checks passed")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
