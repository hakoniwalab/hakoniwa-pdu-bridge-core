# hakoniwa-pdu-bridge

`hakoniwa-pdu-bridge`は、PDU（Protocol Data Unit）チャネル間のデータフローを **時間的な観点から制御する** ことに特化した、論理的な転送コンポーネントです。

本リポジトリの核心は、**「いつ・どのデータを送るか」という転送ポリシー** と **「どう送るか」という通信プロトコル（TCP/UDP/SHM等）** を **意図的に分離** する設計思想にあります。

これにより、開発者は通信の詳細を意識することなく、シミュレーションの要求に応じて「即時同期」「定周期での状態送信」「帯域を考慮した間引き」といった時間モデルを自由に組み合わせ、システム全体のデータフローを宣言的に設計できます。

---

**これは通信ライブラリではありません**

`hakoniwa-pdu-bridge` は、TCP/UDP/WebSocket/Zenoh などの
通信方式そのものを提供したり、抽象化したりするコンポーネントではありません。

本ブリッジが扱うのは、
- どのPDUが
- どの関係に基づいて
- **どの時間モデルで転送されるか**

という「論理的なデータフローの定義」のみです。

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
以下の設定例は、「node1 で生成された PDU を、時間モデル immediate で node2 に流す」という**論理的な配線図**を JSON として記述したものです。
### 最小構成（immediate）

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
  "endpoints": [
    {
      "nodeId": "node1",
      "endpoints": [
        { "id": "n1-src", "mode": "local" },
        { "id": "n1-dst", "mode": "wire" }
      ]
    },
    {
      "nodeId": "node2",
      "endpoints": [
        { "id": "n2-src", "mode": "wire" },
        { "id": "n2-dst", "mode": "local" }
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
  "policy": {
    "type": "throttle",
    "intervalMs": 100
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
  "policy": {
    "type": "ticker",
    "intervalMs": 50
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

- **通信方式からの独立**: 転送ポリシーは、あくまで「いつ・どのデータを流すか」という時間的判断のみを担います。実際のデータグラムをどう構築し、どのプロトコル（TCP/UDP/Zenoh等）で送出するかは、`hakoniwa-pdu-endpoint` をはじめとする通信層の責務です。この関心の分離が、本ブリッジの設計の根幹です。
- **動的なフロー変更との整合性**: `src` / `dst` の切り替えは、シミュレーションのフェーズに応じて動的に行われることを想定しています（例: Runtime Delegation）。本ブリッジの設計は、こうした動的な構成変更時にも、各データフローの時間モデルが一貫して適用されることを保証します。
---

## 転送設定 (Bridge Configuration)

`hakoniwa-pdu-bridge` は、`bridge.json` という設定ファイルを用いて転送の振る舞いを定義します。この設定ファイルは `config/schema/bridge-schema.json` で定義されたJSONスキーマに準拠している必要があります。

設定ファイルは、以下のトップレベルのプロパティを **すべて** 含む必要があります。

-   **`version`**: (必須) 設定ファイルのバージョン。現在は `"2.0.0"` です。
-   **`transferPolicies`**: (必須) `immediate`, `throttle`, `ticker` などの転送ポリシーを、ユニークなID（キー）と共に定義します。
-   **`nodes`**: (必須) ブリッジに関与するノードのリスト。
-   **`endpoints`**: (必須) 各ノードの通信エンドポイントを定義します。エンドポイントは `local` (ノード内) または `wire` (外部通信用) のモードを持ちます。
-   **`wireLinks`**: (任意) 異なるノード間の `wire` エンドポイント接続を定義します。
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