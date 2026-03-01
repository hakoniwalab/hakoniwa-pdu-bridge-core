# Web Bridge Design

## 1. 設計要求

### 1.1 目的

- `hakoniwa-webserver` をレガシー実装として置き換える
- `hakoniwa-pdu-bridge-core` と `hakoniwa-pdu-endpoint` を使って、SHM と WebSocket 間の PDU 転送を実現する
- Unity クライアントは `hakoniwa-pdu-csharp` の `packet v2` を前提とする
- Web 側へ流す PDU、および Web 側から受ける PDU は `bridge.json` で静的に定義する
- 初期到達点は、現行 `hakoniwa-webserver` と同等の挙動を再現することとする
- 将来的に 100 機体規模へ拡張しても維持管理できる構造にする

### 1.2 非目的

- `hakoniwa-webserver` の動的 `DeclarePduForRead` / `DeclarePduForWrite` モデルを再現しない
- Web 側専用の独自プロトコルを新設しない
- endpoint loader をこのリポジトリで再実装しない
- 箱庭コアの時間管理を bridge 側で独自実装しない

注記:

- 初期目標は現行挙動の再現だが、動的 declare 制御そのものを bridge の本質設計には持ち込まない
- 現行相当の転送対象・転送方向を、静的設定で再現する

### 1.3 前提

- SHM 側は `hakoniwa-pdu-endpoint` の SHM endpoint を使う
- WebSocket 側は `hakoniwa-pdu-endpoint` の WebSocket endpoint を使う
- Unity 側の packet format は `v2`
- WebSocket 接続先は `hakoniwa-pdu-endpoint` / `hakoniwa-pdu-bridge-core` 構成へ寄せる
- 初期実装では bridge の転送 policy は `ticker` を採用する
- ただし PDU ごと、または PDU group ごとに policy を切り替えられる設計を前提とする
- config は sample 扱いにせず、専用ディレクトリ配下で厳密管理する
- 機体数増加に備え、config は人手編集前提に固定しない

### 1.4 箱庭コア callback / polling の意味

本設計でいう SHM の `callback` / `poll` は、単なる comm 実装差分ではない。
これは箱庭コア機能のどの asset API モデルに依存するかを意味する。

- `callback`
  - `work/hakoniwa-core-pro/sources/assets/callback` 系を使う
  - 箱庭側が asset の実行進行と時間管理を担う
  - 利用者は時間管理を自前で回す必要がない
  - ユーザビリティ重視
- `poll`
  - `work/hakoniwa-core-pro/sources/assets/polling` 系を使う
  - 利用者が event / simtime を明示的に進める
  - 時間管理を自前で持つ必要がある
  - 柔軟だが上級者向け

### 1.5 本設計の採用方針

- SHM 側は `callback` を採用する
- 理由は、Web bridge 側で箱庭時間管理を持たないため
- ただし PDU 転送のタイミング制御は bridge 側 policy に委譲する
- 初期実装では `ticker` を使う
- 将来的に command 系など一部 group は event 駆動へ切り替える可能性を残す

したがって、本設計は次の分担を採る。

- asset runtime model: callback
- transfer trigger model: policy-driven
- initial policy: ticker


## 2. 設計方針

### 2.1 全体方針

- 箱庭コアとの統合は `callback` モデルに寄せる
- WebSocket 通信は `hakoniwa-pdu-endpoint` の既存 WebSocket comm を使う
- Unity との wire format は `packet v2` で統一する
- どの PDU をどちら向きに流すかは `bridge.json` で固定する
- `bridge.json` は将来の policy 差し替えを前提に、PDU group 単位で管理する
- config は sample ではなく運用対象として管理し、用途別ディレクトリを明示する
- 機体数が増えても同じ規則で展開できる naming / directory / group 設計を採る

### 2.2 転送モデル

- SHM -> WebSocket
  - 箱庭 SHM 上の PDU を ticker 周期で読み出し、WebSocket endpoint へ送る
- WebSocket -> SHM
  - Unity から受信した PDU を ticker 周期で SHM endpoint から読み出し、箱庭 SHM 側へ書く

ここでの「読み出し」は endpoint cache からの取得であり、SHM data receive event は転送トリガとして使用しない。

注記:

- 上記は初期実装の挙動である
- 将来的には PDU group ごとに `immediate` / `ticker` / `throttle` を切り替えられるようにする
- 特に command 系は event 駆動候補とする

### 2.3 動的宣言の扱い

- `DeclarePduForRead` / `DeclarePduForWrite` は Unity 側互換 API としては残る
- ただし本設計では、bridge 側はこれらを要求前提にしない
- 実際の転送対象は静的設定で決める

### 2.4 daemon 分離方針

- 既存 `src/daemon.cpp` は汎用 bridge daemon として残す
- Web bridge 用に `src/web_bridge_daemon.cpp` を追加する
- Web bridge daemon は既存 `BridgeCore` / `BridgeBuilder` / `EndpointContainer` を再利用する薄い起動ラッパに留める

### 2.5 config 管理方針

- `config/sample` や `config/tutorials` には置かない
- Web bridge 専用の config ディレクトリを `config/web_bridge/` 配下に作る
- ここを実運用・実装追従対象として扱う
- bridge / endpoint / comm / cache / pdu 定義の関係が崩れないよう、用途別に固定配置する
- 機体数が少ない間は静的ファイル管理を許容する
- 機体数が増えて手作業保守が破綻する場合は、config 自動生成を正式手段として採用する

### 2.6 スケーラビリティ方針

- 単一機体向けのベタ書き config を量産しない
- 機体ごとの差分は最小限のパラメータに閉じ込める
- PDU 型集合や policy group は可能な限り共有定義を使う
- 機体名、endpoint id、connection id、port などは規則的に生成可能な命名にする
- 100 機体運用で人手修正が必要な項目を最小化する


## 3. 設計概要

### 3.1 構成要素

- Web Bridge Daemon
  - Web bridge 用の起動エントリポイント
- EndpointContainer
  - SHM endpoint と WebSocket endpoint を初期化して保持する
- BridgeCore
  - connection 群を保持し、ticker ベースで転送を実行する
- BridgeConnection
  - source endpoint と destination endpoint 群を束ねる
- TransferPdu
  - 単一 PDU の転送を実行する
- TickerPolicy
  - 指定周期で転送可否を判定する

### 3.2 想定接続

- SHM source endpoint -> WebSocket destination endpoint
- WebSocket source endpoint -> SHM destination endpoint

必要に応じて、片方向ごとに別 connection として定義する。

初期実装では、現行 `hakoniwa-webserver` と同等の read/write 方向を静的 connection に落とし込む。

### 3.3 PDU 定義モデル

- PDU 定義は compact 形式を基本とする
- `work/drone-pdudef-1.json` と `work/drone-pdutypes.json` を参照元として扱う
- channel id と pdu name の対応は bridge 側と Unity 側で一致していることを前提とする

### 3.4 bridge.json のあるべき姿

`bridge.json` は単に connection を列挙するだけでなく、将来の policy 切替単位を表現できる構造であるべきとする。

そのため、

- PDU は用途ごとの group に分ける
- group ごとに policy を割り当てる
- 片方向の違いと timing 特性の違いを混在させない
- 機体数増加時にも機械生成しやすい構造にする

という原則を採る。


## 4. 詳細設計

### 4.1 実行モデル

- 箱庭 SHM endpoint は `impl_type=callback` を使う
- これは箱庭コアの callback asset モデルを利用するためである
- ただし bridge 転送そのものは recv callback ではなく `BridgeCore::cyclic_trigger()` 内の ticker 判定で行う

結果として、

- 箱庭側の PDU lifetime / asset integration は callback モデル
- bridge 側の forwarding cadence は ticker

という二層構造になる

ただし将来は forwarding cadence を PDU group ごとに切り替える。

### 4.2 SHM 側設計

- SHM endpoint は `hakoniwa-pdu-endpoint` の `comm_shm` を使う
- `impl_type` は `callback`
- `notify_on_recv` は本設計の必須条件ではない
- data receive event registration は転送トリガの前提にしない

留意点:

- `callback` を選ぶ理由は「箱庭コア callback asset モデルを使う」ためである
- `notify_on_recv` を使わないことと `callback` 採用は矛盾しない

### 4.3 WebSocket 側設計

- WebSocket endpoint は `hakoniwa-pdu-endpoint` の `comm_websocket` を使う
- role は `server`
- packet version は `v2`
- Unity クライアントの `comm_service_config.json` も `commRawVersion = "v2"` を設定する

### 4.4 Bridge 設計

- 初期実装では `ticker` を主 policy とする
- ただし `bridge.json` 上は将来の `immediate` / `throttle` 差し替えを許容する
- 片方向ごとに connection を分ける
- PDU group は「方向」と「タイミング特性」の両方で整理する

例:

- `conn_shm_to_ws`
- `conn_ws_to_shm`

PDU group 例:

- `ws_out_telemetry_ticker`
- `ws_out_status_ticker`
- `ws_in_command_ticker`
- `ws_in_command_event`

初期段階では実際の connection から event 用 group を参照しなくてもよいが、group 名と責務は先に分離しておく。

### 4.5 初期化シーケンス

Web bridge daemon の初期化順序は以下とする。

1. `EndpointContainer` を生成する
2. `EndpointContainer::initialize()` を実行する
3. `EndpointContainer::start_all()` を実行する
4. `EndpointContainer::post_start_all()` を実行する
5. `BridgeBuilder::build()` で `BridgeCore` を構築する
6. `BridgeCore::start()` を実行する
7. `BridgeCore::cyclic_trigger()` ループを開始する

`post_start_all()` を要求する理由:

- SHM callback 実装では `post_start()` で追加初期化が必要になる
- Web bridge daemon ではこの順序を明示し、既存汎用 daemon と区別する

### 4.6 設定ファイル設計

必要な設定は以下とする。

- `bridge.json`
  - 転送対象 PDU
  - connection
  - ticker interval
- `endpoint_container.json`
  - SHM endpoint
  - WebSocket endpoint
- SHM endpoint config
  - `comm.protocol = "shm"`
  - `impl_type = "callback"`
- WebSocket endpoint config
  - `comm.protocol = "websocket"`
  - `role = "server"`
  - `comm_raw_version = "v2"`

配置方針:

- `config/web_bridge/bridge/`
  - `bridge.json`
- `config/web_bridge/endpoint/`
  - endpoint config
- `config/web_bridge/comm/`
  - SHM / WebSocket comm config
- `config/web_bridge/cache/`
  - cache config
- `config/web_bridge/pdu/`
  - `drone-pdudef-1.json`
  - `drone-pdutypes.json`

この配置は sample ではなく設計の一部として扱う。

将来的に自動生成を導入する場合は、生成元も同ディレクトリ配下で管理する。

例:

- `config/web_bridge/templates/`
- `config/web_bridge/generated/`
- `tools/generate_web_bridge_config.py`

### 4.7 bridge.json 設計原則

`bridge.json` は以下を満たすように設計する。

1. 片方向ごとに connection を分離する
2. policy を切り替えたい単位ごとに `pduKeyGroup` を分離する
3. command 系と telemetry 系を混在させない
4. 初期実装が `ticker` でも、将来 event 駆動に切り替えられる命名にする
5. 複数機体分を規則的に生成できる naming にする

推奨する group 粒度:

- 方向
- ドメイン
- policy 候補

例:

- `drone_ws_out_status`
- `drone_ws_out_sensor`
- `drone_ws_in_camera_cmd`
- `drone_ws_in_game_cmd`
- `drone_ws_in_magnet_cmd`

初期実装ではこれらに全て `ticker` policy を割り当ててもよい。

機体単位の展開例:

- `drone001_ws_out_status`
- `drone001_ws_in_game_cmd`
- `drone002_ws_out_status`

ただし、機体ごとの group を完全個別定義すると 100 機体で保守困難になるため、最終的には生成規則ベースで出力する前提を持つ。

### 4.8 PDU グループ初期案

`info.txt` の legacy 宣言情報と `work/drone-pdutypes.json` から、初期対象候補は以下とする。

Web 側から箱庭へ送る候補:

- `impulse` (2)
- `hako_cmd_game` (9)
- `hako_status_magnet_holder` (15)
- `lidar_pos` (17)
- `lidar_points` (16)
- `hako_camera_data` (12)
- `hako_cmd_camera_info` (13)

箱庭から Web 側へ送る候補:

- `battery` (4)
- `hako_cmd_magnet_holder` (14)
- `hako_cmd_camera` (10)
- `hako_cmd_camera_move` (11)
- `disturb` (3)
- `status` (18)

注記:

- 最終的な向きは Unity 側実装と接続テストで確定する
- legacy webserver の declare 名称だけで決め切らず、実際の Unity 利用コードで再確認する
- 初期接続は現行 `hakoniwa-webserver` 相当挙動を優先する
- ただし group 分割は将来の policy 変更に備えて先に整理する
- 100 機体展開時には group 自体を手書きで複製しない

### 4.9 config 自動生成方針

機体数増加により、以下の条件を満たせなくなった場合は config 自動生成を採用する。

- 人手での修正差分レビューが容易であること
- 1 機体追加時の変更箇所が限定的であること
- connection / endpoint / port / node の対応関係を安全に維持できること

自動生成対象候補:

- `bridge.json`
- `endpoint_container.json`
- endpoint config
- comm config

自動生成の原則:

- 生成元は最小パラメータ集合にする
- 生成後ファイルは deterministic にする
- 生成物は実行対象としてリポジトリ管理してよい
- 手編集前提の generated file は作らない

### 4.10 制約事項

- endpoint loader はこのリポジトリで実装しない
- WebSocket protocol は `packet v2` 前提とする
- bridge 側で legacy webserver 互換の declare 制御は提供しない
- 本設計では monitor/on-demand 機能を主目的に含めない

### 4.11 移行方針

1. Unity 側の接続先を `hakoniwa-webserver` から Web bridge daemon へ切り替える
2. `commRawVersion` を `v2` に固定する
3. `bridge.json` に静的 PDU mapping を定義する
4. `DeclarePduForRead` / `DeclarePduForWrite` 依存箇所を段階的に削減する
5. 最終的に `hakoniwa-webserver` を不要化する


## 5. 実装メモ

### 5.1 追加ファイル

- `src/web_bridge_daemon.cpp`
- 必要なら Web bridge 向け sample config 一式

注記:

- `sample` という言葉は実運用 config には使わない
- Web bridge 用 config は `config/web_bridge/` 配下の正式管理対象とする

### 5.2 実装上の最重要点

- `web_bridge_daemon.cpp` は既存 core を再実装しない
- `EndpointContainer::post_start_all()` を確実に呼ぶ
- WebSocket `v2` と SHM `callback` の組み合わせを最初の成立条件として扱う

### 5.3 今後の検討項目

- `bridge.json` の具体値
- WebSocket 用 sample endpoint config
- Unity 側の declare 呼び出し削減
- `src/daemon.cpp` と `src/web_bridge_daemon.cpp` の共通化要否
- config 自動生成の要否と生成元スキーマ
