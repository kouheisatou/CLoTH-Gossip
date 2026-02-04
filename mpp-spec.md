# MPP (Multi-Path Payment) 処理仕様書

本ドキュメントはCLoTH-GossipシミュレータにおけるMulti-Path Payment (MPP) 処理フローを記述する。

---

## 1. システム概要

### 1.1 イベント駆動型アーキテクチャ

CLoTHは離散イベントシミュレーションモデルを使用する。イベントは優先度キュー（ヒープ）から時系列順に処理される。

**ペイメント関連イベント一覧:**

| イベント           | ハンドラ関数         | 説明                           |
|--------------------|----------------------|--------------------------------|
| `FINDPATH`         | `find_path()`        | ペイメント経路を探索           |
| `SENDPAYMENT`      | `send_payment()`     | 送信者がHTLCを開始             |
| `FORWARDPAYMENT`   | `forward_payment()`  | 中間ノードがHTLCを転送         |
| `RECEIVEPAYMENT`   | `receive_payment()`  | 受信者がペイメントを受領       |
| `FORWARDSUCCESS`   | `forward_success()`  | 中間ノードが成功を転送         |
| `RECEIVESUCCESS`   | `receive_success()`  | 送信者が成功通知を受領         |
| `FORWARDFAIL`      | `forward_fail()`     | 中間ノードが失敗を転送         |
| `RECEIVEFAIL`      | `receive_fail()`     | 送信者が失敗通知を受領         |

### 1.2 ペイメントエラー種別

| エラー種別                   | 説明                         | 挙動                       |
|------------------------------|------------------------------|----------------------------|
| `NOBALANCE`                  | エッジの残高不足             | 別経路でリトライ           |
| `OFFLINENODE`                | 次ノードがオフライン         | 別経路でリトライ           |
| `NOPATH` (dijkstra)          | 有効な経路が存在しない       | **ペイメント即終了**       |
| `NOLOCALBALANCE` (dijkstra)  | 送信者のローカル残高不足     | **ペイメント即終了**       |

### 1.3 状態遷移図

CLoTHのペイメント処理は以下の状態遷移に従う:

```
                          +------------------+
                          |                  |
                          v                  |
+------------+       +------------+       +------------+       +------------+
| find path  |------>|   send     |------>|  forward   |------>|  receive   |
|            |       |  payment   |       |  payment   |       |  payment   |
+------------+       +------------+       +------------+       +------------+
      ^                                     |    ^                   |
      |                                     |    |                   |
      |                                     |    +-------------------+
      |                                     |    (中間ノードが複数の場合ループ)
      |                                     |
      |                                     v
      |              +------------+       +------------+
      |              |  receive   |<------|  forward   |<------+
      |              |   fail     |       |   fail     |       |
      |              +------------+       +------------+       |
      |                   |                                    |
      +-------------------+                                    |
      (リトライ)                                               |
                                                               |
                         +------------+       +------------+   |
                         |  receive   |<------|  forward   |---+
                         |  success   |       |  success   |
                         +------------+       +------------+
                                                    ^
                                                    |
                                              (receive paymentから)
```

**成功フロー:** find path → send payment → forward payment(複数回) → receive payment → forward success(複数回) → receive success

**失敗フロー:** find path → send payment → forward payment → forward fail(複数回) → receive fail → find path (リトライ)

---

## 2. 現行MPP実装 (GCB無効時)

### 2.1 基本フロー

```
[ペイメント開始]
      |
      v
[FINDPATHイベント] --> find_path()
      |
      +-- 初回試行?
      |       |
      |       +-- YES: 事前計算済みパスを使用 (paths[payment->id])
      |       |
      |       +-- NO: 除外エッジ付きでdijkstra()を呼び出し
      |
      v
[パス発見?]
      |
      +-- YES --> [SENDPAYMENTイベント生成]
      |                    |
      |                    v
      |             [ペイメントフロー開始]
      |
      +-- NO --> [MPP適格?]
                      |
                      +-- NO --> [ペイメント終了 (NOPATH)]
                      |
                      +-- YES --> [2シャードに分割]
```

### 2.2 MPP発動条件

MPPは以下の条件が**全て**満たされた場合に発動する:

1. `mpp`フラグが有効 (mpp=1)
2. `path == NULL` (dijkstraがパスなしを返却)
3. `payment->is_shard == 0` (既にシャードではない)
4. `payment->attempts == 1` (初回試行のみ)

**現行の制限:** MPPは初回試行時のみ発動する。以降のNOPATH失敗ではペイメントは即終了する。

### 2.3 シャード生成プロセス

```
[親ペイメント (amount=A)]
            |
            v
    [50/50で分割]
            |
    +-------+-------+
    |               |
    v               v
[シャード1]     [シャード2]
amount=A/2      amount=A-(A/2)
    |               |
    v               v
[パス探索]      [パス探索]
    |               |
    +-- 失敗? --> [親ペイメント終了]
    |               |
    +-- 成功        +-- 失敗? --> [親ペイメント終了]
    |               |
    v               v
[SENDPAYMENT]   [SENDPAYMENT]
```

**シャードのプロパティ:**

- `shard->is_shard = 1`
- `shard->attempts = 1`
- `shard->max_fee_limit = parent->max_fee_limit / 2`
- 親の`shards_id[0]`と`shards_id[1]`に子シャードIDを格納

### 2.4 重複パス検出 (非CLOTH_ORIGINAL時)

routing_methodがCLOTH_ORIGINAL以外の場合:

- shard1_pathとshard2_pathが同一エッジを持つ場合、ペイメントは失敗
- 同じ混雑パスに両シャードを送信することを防止

### 2.5 エッジ除外付きリトライロジック

`RECEIVEFAIL`時、ペイメントはリトライを試みる:

1. 失敗エッジを`payment->history`に記録
2. 新規`FINDPATH`イベントを生成
3. `find_path()`内で:
   - payment->historyからexclude_edgesリストを構築
   - exclude_edgesパラメータ付きで`dijkstra()`を呼び出し
   - Dijkstraはexclude_edgesリスト内のエッジをスキップ

### 2.6 後処理 (post_process_payment_stats)

シミュレーション完了後、シャードを持つペイメントに対して:

- `end_time` = max(shard1.end_time, shard2.end_time)
- `is_success` = shard1.is_success AND shard2.is_success
- `no_balance_count` = 両シャードの合計
- `offline_node_count` = 両シャードの合計
- `is_timeout` = shard1.is_timeout OR shard2.is_timeout
- `attempts` = 両シャードの合計
- `total_fee` = 両ルートの手数料合計

### 2.7 現行実装の制限事項

1. **2分割固定**: 現行実装は2シャードへの分割のみサポート
2. **初回のみ**: MPPは初回試行のNOPATH時のみ発動
3. **再帰分割なし**: シャードをさらに分割することは不可
4. **協調なし**: シャードは協調なく独立して実行
5. **ロールバック未実装**: 失敗シャードが成功シャードのロールバックをトリガーしない

---

## 3. MPP実装 (GCB無効時, LNに実装されている方式)

### 3.1 概要

Lightning Networkに実装されているMPP方式を模倣した強化版実装。シングルパスでの送金を試行し、失敗した場合に再帰的な2分割を行う。

### 3.2 基本フロー

```
[ペイメント開始]
      |
      v
[シングルパス試行] ──────────────────────────────────┐
      |                                              |
      +── 成功 ──> [ペイメント完了]                  |
      |                                              |
      +── NOBALANCE/OFFLINENODE ──> [リトライ] ─────┘
      |
      +── NOPATH (リトライ後も経路なし) ──> [MPP処理に移行]
                                                |
                                                v
                                    [2分割してシャード生成]
                                                |
                              +-----------------+-----------------+
                              |                                   |
                              v                                   v
                        [シャード1]                         [シャード2]
                              |                                   |
                              +── 成功                            +── 成功
                              |                                   |
                              +── 失敗 ──> [再帰的に2分割]        +── 失敗 ──> [再帰的に2分割]
                                              |                                   |
                                              v                                   v
                                    [最小分割サイズに達するまで繰り返し]
```

### 3.3 MPP移行条件

以下の条件でMPPに移行する:

1. `mpp`フラグが有効 (mpp=1)
2. シングルパスでのリトライを繰り返した結果、`dijkstra()`が`NULL`を返却 (NOPATH)
3. まだ`max_shard_count`に達していない

**重要:** NOBALANCEやOFFLINENODEエラーでは、まずシングルパスでのリトライを試みる。リトライを繰り返してもパスが見つからなくなった時点でMPPに移行する。

### 3.4 再帰的2分割アルゴリズム

```
function try_payment(payment):
    path = find_path(payment)

    if path != NULL:
        send_payment(payment, path)
        wait_for_result()

        if SUCCESS:
            return SUCCESS
        else if NOBALANCE or OFFLINENODE:
            // リトライ (エッジを除外リストに追加)
            add_to_exclude_list(failed_edge)
            return try_payment(payment)  // 同じ金額でリトライ
        else:
            // その他のエラー
            return FAIL

    else:  // path == NULL (NOPATH)
        if payment.amount < MIN_SHARD_SIZE:
            return FAIL  // これ以上分割不可

        if current_shard_count >= max_shard_count:
            return FAIL  // シャード数上限

        // 2分割
        shard1 = create_shard(payment.amount / 2)
        shard2 = create_shard(payment.amount - payment.amount / 2)

        result1 = try_payment(shard1)
        result2 = try_payment(shard2)

        if result1 == SUCCESS and result2 == SUCCESS:
            return SUCCESS
        else:
            rollback_all_shards()
            return FAIL
```

### 3.5 シャード管理

**親ペイメントとシャードの関係:**

```
[親ペイメント P0 (amount=1000)]
         |
    [NOPATH発生]
         |
    +----+----+
    |         |
    v         v
  [S1]      [S2]
 (500)     (500)
    |         |
 [成功]   [NOPATH]
              |
         +----+----+
         |         |
         v         v
       [S2a]     [S2b]
       (250)     (250)
         |         |
      [成功]    [成功]

結果: P0成功 (S1, S2a, S2bが全て成功)
```

**シャードツリー構造:**

- 各シャードは親シャードへの参照を持つ
- 親ペイメントは全ての葉シャードの状態を追跡
- 全ての葉シャードが成功した場合のみ親ペイメントが成功

### 3.6 ロールバック処理

いずれかのシャードが最終的に失敗した場合:

1. 全てのシャードの送金をキャンセル
2. 既に成功したシャードについてもHTLCをタイムアウトさせてロールバック
3. 親ペイメントを失敗としてマーク

**注意:** 実際のLNでは、受信者がpreimageを公開するまでHTLCは確定しない。全シャードが到着するまで受信者はpreimageを公開しないため、部分的な成功状態は発生しない。

### 3.7 最小シャードサイズ

設定パラメータまたはネットワークポリシーで決定:

- `MIN_SHARD_SIZE`: これ以下の金額には分割しない
- 各エッジの`policy.min_htlc`も考慮する必要がある

### 3.8 状態遷移 (MPPモード)

```
[FINDPATH]
    |
    +-- path発見 --> [SENDPAYMENT] --> (通常フロー)
    |
    +-- path=NULL --> [MPP_SPLIT]
                          |
                    +-----+-----+
                    |           |
                    v           v
              [FINDPATH]   [FINDPATH]  (子シャード)
                    |           |
                   ...         ...
                    |           |
                    v           v
              [WAIT_CHILDREN]  (親で全子の完了を待機)
                    |
              +-----+-----+
              |           |
              v           v
          [全成功]    [いずれか失敗]
              |           |
              v           v
         [SUCCESS]   [ROLLBACK]
```

---

## 4. GCB有効時MPP (提案機能)

### 4.1 非GCB MPPとの主な違い

| 観点               | 非GCB MPP                    | GCB有効MPP                   |
|--------------------|------------------------------|------------------------------|
| 分割戦略           | 再帰的2分割                  | 最適N分割                    |
| 容量情報           | 不明 (チャネル容量使用)      | 既知 (group_cap)             |
| パス選択           | 利用可能な任意のパス         | 手数料の安いリンクを優先     |
| 非グループリンク   | チャネル容量を使用           | `容量 / 2` で推定            |
| 失敗時リトライ     | 失敗エッジを除外             | 非グループリンクを除外       |

### 4.2 GCB MPPフロー

```
[ペイメント開始]
      |
      v
[シングルパス試行] <──── 常に最初はシングルパスを試行
      |
      +── 成功 ──> [ペイメント完了]
      |
      +── NOBALANCE/OFFLINENODE ──> [シングルパスでリトライ]
      |                                    |
      |                             (リトライループ)
      |                                    |
      +── NOPATH ──> [MPPに移行]
                            |
                            v
                   [利用可能パスを分析]
                            |
                            v
                   [最適N分割を計算]
                            |
                            v
                   [Nシャードを生成]
```

### 4.3 GCB容量推定

パス容量計算:

```
パス内の各エッジに対して:
    if (edge->group != NULL):
        estimated_cap = edge->group->group_cap
    else:
        estimated_cap = channel_capacity / 2  // 保守的な推定

    path_capacity = min(path_capacity, estimated_cap)
```

### 4.4 最適分割計算

グループ容量情報を使用:

1. 受信者への全利用可能パスを探索
2. 各パスについて計算:
   - `path_capacity` = グループ化エッジの最小group_cap
   - `path_fee` = パスの手数料合計
3. パスを手数料昇順でソート
4. 最も安いパスから貪欲にペイメント金額を割り当て
5. シャード数Nを計算

```
remaining_amount = payment->amount
shards = []
for path in sorted_paths_by_fee:
    if remaining_amount <= 0:
        break
    shard_amount = min(remaining_amount, path_capacity)
    shards.append({path, shard_amount})
    remaining_amount -= shard_amount

if remaining_amount > 0:
    // ペイメント完了不可
    return FAILURE
```

### 4.5 非グループリンクの処理

パスにグループに属さないエッジが含まれる場合:

1. 容量を`channel_capacity / 2`と推定
2. 非グループリンクでペイメントが失敗した場合:
   - リンクを除外リストに追加
   - このリンクなしで`find_path()`をリトライ
3. 不明な容量のリンクでの繰り返し失敗を防止

### 4.6 最大シャード数

設定パラメータ: `max_shard_count` (デフォルト: 16)

- 過剰な分割を防ぐためシャード数を制限
- 最適分割が制限を超えるシャード数を必要とする場合、ペイメントは失敗

---

## 5. 主要データ構造

### 5.1 ペイメント構造体

```c
struct payment {
    long id;
    long sender;
    long receiver;
    uint64_t amount;
    uint64_t max_fee_limit;
    struct route* route;
    uint64_t start_time;
    uint64_t end_time;
    int attempts;
    struct payment_error error;

    // MPPフィールド
    unsigned int is_shard;      // シャードの場合は1
    long shards_id[2];          // 子シャードID (なければ-1)

    // 統計
    unsigned int is_success;
    int offline_node_count;
    int no_balance_count;
    unsigned int is_timeout;
    struct element* history;    // struct attemptのリスト
};
```

### 5.2 試行履歴

```c
struct attempt {
    int attempts;
    uint64_t end_time;
    long error_edge_id;
    enum payment_error_type error_type;
    struct array* route;        // エッジスナップショット
    short is_succeeded;
};
```

---

## 6. 設定パラメータ

| パラメータ           | 説明                             | MPP関連性              |
|----------------------|----------------------------------|------------------------|
| `mpp`                | MPP有効/無効 (0または1)          | MPPに必須              |
| `max_shard_count`    | 最大シャード数                   | 分割制限               |
| `routing_method`     | ルーティングアルゴリズム選択     | 容量推定に影響         |
| `group_size`         | グループあたりのエッジ数         | GCBグルーピング        |
| `payment_timeout`    | ペイメントタイムアウト (ms)      | 全シャードに適用       |

---

## 7. デバッグログ仕様

MPP処理のデバッグ用に以下のログを出力する:

**成功時:**
```
[MPP DEBUG] SUCCESS: payment_id=%ld, shard_count=%d, total_fee=%lu, total_time=%lu
```

**失敗時:**
```
[MPP DEBUG] FAIL: payment_id=%ld, reason=%s, failed_shard_id=%ld, attempts=%d
```

**分割時:**
```
[MPP DEBUG] SPLIT: parent_id=%ld, parent_amount=%lu, shard1_id=%ld, shard1_amount=%lu, shard2_id=%ld, shard2_amount=%lu
```

**シャード完了時:**
```
[MPP DEBUG] SHARD_COMPLETE: shard_id=%ld, parent_id=%ld, is_success=%d, fee=%lu
```
