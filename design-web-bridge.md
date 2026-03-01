# Web Bridge Design

## 1. 設計要求

### 1.1 目的

- `hakoniwa-webserver` をレガシー実装として置き換える
- `hakoniwa-pdu-bridge-core` と `hakoniwa-pdu-endpoint` を使って、SHM と WebSocket 間の PDU 転送を実現する
- Unity クライアントは `hakoniwa-pdu-csharp` の `packet v2` を前提とする
- Web 側へ流す PDU、および Web 側から受ける PDU は `bridge.json` で静的に定義する
- 初期到達点は、現行 `hakoniwa-webserver` と同等の挙動を再現することとする
- 将来的に 100 機体規模へ拡張しても維持管理できる構造にする
- bridge runtime の状態と転送対象を monitor で観測できるようにする

### 1.2 非目的

- `hakoniwa-webserver` の動的 `DeclarePduForRead` / `DeclarePduForWrite` モデルを再現しない
- Web 側専用の独自プロトコルを新設しない
- endpoint loader をこのリポジトリで再実装しない
- 箱庭コアの時間管理を bridge 側で独自実装しない
- Web bridge 専用の monitor 実装を新設しない

注記:

- 初期目標は現行挙動の再現だが、動的 declare 制御そのものを bridge の本質設計には持ち込まない
- 現行相当の転送対象・転送方向を、静的設定で再現する

### 1.3 前提

- SHM 側は `hakoniwa-pdu-endpoint` の SHM endpoint を使う
- WebSocket 側は `hakoniwa-pdu-endpoint` の WebSocket endpoint を使う
- Unity 側の packet format は `v2`
- WebSocket 接続先は `hakoniwa-pdu-endpoint` / `hakoniwa-pdu-bridge-core` 構成へ寄せる
- 初期実装では `SHM -> WebSocket` は `ticker`、`WebSocket -> SHM` は `immediate` を採用する
- ただし PDU ごと、または PDU group ごとに policy を切り替えられる設計を前提とする
- config は sample 扱いにせず、専用ディレクトリ配下で厳密管理する
- 機体数増加に備え、config は人手編集前提に固定しない
- monitor は既存の `BridgeMonitorRuntime` / on-demand control plane を再利用する

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
- 初期実装では `SHM -> WebSocket` は `ticker`、`WebSocket -> SHM` は `immediate` を使う
- 将来的に command 系など一部 group は event 駆動へ切り替える可能性を残す
- 箱庭コア側が simulation time を管理し、bridge 側は必要に応じて real time 同期だけを行う

したがって、本設計は次の分担を採る。

- asset runtime model: callback
- transfer trigger model: policy-driven
- initial policies:
  - `SHM -> WebSocket`: `ticker`
  - `WebSocket -> SHM`: `immediate`


## 2. 設計方針

### 2.1 全体方針

- 箱庭コアとの統合は `callback` モデルに寄せる
- WebSocket 通信は `hakoniwa-pdu-endpoint` の既存 WebSocket comm を使う
- Unity との wire format は `packet v2` で統一する
- どの PDU をどちら向きに流すかは `bridge.json` で固定する
- `bridge.json` は将来の policy 差し替えを前提に、PDU group 単位で管理する
- monitor は既存 on-demand monitor を使い、web bridge 専用の control plane は増やさない
- config は sample ではなく運用対象として管理し、用途別ディレクトリを明示する
- 機体数が増えても同じ規則で展開できる naming / directory / group 設計を採る

### 2.2 転送モデル

- SHM -> WebSocket
  - 箱庭 SHM 上の PDU を `ticker` で WebSocket endpoint へ送る
- WebSocket -> SHM
  - Unity から受信した PDU を `immediate` で箱庭 SHM 側へ書く

ここでの「読み出し」は endpoint cache からの取得であり、初期構成の `SHM -> WebSocket` では SHM data receive event は転送トリガとして使用しない。

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
- monitor 初期化も既存 `BridgeMonitorRuntime` を呼び出すだけに留める

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
  - 箱庭 callback asset として bridge runtime を起動する
- EndpointContainer
  - SHM endpoint と WebSocket endpoint を初期化して保持する
- BridgeCore
  - connection 群を保持し、policy-driven に転送を実行する
- BridgeConnection
  - source endpoint と destination endpoint 群を束ねる
- TransferPdu
  - 単一 PDU の転送を実行する
- PduTransferPolicy
  - `immediate` / `throttle` / `ticker` の転送判定を担う
- BridgeMonitorRuntime
  - on-demand monitor session と control plane を管理する

### 3.2 想定接続

- SHM source endpoint -> WebSocket destination endpoint
- WebSocket source endpoint -> SHM destination endpoint
- monitor control client -> on-demand monitor mux endpoint

必要に応じて、片方向ごとに別 connection として定義する。

初期実装では、現行 `hakoniwa-webserver` と同等の read/write 方向を静的 connection に落とし込む。

### 3.3 PDU 定義モデル

- PDU 定義は compact 形式を基本とする
- `work/drone-pdudef-1.json` と `work/drone-pdutypes.json` を参照元として扱う
- channel id と pdu name の対応は bridge 側と Unity 側で一致していることを前提とする

### 3.5 monitor 構成

- monitor は既存 `BridgeMonitorRuntime` を web bridge daemon に接続して使う
- control plane は既存 on-demand control schema を使う
- Web bridge の転送 endpoint とは別に、monitor 用 endpoint mux を持つ
- monitor session は bridge 本体の connection 定義を参照して動的に monitor transfer を生成する

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
- Web bridge daemon 自身も箱庭 asset として動作する
- daemon は `hako_asset_register()` で asset 登録し、`hako_asset_start()` で callback 実行を開始する
- daemon の bridge 処理は `on_simulation_step()` の中で 1 step ごとに実行する
- `BridgeCore` に渡す policy 判定用 time source は `hakoniwa_callback` を使う
- real time sleep 用には別の `real` time source を持つ
- real time sleep は daemon option で ON/OFF を切り替えられるようにする
- ただし bridge 転送 policy は PDU group ごとに切り替え可能とする

結果として、

- 箱庭側の PDU lifetime / asset integration は callback モデル
- 箱庭側の simulation time は core 側が管理する
- bridge の policy 判定は箱庭 callback 時刻に従う
- bridge 側の forwarding cadence は policy-driven であり、必要なら別の real time sleep で外部同期する
- monitor control plane 処理は `BridgeCore::cyclic_trigger()` 内で `BridgeMonitorRuntime::process_control_plane_once()` により実行する

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
- `server-uri-v2.json` は接続先 URI 用であり、packet version は `comm_service_config.json` 側で管理する

### 4.3.1 起動順

- `SHM endpoint` を先に `start/post_start` する
- その後に `BridgeCore` を `build/start` する
- 最後に `WebSocket endpoint` を `start/post_start` する

理由:

- `WebSocket -> SHM` は `immediate` であり、bridge の subscriber 登録前に WebSocket を公開すると初回 packet を取りこぼす可能性がある
- `SHM -> WebSocket` は `ticker` であり、WebSocket 未接続中は `cyclic_trigger()` を回さなければよい

したがって本設計では、`SHM -> Bridge -> WebSocket` の順で起動する。

### 4.4 Bridge 設計

- 初期実装では `SHM -> WebSocket` を `ticker_20ms`、`WebSocket -> SHM` を `immediate` とする
- ただし `bridge.json` 上は将来の `immediate` / `ticker` / `throttle` 差し替えを許容する
- 片方向ごとに connection を分ける
- PDU group は「方向」と「タイミング特性」の両方で整理する

例:

- `conn_shm_to_ws`
- `conn_ws_to_shm`

PDU group 例:

- `ws_out_motion_ticker`
- `ws_out_status_ticker`
- `ws_in_command_immediate`
- `ws_in_command_event`

初期段階では実際の connection から event 用 group を参照しなくてもよいが、group 名と責務は先に分離しておく。

実装反映済みの初期 policy:

- `SHM -> WebSocket`
  - `motor`
  - `pos`
  - `battery`
  - `status`
  - `hako_cmd_camera`
  - `hako_cmd_camera_move`
  - `hako_cmd_magnet_holder`
  - `disturb`
  - 上記は `ticker_20ms`
- `WebSocket -> SHM`
  - `hako_cmd_game`
  - `impulse`
  - `hako_status_magnet_holder`
  - `lidar_pos`
  - `lidar_points`
  - `hako_camera_data`
  - `hako_cmd_camera_info`
  - 上記は `immediate`

### 4.5 monitor 設計

- monitor は既存 `BridgeMonitorRuntime` をそのまま使う
- web bridge daemon は monitor runtime を生成し、`BridgeCore::attach_monitor_runtime()` で接続する
- on-demand monitor を有効にする場合は mux config を別途受け取る
- control plane request/response schema は既存 `ondemand-control-request-schema.json` / `ondemand-control-response-schema.json` を使う
- monitor session の destination endpoint は on-demand mux から生成される per-session endpoint を使う
- monitor の policy は既存 monitor 制約に従う

非採用:

- Web bridge 専用 monitor schema
- WebSocket bridge endpoint を monitor control plane と兼用する構成

### 4.6 daemon での monitor 統合

- `web_bridge_daemon.cpp` は monitor を任意機能として持つ
- monitor 無効時は bridge 本体のみ起動する
- monitor 有効時は:
  1. `BridgeCore` 起動
  2. `BridgeMonitorRuntime` 生成
  3. `initialize(options)`
  4. `attach_monitor_runtime()`
- shutdown 時は `detach_monitor_runtime()` を呼び、monitor runtime を停止する

### 4.4.1 現行挙動の調査結果

`work/hakoniwa-webserver/config/custom.json` と `work/hakoniwa-webserver/config/twin-custom.json` を確認した結果、
Unity 向けの主系統として `drone_motor` と `drone_pos` が `shm_pdu_readers` に定義されている。

このため、Web bridge の初期 `SHM -> WebSocket` 構成では、少なくとも以下を含める。

- `motor`
- `pos`

`battery` `status` `lidar_points` `hako_camera_data` などは、初期主系統とは切り分けて扱う。

### 4.4.2 info.txt 更新後の静的マッピング

更新後の `info.txt` では、legacy `hakoniwa-webserver` が以下を動的宣言していた。

- `DeclarePduForRead`
  - `motor` (0)
  - `pos` (1)
  - `battery` (4)
  - `hako_cmd_magnet_holder` (14)
  - `hako_cmd_camera` (10)
  - `hako_cmd_camera_move` (11)
  - `disturb` (3)
  - `status` (18)
- `DeclarePduForWrite`
  - `impulse` (2)
  - `hako_cmd_game` (9)
  - `hako_status_magnet_holder` (15)
  - `lidar_pos` (17)
  - `lidar_points` (16)
  - `hako_camera_data` (12)
  - `hako_cmd_camera_info` (13)

本設計ではこれを静的 connection に落とし込む。

- `DeclarePduForRead`
  - Unity が読む対象であるため `SHM -> WebSocket`
- `DeclarePduForWrite`
  - Unity が書く対象であるため `WebSocket -> SHM`

したがって、初期 `bridge.json` は少なくとも上記一式を静的対象として持つ。

### 4.5 初期化シーケンス

Web bridge daemon の初期化順序は以下とする。

1. `hako_asset_register(asset_name, asset_config_path, callbacks, delta_time_step_usec, HAKO_ASSET_MODEL_CONTROLLER)` を実行する
2. `hako_asset_start()` を実行する
3. `on_initialize()` の中で `EndpointContainer` を生成する
4. `EndpointContainer::initialize()` を実行する
5. `EndpointContainer::start_all()` を実行する
6. `EndpointContainer::post_start_all()` を実行する
7. `create_time_source("hakoniwa_callback", delta_time_step_usec)` で `BridgeCore` 用 time source を生成する
8. `create_time_source("real", delta_time_step_usec)` で real sleep 用 time source を生成する
9. `BridgeBuilder::build()` で `BridgeCore` を構築する
10. `BridgeCore::start()` を実行する
11. `on_simulation_step()` の中で `BridgeCore::cyclic_trigger()` を 1 回実行する
12. 必要なら daemon option に従って real time sleep を挿入する

`post_start_all()` を要求する理由:

- SHM callback 実装では `post_start()` で追加初期化が必要になる
- Web bridge daemon ではこの順序を明示し、既存汎用 daemon と区別する

補足:

- `hako_asset_pdu_create()` / `hako_asset_pdu_read()` / `hako_asset_pdu_write()` は、箱庭 asset 初期化の前提が満たされている必要がある
- したがって Web bridge では、bridge / endpoint 初期化の前に箱庭 asset 登録を済ませる
- simulation time 自体は箱庭コア機能側が管理する
- そのため `BridgeCore` に渡す time source は `hakoniwa_callback` とする
- 一方、wall-clock pacing が必要な場合だけ別の `real` time source で sleep を行う

### 4.6 設定ファイル設計

必要な設定は以下とする。

- `bridge.json`
  - 転送対象 PDU
  - connection
  - transfer policy
- `endpoint_container.json`
  - SHM endpoint
  - WebSocket endpoint
- asset config
  - `hako_asset_register()` に渡す箱庭 asset 用 pdudef
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
  - `drone-pdudef.json`
  - `drone-pdutypes.json`

この配置は sample ではなく設計の一部として扱う。

将来的に自動生成を導入する場合は、生成元も同ディレクトリ配下で管理する。

例:

- `config/web_bridge/templates/`
- `config/web_bridge/generated/`
- `tools/generate_web_bridge_config.py`

### 4.6.1 name フィールドに関する調査結果

本設計では、以下の 3 つの `name` を混同しない。

- endpoint config の `name`
- comm config の `name`
- 箱庭コアの asset 名

調査結果:

- endpoint config の `name`
  - `hakoniwa-pdu-endpoint` の `Endpoint` インスタンス名として使われる
  - container 管理上は endpoint `id` の方が主識別子であり、asset 名ではない
- SHM comm config の `name`
  - callback 実装では本質的な asset 識別子として使われていない
  - 設定ラベルとしての意味が強い
- 箱庭コアの asset 名
  - `hakoniwa-core-pro` の asset runtime で使われる実行主体名である
  - callback / polling の実行モデルと結びつく

設計判断:

- Web bridge では endpoint `name` と comm `name` を、箱庭 asset 名と一致させることを要求しない
- ただし将来の運用混乱を避けるため、命名規則は役割ベースで統一する
- 箱庭 asset 名が必要な場合は、daemon 設計の責務として別途明示する

### 4.7 bridge.json 設計原則

`bridge.json` は以下を満たすように設計する。

1. 片方向ごとに connection を分離する
2. policy を切り替えたい単位ごとに `pduKeyGroup` を分離する
3. command 系と telemetry 系を混在させない
4. 初期実装が `immediate` でも、将来 `ticker` や event 駆動に切り替えられる命名にする
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

初期実装では `immediate` を割り当て、将来必要な group のみ `ticker` や event 駆動へ切り替える。

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

- `motor` (0)
- `pos` (1)
- `battery` (4)
- `hako_cmd_magnet_holder` (14)
- `hako_cmd_camera` (10)
- `hako_cmd_camera_move` (11)
- `disturb` (3)
- `status` (18)

注記:

- `motor` / `pos` は legacy config 調査で Unity 向け主系統として確認済みである
- `DeclarePduForRead` は `SHM -> WebSocket`、`DeclarePduForWrite` は `WebSocket -> SHM` として静的化する
- 初期接続は現行 `hakoniwa-webserver` 相当挙動を優先する
- ただし group 分割は将来の policy 変更に備えて先に整理する
- 100 機体展開時には group 自体を手書きで複製しない

### 4.8.1 現時点の warning 整理

- Unity 側は依然として `DeclarePduForRead` / `DeclarePduForWrite` を送信する
- 静的 bridge 構成では declare packet を転送制御には使わない
- そのため、受信 source として未使用の channel では `No subscribers found` warning が出る
- `WebSocket -> SHM` 側では 4 byte の declare payload が通常 PDU として扱われ、サイズ不一致 warning が出ることがある
- `hako_cmd_game` は Unity 側サイズが正であり、bridge 側 PDU 定義との差分整理が残課題である

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
- real time 同期は daemon option として扱い、常時強制しない

### 4.11 移行方針

1. Unity 側の接続先を `hakoniwa-webserver` から Web bridge daemon へ切り替える
2. `commRawVersion` を `v2` に固定する
3. `bridge.json` に静的 PDU mapping を定義する
4. `DeclarePduForRead` / `DeclarePduForWrite` 依存箇所を段階的に削減する
5. 最終的に `hakoniwa-webserver` を不要化する


## 5. 実装メモ

### 5.1 追加ファイル

- `src/web_bridge_daemon.cpp`
- `config/web_bridge/` 配下の正式 config 一式

注記:

- `sample` という言葉は実運用 config には使わない
- Web bridge 用 config は `config/web_bridge/` 配下の正式管理対象とする

### 5.2 実装上の最重要点

- `web_bridge_daemon.cpp` は既存 core を再実装しない
- `web_bridge_daemon.cpp` は箱庭 asset 初期化を正しく行う
- `hako_asset_register()` / `hako_asset_start()` / callback 実行順を守る
- bridge 処理は `on_simulation_step()` で step 駆動にする
- `EndpointContainer::post_start_all()` を確実に呼ぶ
- `BridgeCore` の policy 時刻は `hakoniwa_callback` time source を使う
- real sleep は別の `real` time source で行う
- real time sleep は option で ON/OFF を切り替えられるようにする
- WebSocket `v2` と SHM `callback` の組み合わせを最初の成立条件として扱う

### 5.3 今後の検討項目

- `bridge.json` の具体値
- WebSocket 用 sample endpoint config
- Unity 側の declare 呼び出し削減
- `src/daemon.cpp` と `src/web_bridge_daemon.cpp` の共通化要否
- config 自動生成の要否と生成元スキーマ
