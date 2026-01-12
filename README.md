# hakoniwa-pdu-bridge

`hakoniwa-pdu-bridge` は、PDU（Protocol Data Unit）チャネル間のデータフローを **時間的な観点から制御する** ことに特化した、論理的な転送コンポーネントです。

本リポジトリの核心は、**「いつ・どのデータを送るか」という転送ポリシー** と **「どう送るか」という通信プロトコル（TCP/UDP/SHM等）** を **意図的に分離** する設計思想にあります。

このブリッジは転送の判断だけを行い、データの送受は `hakoniwa-pdu-endpoint` 側に委譲します。

---

## What this is / isn't

**This is:**
- PDUの「論理フロー」を宣言するための転送レイヤ
- どのPDUを、どこへ、**どの時間モデルで**流すかを定義する

**This is NOT:**
- 通信プロトコル実装（TCP/UDP/WebSocket/Zenoh/SHMなど）
- 到達保証、リトライ、永続キュー
- Endpoint JSON をロードする仕組み（endpoint loader は `hakoniwa-pdu-endpoint` の責務）

---

## アーキテクチャ

### 主要コンポーネント

- **BridgeDaemon**: `main()` で `BridgeCore` を構築し、ループ実行する起点。
- **BridgeCore**: `BridgeConnection` を保持し、`cyclic_trigger()` を回して転送を駆動。
- **BridgeConnection**: 1つの `source` と複数の `destinations` を束ね、`TransferPdu` を保持。
- **TransferPdu / TransferAtomicPduGroup**: 単一PDUまたはPDUグループの転送を行う。
- **Policy**: `immediate` / `throttle` / `ticker` の時間モデルを提供。
- **TimeSource**: `real` / `virtual` / `hakoniwa` の時間基準を提供。
- **EndpointContainer**: endpoint を生成・管理（`hakoniwa-pdu-endpoint` 側）。

### データフロー（概略）

1. `BridgeDaemon` が `bridge.json` を読み込み、`BridgeCore` を組み立てる。
2. `BridgeCore` が `BridgeConnection` を通じて `TransferPdu` を管理する。
3. `TransferPdu` が policy 判定により、src endpoint から dst endpoint へ転送する。

---

## ビルド

### 依存関係

- C++20 対応コンパイラ (GCC, Clangなど)
- CMake 3.16以上
- Hakoniwa コアライブラリ ( `/usr/local/hakoniwa` にインストール済み)
- `hakoniwa-pdu-endpoint` サブモジュール

### 手順

```bash
# 1. submodule 初期化
git submodule update --init --recursive

# 2. out-of-source ビルド
cmake -S . -B build
cmake --build build
```

`build/hakoniwa-pdu-bridge` が生成されます。

### Hakoniwa コアのインストール注意

- ヘッダ: `/usr/local/hakoniwa/include/hakoniwa`
- ライブラリ: `/usr/local/hakoniwa/lib`

実行時に共有ライブラリが見つからない場合は、`LD_LIBRARY_PATH`（Linux）や `DYLD_LIBRARY_PATH`（macOS）に追加してください。

---

## 実行

`hakoniwa-pdu-bridge` は以下の引数を取ります。

```bash
./build/hakoniwa-pdu-bridge <bridge.json> <delta_time_step_usec> <endpoint_container.json> [node_name]
```

- `bridge.json`: 本リポジトリが読む設定ファイル
- `delta_time_step_usec`: タイムソースの刻み幅（マイクロ秒）
- `endpoint_container.json`: endpoint loader が読む設定ファイル（`hakoniwa-pdu-endpoint` 側の仕様）
- `node_name`: 任意。省略時は `node1`

### 実行例（単一ノード・ローカル）

```bash
./build/hakoniwa-pdu-bridge \
  config/sample/simple-bridge.json \
  1000 \
  test/config/core_flow/endpoints.json \
  node1
```

### 実行例（2ノード・TCP構成）

以下はテスト用設定を参照する例です。

```bash
# node1 側
./build/hakoniwa-pdu-bridge \
  test/config/tcp/bridge.json \
  1000 \
  test/config/tcp/endpoints.json \
  node1

# node2 側
./build/hakoniwa-pdu-bridge \
  test/config/tcp/bridge.json \
  1000 \
  test/config/tcp/endpoints.json \
  node2
```

---

## テスト

GTest を使用します。ビルド後に `ctest` を実行してください。

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

必要に応じて `HAKO_TEST_CONFIG_DIR` でテスト用設定のルートを変更できます。

```bash
HAKO_TEST_CONFIG_DIR=/path/to/test/config ctest --test-dir build
```

---

## 転送設定 (Bridge Configuration)

`bridge.json` は `config/schema/bridge-schema.json` に準拠します。

必須トップレベル項目:
- `version` (現在は `2.0.0`)
- `time_source_type` (`real` / `virtual` / `hakoniwa`)
- `transferPolicies`
- `nodes`
- `endpoints`
- `wireLinks`
- `pduKeyGroups`
- `connections`

主な制約:
- IDは `^[A-Za-z][A-Za-z0-9_\-\.]*$`
- `throttle` と `ticker` は `intervalMs` 必須
- `immediate` は `intervalMs` を持てない

`endpoints` 内の `config_path` は endpoint loader に渡す **参照パス** であり、本リポジトリでは読み込みません。

### スキーマ検証

任意の JSON Schema バリデータで検証できます。例（`ajv` がある場合）:

```bash
ajv validate -s config/schema/bridge-schema.json -d bridge.json
```

---

## Endpoint Container 設定

`endpoint_container.json` は `hakoniwa-pdu-endpoint` が読み込む **EndpointContainer の設定** です。形式は配列で、`nodeId` ごとに endpoint をまとめます。

例（`test/config/core_flow/endpoints.json`）:

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

`config_path` は **このファイルの場所からの相対パス** で解決されます。

---

## 転送ポリシー（概要）

- **immediate**: 更新と同時に転送する。
- **throttle**: 更新は追従するが、最小間隔を満たした時だけ転送する。
- **ticker**: 周期ごとに最新値を転送する（更新がなくても送る）。

---

## 転送ポリシー詳細

### immediate

更新された瞬間に転送します。低遅延・同期用途向け。

### throttle

更新イベントは受けるが、前回転送から `intervalMs` 以上経過した場合のみ転送します。

### ticker

一定周期で最新値を転送します。更新がなくても送られます。

### atomic immediate（フレーム単位の即時転送）

`immediate` に `atomic: true` を指定すると、同一 `transferPdus` 内の PDU 群を1フレームとして扱います。

- 全対象PDUの更新が揃った時点でフレーム転送
- ブリッジが観測した時刻 `T_frame` を暗黙的に扱う
- 各PDUの生成時刻の厳密一致は保証しない

**重要:** `atomic: true` を使う場合は、箱庭時刻通知用の `hako_msgs/SimTime` を必ず含めてください。

---

## 時間ソース (Time Source)

`time_source_type` は `throttle`/`ticker` の時間基準となる重要な設定です。

- `real`: システムの壁時計時間
- `virtual`: 外部提供の仮想時間
- `hakoniwa`: Hakoniwa コア時間と同期

---

## Runtime Delegation（epoch の扱い）

owner 切替の瞬間に旧/新が同時送信する可能性があるため、受信側は **最新 epoch 以外を捨てる** ことが前提です。ポリシーには混ぜず、`TransferPdu` 側で判定する設計です。

---

## 最小構成例

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
  "endpoints": [
    {
      "nodeId": "node1",
      "endpoints": [
        { "id": "n1-src", "mode": "local", "config_path": "config/sample/endpoint/n1-epSrc.json", "direction": "out" },
        { "id": "n1-dst", "mode": "wire",  "config_path": "config/sample/endpoint/n1-epDst.json", "direction": "in" }
      ]
    },
    {
      "nodeId": "node2",
      "endpoints": [
        { "id": "n2-src", "mode": "wire",  "config_path": "config/sample/endpoint/n2-epSrc.json", "direction": "in" },
        { "id": "n2-dst", "mode": "local", "config_path": "config/sample/endpoint/n2-epDst.json", "direction": "out" }
      ]
    }
  ],
  "wireLinks": [
    { "from": "n1-dst", "to": "n2-src" }
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
