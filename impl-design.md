# 1. 目的とスコープ

* 本リポジトリは `hakoniwa-pdu-bridge`を実装する
* `EndPoint` の生成・設定は **外部**（`hakoniwa-pdu-endpoint` の loader）に委譲する
* 本実装のローダーは **bridge.json のみ**を読む
* endpoint.json は読まない（必要なら参照パスを受け取るだけ）

# 2. 用語

* BridgeCore / BridgeConnection / TransferPdu / PduKey / Epoch(owner_epoch)
* src/dst endpoint
* policy(immediate/throttle/ticker)

# 3. 入出力仕様

## 3.1 bridge.jsonのスキーマ

config/schema/bridge-config.schema.json を参照


# 4. クラス設計（責務と主要メソッド）

## 4.1 BridgeDaemon(main)

* `main(args)` で `BridgeCore` を作って run

## 4.2 BridgeCore
責務:

* bridge.json をロードして `BridgeConnection` を組み立てる（組み立て自体は Loader でもOK）
* `connections` を保持し、run loop を回す

主要メソッド例:

* `load(const BridgeConfig&)`
* `run()` / `stop()`

## 4.3 BridgeConnection

責務:

* src/dst endpoint を2つ保持（参照 or shared_ptr）
* TransferPdu の配列を持つ
* `step()` で転送を実行

主要メソッド:

* `step(now)` (policy/tickerに合わせるなら now 渡し)

## 4.4 TransferPdu

責務:

* `PduKey` と `epoch/active` を持つ
* policy を持つ（strategy）
* src→dst の “単一PDU転送” を行う

主要メソッド:

* `transfer()` または `try_transfer(now)`
* `set_active(bool)`
* `set_epoch(uint64_t)`
* `accept_epoch(epoch)`（dst側で破棄判定するならここがガード）

## 4.5 PduTransferPolicy（interface）

* `bool should_transfer(now)` / `void on_transferred(now)` など

派生:

* Immediate
* Throttle
* Ticker

# 5. Runtime Delegation（epoch二重送信の扱い）

* owner切替の瞬間は旧新が同時送信しうる
* 受け側（または bridge）で **最新epoch以外は捨てる**
* 捨てる場所:

  * 推奨: `TransferPdu::accept_epoch()` で reject
  * policyには混ぜない（タイミングと整合性を分離）

# 6. 依存関係と禁止事項

* `BridgeCore` は JSON を直接知らない（Loaderに寄せる）でもよい
* endpoint生成は `hakoniwa-pdu-endpoint` の API を利用（ここで再実装しない）
* 循環参照禁止（include方向ルール）

# 7. 実装タスク分割（geminiに優しい）

1. データ構造（Config DTO）
2. Policy interface + 3実装
3. TransferPdu
4. BridgeConnection
5. BridgeCore run loop（最初は単純でOK）
6. BridgeLoader（bridge.json→BridgeCore構築）
7. 最小サンプル + テスト（可能なら）

---

# 補足

* 「ファイル構成は 1クラス1ファイル」
* 「例外は policy/ と config/ のみ」
* 「C++20、エラーハンドリングは HakoPduErrorType（または例外禁止）」
* 「まずは動く最小を作り、その後拡張」

