from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("hako.py")
SPEC = importlib.util.spec_from_file_location("hako_bridge_tool", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
HAKO = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = HAKO
SPEC.loader.exec_module(HAKO)


class FoundationInstallTests(unittest.TestCase):
    def test_dependency_receipt_reads_contract_fields(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            prefix = Path(temp_dir)
            receipt = (
                prefix
                / "share"
                / "hakoniwa"
                / "receipts"
                / "hakoniwa-pdu-endpoint.yaml"
            )
            receipt.parent.mkdir(parents=True)
            receipt.write_text(
                """schema_version: 1
component:
  id: hakoniwa-pdu-endpoint
  version: 1.0.0
  source_revision: "def456"
build_limits:
  asset_num: 16
artifacts:
  - path: "lib/libhakoniwa_pdu_endpoint.so"
    kind: library
""",
                encoding="utf-8",
            )

            dependency = HAKO._read_dependency_receipt(
                prefix,
                "hakoniwa-pdu-endpoint",
            )

            self.assertEqual(dependency["version"], "1.0.0")
            self.assertEqual(dependency["source_revision"], "def456")
            self.assertEqual(dependency["build_limits"]["asset_num"], 16)

    def test_bridge_artifacts_include_runtime_and_package_surfaces(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            prefix = Path(temp_dir)
            cmake_dir = prefix / "lib" / "cmake" / "hakoniwa_pdu_bridge"
            cmake_dir.mkdir(parents=True)
            config_dir = (
                prefix
                / "share"
                / "hakoniwa-pdu-bridge"
                / "config"
                / "web_bridge_fleets"
            )
            config_dir.mkdir(parents=True)
            (prefix / "bin").mkdir()
            (prefix / "bin" / "hakoniwa-pdu-web-bridge").write_text(
                "",
                encoding="utf-8",
            )

            artifacts = HAKO._bridge_artifacts(prefix)

            self.assertIn(
                (Path("bin/hakoniwa-pdu-web-bridge"), "executable"),
                artifacts,
            )
            self.assertIn(
                (Path("lib/cmake/hakoniwa_pdu_bridge"), "cmake-package"),
                artifacts,
            )


if __name__ == "__main__":
    unittest.main()
