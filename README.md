# hakoniwa-pdu-bridge

`hakoniwa-pdu-bridge`は、PDU（Protocol Data Unit）チャネル間のデータフローを **時間的な観点から制御する** ことに特化した、論理的な転送コンポーネントです。

本リポジトリの核心は、**「いつ・どのデータを送るか」という転送ポリシー** と **「どう送るか」という通信プロトコル（TCP/UDP/SHM等）** を **意図的に分離** する設計思想にあります。

これにより、開発者は通信の詳細を意識することなく、シミュレーションの要求に応じて「即時同期」「定周期での状態送信」「帯域を考慮した間引き」といった時間モデルを自由に組み合わせ、システム全体のデータフローを宣言的に設計できます。

---

## What this is / isn't

**This is:**
- PDUの「論理フロー」を宣言するための転送レイヤ
- どのPDUを、どこへ、**どの時間モデルで**流すかを定義する

**This is NOT:**
- 通信プロトコル実装（TCP/UDP/WebSocket/Zenoh/SHMなど）
- 到達保証、リトライ、永続キューを提供する仕組み
- ランタイム状態（epoch/activeなど）を設定に持ち込む仕組み

転送失敗時の挙動は、使用するtransportの性質に依存します。

---

## Quick Start (mental model)

この設定は **配線図** です。

- `endpoints` は端点（local / wire）
- `wireLinks` は wire端点同士のリンク
- `connections` は node内の「source → destinations」
- `transferPdus` が「何を流すか（PDUグループ）＋いつ流すか（policy）」

bridgeは「転送の判断」だけを行い、実データの送受はendpoint層に委譲します。
そのため、同じ `bridge.json` を使ったまま transport を差し替えても、時間モデルは変わりません。

**試してみる:**
下の `最小構成（immediate）` をコピーして `bridge.json` に保存し、
`hakoniwa-pdu-bridge --config bridge.json` で起動してください。

---

## ビルドと実行

本プロジェクトのビルドには `cmake` が必要です。

### 依存関係

- C++20 対応コンパイラ (GCC, Clangなど)
- CMake (3.16以上)
- Hakoniwaコアライブラリ ( `/usr/local/hakoniwa` にインストールされている必要があります)

### ビルド手順

標準的な out-of-source ビルドを推奨します。

```bash
# 1. git submoduleを初期化・更新
git submodule update --init --recursive

# 2. ビルドディレクトリを作成
mkdir build
cd build

# 3. CMakeでビルドファイルを生成
cmake ..

# 4. ビルドを実行
make
```

ビルドが成功すると、`build` ディレクトリ内に `hakoniwa-pdu-bridge` という実行ファイルが生成されます。

### 実行

プロジェクトのルートディレクトリから、以下のように実行します。

```bash
./build/hakoniwa-pdu-bridge config/sample/simple-bridge.json
```

実行すると、設定ファイルに従ってPDU転送のデバッグメッセージがコンソールに表示されます。
プログラムを停止するには `Ctrl+C` を押してください。

---

## 転送ポリシー

PDUの転送タイミングは、現実世界の物理現象や情報伝達の性質を模擬する上で極めて重要です。本ブリッジでは、その性質を **3つの時間モデル** として抽象化し、転送ポリシーとして提供します。

これらは単なる「機能」ではなく、データが持つべき時間的制約を表現するための設計の核です。

---

### immediate（即時反応）

更新したら必ず転送する。

- **定義**
  - PDUチャネルが更新／受信された瞬間に、可能な限り即座に転送する。
- **トリガー**
  - PDUチャネルの更新／受信タイミング
- **転送タイミング**
  - 更新／受信されたそのデータを即座に転送

主に、制御入力・状態同期など「遅延を許容しないデータ」に使用します。

このポリシーは「転送方法」ではなく、**PDUが従うべき時間的な振る舞い**を定義します。

#### Frame-based Immediate Transfer（atomic immediate）

`immediate` ポリシーは、通常、PDUが更新された瞬間に個別に転送されます。
一方で、制御や同期用途では、複数のPDUを「同一時刻の観測された状態」として
一括で扱いたいケースがあります。

この要件に対応するため、`hakoniwa-pdu-bridge` では
`immediate` ポリシーに対して `atomic` オプションを提供します。

```json
{
  "type": "immediate",
  "atomic": true
}

**atomic: true の意味**

atomic: true が指定された場合、以下の振る舞いが定義されます。

- 同一の transferPdus に属する PDU 群は1つのフレーム（Frame） として扱われます
- フレームには、ブリッジが「全対象 PDU の更新を観測した時点」 の箱庭時刻T_frame が付与されます
- 各 PDU の生成時刻（アセット側の更新時刻）は完全には一致しない可能性があります

ただし、

- 各 PDU の時刻ずれは箱庭の時間同期機構が保証する最大遅延内 に収まることが前提です
- T_frame は「ブリッジ自身が観測した時刻」 であり、各 PDU の生成時刻そのものではありません

**設計上の意図**

この設計は、以下を目的としています。

- PDU 自体に時刻フィールドを 埋め込まない
- にもかかわらず、
  - 箱庭時刻に基づく
  - 一貫したフレーム同期
  - 制御・判断に十分な時間的整合性

をていきょうすることです。

言い換えると：

> 複数の PDU A, B, C があり、
> それらが同一の transferPdus に属している場合、
> 「A, B, C は完全に同時ではないが、
> このフレームは T_frame 時点の観測結果である」

という意味論を、明示的に定義しています。

**時刻PDUの扱いについて（重要）**

atomic: true を使用する場合、transferPdus には 必ず箱庭時刻を伝達するための PDU を含めてください。

- PDU 型： hako_msgs/SimTime

これにより、
- フレーム単位での PDU 受信
- フレームに付与された T_frame
- その時刻を 転送先ノードの箱庭アセット時刻として反映
といった処理が、受信側で一貫して行えます。

箱庭ブリッジは、
- PDU 群を フレームとしてまとめて転送
- 受信側では フレーム単位で処理
- 最終的に T_frame を転送先ノードの箱庭コアに通知

することを想定しています。

**何が保証され、何が保証されないか**

- 保証されること
  - フレームは、ブリッジが観測した 単一の箱庭時刻 T_frame を持つ
  - 各 PDU の時刻ずれは、箱庭の時間同期設計が許容する範囲内
- 保証されないこと
  - 各 PDU が「完全に同一時刻に生成された」こと
  - フレーム内 PDU の生成順序や厳密な内部タイムスタンプ一致

これは意図的な設計判断であり、
「制御・同期に十分な時間的意味論」 と
「PDU設計の単純さ」 のトレードオフです。

---

### throttle（更新追従を間引く）

更新は追従するが、送信頻度を制限する。

- **定義**
  - PDUチャネルが更新／受信されても、指定された最小間隔を満たした場合のみ転送する。
- **トリガー**
  - PDUチャネルの更新／受信タイミング
- **転送タイミング**
  - 前回転送から一定時間以上経過している場合に、
    最新のデータを転送

高頻度センサやログ用途で、「情報量は落とさず流量を抑えたい」場合に使用します。

このポリシーは「転送方法」ではなく、**PDUが従うべき時間的な振る舞い**を定義します。

---

### ticker（定期転送）

時間になったら、今の状態を送る。

- **定義**
  - 一定周期で、PDUチャネルの現在値を転送する。
- **トリガー**
  - シミュレーション時間（周期タイマ）
- **転送タイミング**
  - 周期に達した時点で、PDUチャネルに存在する最新データを転送
  - ※ PDU更新がなくても転送は行われる

状態配信や可視化用途など、「現在値を定期的に知りたい」場合に使用します。

このポリシーは「転送方法」ではなく、**PDUが従うべき時間的な振る舞い**を定義します。

---

## 時間ソース (Time Source)

転送ポリシー、特に `throttle` や `ticker` が時間間隔を計測する際の基準となる「時間」を定義します。この設定は `bridge.json` のトップレベルで **`time_source_type`** として指定する必須項目です。

- **`real`**: システムの壁時計時間（実時間）を基準にします。ブリッジは現実世界の時間で動作します。
- **`virtual`**: 外部から提供される仮想的な時間を基準にします。主にシミュレーション環境で、特定の時間管理コンポーネントと同期する場合に使用されます。
- **`hakoniwa`**: [Hakoniwaシミュレータ](https://github.com/toppers/hakoniwa-core)のコア時間と同期します。Hakoniwa環境下でのシミュレーション時間と厳密に連携する場合に選択します。

時間ソースの選択は、ブリッジが現実のシステムと連携するのか、あるいは閉じたシミュレーション内で動作するのかを決定する、極めて重要な設定です。

---

以下の設定例は、「node1 で生成された PDU を、時間モデル immediate で node2 に流す」という**論理的な配線図**を JSON として記述したものです。

### 最小構成（immediate）

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
        { "id": "n1-dst", "mode": "wire", "config_path": "config/sample/endpoint/n1-epDst.json", "direction": "in" }
      ]
    },
    {
      "nodeId": "node2",
      "endpoints": [
        { "id": "n2-src", "mode": "wire", "config_path": "config/sample/endpoint/n2-epSrc.json", "direction": "in" },
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

- endpoint は hakoniwa-pdu-endpoint で定義された通信端点設定(JSON)を指します。
- destinations は 1つ以上指定可能で、同一PDUを複数の通信端点へ fan-out できます。

---

### throttle の例

```json
{
  "transferPolicies": {
    "limit_100ms": {
      "type": "throttle",
      "intervalMs": 100
    }
  }
}
```

意味：

* PDUは更新され続けても
* **100ms以上経過したときだけ**
* 最新の値を転送する
* `intervalMs` の指定は必須です（1以上の整数）

---

### ticker の例

```json
{
  "transferPolicies": {
    "periodic_50ms": {
      "type": "ticker",
      "intervalMs": 50
    }
  }
}
```

意味：

* 50ms周期で
* 現在PDUチャネルに存在する最新データを転送
* 更新がなくても送る
* `intervalMs` の指定は必須です（1以上の整数）

---

## 設計上の境界と前提

### 通信方式からの独立

転送ポリシーは、あくまで「いつ・どのデータを流すか」という時間的判断のみを担います。実際のデータグラムをどう構築し、どのプロトコル（TCP/UDP/Zenoh等）で送出するかは、`hakoniwa-pdu-endpoint` をはじめとする通信層の責務です。この関心の分離が、本ブリッジの設計の根幹です。

### 静的な転送定義と動的な実行権限

`bridge.json` は**転送の論理構造を静的に宣言**します。
「どのノードが今データを送る権限を持つか」という
**実行権限の動的な移譲（Runtime Delegation）** は、
本ブリッジの責務では**ありません**。

ただし、本ブリッジの設計は、外部の権限管理機構
（例: Owner Epoch, Consensus Protocol）と組み合わせて
使用されることを**明示的に想定**しています。

つまり：
- ✅ bridge.json は不変（デプロイ時に確定）
- ✅ 実行時には「誰がmaster」だけが動的に変わる
- ✅ 転送ポリシーはどちらのノードでも一貫して適用される

この分離により、設定の再現性と、運用時の柔軟性の両立を実現しています。

---

## 転送設定 (Bridge Configuration)

`hakoniwa-pdu-bridge` は、`bridge.json` という設定ファイルを用いて転送の振る舞いを定義します。この設定ファイルは `config/schema/bridge-schema.json` で定義されたJSONスキーマに準拠している必要があります。

設定ファイルは、以下のトップレベルのプロパティを **すべて** 含む必要があります。

-   **`version`**: (必須) 設定ファイルのバージョン。現在は `"2.0.0"` です。
-   **`time_source_type`**: (必須) 時間の基準を `"real"`, `"virtual"`, `"hakoniwa"` から選択します。詳細は「時間ソース」のセクションを参照してください。
-   **`transferPolicies`**: (必須) `immediate`, `throttle`, `ticker` などの転送ポリシーを、ユニークなID（キー）と共に定義します。
-   **`nodes`**: (必須) ブリッジに関与するノードのリスト。
-   **`endpoints`**: (必須) 各ノードの通信エンドポイントを定義します。エンドポイントは `local` (ノード内) または `wire` (外部通信用) のモードを持ちます。
-   **`wireLinks`**: (必須) 異なるノード間の `wire` エンドポイント接続を定義します。
-   **`pduKeyGroups`**: (必須) 関連するPDUをグループ化し、一括で管理するための仕組みです。
-   **`connections`**: (必須) 転送の中核となる設定です。
    -   `nodeId`: 接続が属するノード。
    -   `source`: データが流れ出す元となるエンドポイント。
    -   `destinations`: データが流れ込む先となるエンドポイントのリスト。
    -   `transferPdus`: どのPDUグループ(`pduKeyGroupId`)にどの転送ポリシー(`policyId`)を適用するかを定義します。

### 主要な制約と推奨事項

-   **IDの命名規則**: 設定ファイル内で使用される各種ID（例: `nodes.id`, `endpoints.id`, `transferPolicies`のキー）は、正規表現 `^[A-Za-z][A-Za-z0-9_\\-\\.]*$` に従う必要があります。つまり、英字で始まり、英数字、アンダースコア(`_`)、ハイフン(`-`)、ドット(`.`)のみ使用できます。
-   **Endpoint IDのグローバルな一意性**: `endpoints.id` は、すべてのノード間でグローバルに一意にすることが強く推奨されます。これにより、設定の可読性と保守性が向上します。
-   **ポリシーパラメータ**:
    -   `throttle` および `ticker` ポリシーでは、`intervalMs` (1以上の整数) の指定が **必須** です。
    -   `immediate` ポリシーでは、`intervalMs` は指定 **できません**。
-   **PDUキーの構造**: `pduKeyGroups` 内の各PDUキーは、`id` (例: `"Robot1.camera"`), `robot_name`, `pdu_name` の3つのフィールドを **すべて** 含む必要があります。

この設定により、PDUが「いつ」「どこへ」転送されるかを柔軟かつ厳密に制御できます。