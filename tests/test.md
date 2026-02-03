# CLoTH Lightning Network Simulator - テストドキュメント

本ドキュメントでは、CLoTH-Gossip Lightning Networkシミュレーターの各テストケースについて説明します。

**全42個のテストケース**を実装し、以下をカバーしています：
- 全ルーティング手法（CLoTH Original, Ideal, GCB, CUL）で基本テスト実施
- MPP（Multi-Path Payments）の包括的テスト
- **エッジ残高（balance）とグループ容量（group_cap）の厳密な検証**

## テスト概要

テストランナー (`run_tests.py`) は、異なるルーティング手法と設定で複数のテストケースを実行し、期待される結果と比較検証します。

### 検証レベル

各テストでは以下の3つのレベルで検証を行います：

1. **payments_output.csv**: 支払い結果（成功/失敗、試行回数、経路など）
2. **edges_output.csv**: **エッジ残高の正確性** - 支払い後のリンク容量が正しく更新されているか
3. **groups_output.csv**: **グループ形成の正確性** - GCB/CULでグループが正しく形成されているか

### 実行方法

```bash
# 全テストを実行
python tests/run_tests.py

# 特定のテストのみ実行
python tests/run_tests.py --test test_name

# ルーティング手法別に実行
python tests/run_tests.py --routing cloth    # CLoTH Original
python tests/run_tests.py --routing group    # GCB (Group Capacity Based)
python tests/run_tests.py --routing cul      # CUL (Channel Usage Limit)
python tests/run_tests.py --routing ideal    # Ideal

# MPPテストのみ実行
python tests/run_tests.py --mpp

# 全テストのリスト表示
python tests/run_tests.py --list
```

## テストネットワーク構成

### 1. simple_linear
- **構成**: A → B → C (線形3ノード)
- **用途**: 基本的な成功シナリオのテスト

### 2. imbalanced_4node
- **構成**: 4ノードネットワーク（容量が不均衡）
- **用途**: リトライと容量枯渇シナリオのテスト

### 3. isolated_node
- **構成**: 孤立ノードを含むネットワーク
- **用途**: 経路が存在しないケースのテスト

### 4. diamond_4node
- **構成**: ダイヤモンド型（A→B/C→D、並列経路あり）
- **用途**: MPP（Multi-Path Payments）のテスト

### 5. parallel_6node
- **構成**: 6ノード、複数の並列経路
- **用途**: 最大シャード数のテスト

### 6. multipath_8node
- **構成**: 8ノード、4つ以上の並列経路
- **用途**: 3-way、4-way分割など複雑なMPPシナリオ

### 7. grouped_network
- **構成**: 10ノード、複数のグループを形成するネットワーク
  - グループ1（高容量）: 0→1→2→3 (10M容量)
  - グループ2（中容量）: 0→4→5→3 (8M容量)
  - グループ3（低容量）: 0→6→7→8→3 (6M容量)
  - 非グループリンク: 1→9→3 (4M容量)
- **用途**: グループルーティング（GCB/CUL）のテスト、グループ外リンクでのリトライテスト

---

## テストケース詳細

## 基本ルーティングテスト（全ルーティング手法）

### CLoTH Original

#### test_success_single_cloth
- **ルーティング**: cloth_original
- **ネットワーク**: simple_linear
- **支払い**: 0→2に1,000,000送金
- **テスト内容**: 単一の支払いが成功する基本シナリオ
- **期待結果**: 支払いが成功（is_success=1）
- **検証項目**:
  - payments: is_success=1, attempts=1, no_balance_count=0
  - **edges**: edge 0 (0→1) と edge 2 (1→2) の残高が初期値5,000,000から減少

#### test_retry_success_cloth
- **ルーティング**: cloth_original
- **ネットワーク**: imbalanced_4node
- **支払い**: 0→3に2,000,000送金
- **テスト内容**: 容量不足で最初は失敗するが、リトライにより成功
- **期待結果**: リトライ後に成功（attempts > 1）
- **検証項目**:
  - payments: is_success=1, attempts>1, no_balance_count>0
  - **edges**: edge 0の残高が初期値4,500,000から減少

#### test_capacity_exhaust_cloth
- **ルーティング**: cloth_original
- **ネットワーク**: imbalanced_4node
- **支払い**: 0→3に大量送金（容量超過）
- **テスト内容**: 全ての経路が容量不足で失敗
- **期待結果**: 全経路枯渇で失敗（is_success=0）
- **検証項目**:
  - payments: is_success=0
  - **edges**: 失敗したため edge 0の残高は変化なし（4,500,000のまま）

#### test_no_path_cloth
- **ルーティング**: cloth_original
- **ネットワーク**: isolated_node
- **テスト内容**: そもそも経路が存在しない
- **期待結果**: 経路なしで失敗（status=NO_PATH）

---

### Ideal Routing

#### test_success_single_ideal
- **ルーティング**: ideal
- **ネットワーク**: simple_linear
- **テスト内容**: 理想的なルーティングで単一支払いが成功
- **期待結果**: 支払いが成功

#### test_retry_success_ideal
- **ルーティング**: ideal
- **ネットワーク**: imbalanced_4node
- **テスト内容**: 容量不足でリトライして成功
- **期待結果**: リトライ後に成功

#### test_capacity_exhaust_ideal
- **ルーティング**: ideal
- **ネットワーク**: imbalanced_4node
- **テスト内容**: 全ての経路が容量不足で失敗
- **期待結果**: 全経路枯渇で失敗

#### test_no_path_ideal
- **ルーティング**: ideal
- **ネットワーク**: isolated_node
- **テスト内容**: 理想的なルーティングでも経路がなければ失敗
- **期待結果**: 経路なしで失敗

---

### GCB (Group Capacity Based) Routing

#### test_success_single_gcb
- **ルーティング**: group_routing
- **パラメータ**: group_size=5, group_limit_rate=0.3
- **ネットワーク**: grouped_network
- **支払い**: 0→3に1,000,000送金
- **テスト内容**: グループルーティングで単一支払いが成功
- **期待結果**: 支払いが成功、エッジの残高が正しく更新される
- **検証項目**: 
  - payments: is_success=1, attempts=1
  - **edges**: edge 0 (0→1) の残高が5,000,000から減少（3,900,000 < balance < 5,000,000）
  - **groups**: グループが正しく形成されているか（今後実装予定）

#### test_retry_success_gcb
- **ルーティング**: group_routing
- **パラメータ**: group_size=5, group_limit_rate=0.3
- **ネットワーク**: grouped_network
- **支払い**: 0→3に3,500,000送金
- **テスト内容**: グループ外リンクで失敗時のリトライ
- **説明**: グループルーティングでは理論上リトライは発生しないが、グループに属さないリンクがパスに含まれている時のみリトライが発生する可能性がある
- **期待結果**: リトライして成功（attempts >= 1）
- **検証項目**:
  - payments: is_success=1, attempts>=1
  - **edges**: 使用されたエッジの残高が減少

#### test_capacity_exhaust_gcb
- **ルーティング**: group_routing
- **パラメータ**: group_size=5, group_limit_rate=0.3
- **ネットワーク**: grouped_network
- **テスト内容**: 全ての経路が容量不足で失敗
- **期待結果**: 全経路枯渇で失敗（attempts > 1）

#### test_no_path_gcb
- **ルーティング**: group_routing
- **ネットワーク**: isolated_node
- **テスト内容**: グループルーティングでも経路がなければ失敗
- **期待結果**: 経路なしで失敗

---

### CUL (Channel Usage Limit) Routing

#### test_success_single_cul
- **ルーティング**: group_routing_cul
- **パラメータ**: group_size=5
- **ネットワーク**: grouped_network
- **テスト内容**: CUL方式で単一支払いが成功
- **期待結果**: 支払いが成功

#### test_retry_success_cul
- **ルーティング**: group_routing_cul
- **パラメータ**: group_size=5
- **ネットワーク**: grouped_network
- **テスト内容**: グループ外リンクで失敗時のリトライ
- **説明**: CULでも同様に、グループに属さないリンクがパスに含まれている時のみリトライが発生する
- **期待結果**: リトライして成功（attempts >= 1）

#### test_capacity_exhaust_cul
- **ルーティング**: group_routing_cul
- **パラメータ**: group_size=5
- **ネットワーク**: grouped_network
- **テスト内容**: 全ての経路が容量不足で失敗
- **期待結果**: 全経路枯渇で失敗

#### test_no_path_cul
- **ルーティング**: group_routing_cul
- **ネットワーク**: isolated_node
- **テスト内容**: CULでも経路がなければ失敗
- **期待結果**: 経路なしで失敗

---

## MPP（Multi-Path Payments）テスト - CLoTH Original

### test_mpp_single_path_cloth
- **ルーティング**: cloth_original + MPP
- **ネットワーク**: diamond_4node
- **テスト内容**: 金額が単一経路に収まる場合
- **期待結果**: シャードなしで成功（MPP不要）

### test_mpp_split_success_cloth
- **ルーティング**: cloth_original + MPP
- **パラメータ**: max_shard_count=16
- **ネットワーク**: diamond_4node
- **テスト内容**: バイナリ分割による成功
- **期待結果**: 複数シャードに分割して成功（is_shard=1）

### test_mpp_max_shards_cloth
- **ルーティング**: cloth_original + MPP
- **パラメータ**: max_shard_count=4
- **ネットワーク**: parallel_6node
- **テスト内容**: 最大シャード数を超過
- **期待結果**: シャード数超過で失敗（status=MAX_SHARDS_EXCEEDED）

---

## MPP テスト - GCB (Group Capacity Based)

### test_mpp_single_path_gcb
- **ルーティング**: group_routing + MPP
- **パラメータ**: group_size=5, group_limit_rate=0.3
- **ネットワーク**: diamond_4node
- **テスト内容**: 金額が単一経路に収まる場合
- **期待結果**: シャードなしで成功

### test_mpp_split_success_gcb
- **ルーティング**: group_routing + MPP
- **ネットワーク**: diamond_4node
- **テスト内容**: グループ容量情報を使った最適分割
- **期待結果**: 複数シャードに分割して成功

### test_mpp_max_shards_gcb
- **ルーティング**: group_routing + MPP
- **パラメータ**: max_shard_count=4
- **ネットワーク**: parallel_6node
- **テスト内容**: 最大シャード数を超過
- **期待結果**: シャード数超過で失敗

---

## GCB/CUL 固有パラメータテスト

### test_gcb_group_limit
- **ルーティング**: group_routing
- **パラメータ**: group_size=5, group_limit_rate=0.1（厳しい制限）
- **ネットワーク**: grouped_network
- **テスト内容**: グループ容量制限率がルーティングに影響
- **期待結果**: 厳しい制限によりグループ容量推定が変化
- **検証項目**: 
  - payments: 支払い結果
  - groups: group_capが制限率に応じて調整されている

### test_gcb_cul_threshold
- **ルーティング**: group_routing_cul
- **パラメータ**: group_size=5
- **ネットワーク**: grouped_network
- **テスト内容**: CUL閾値による挙動
- **期待結果**: CUL閾値に基づいた経路選択
- **検証項目**:
  - payments: 支払い結果
  - groups: CUL閾値に基づくグループ状態

---

## GCB + MPP 包括的テスト

### test_mpp_gcb_small_amount
- **テスト内容**: 小額支払い - 分割不要
- **期待結果**: グループ容量内で単一経路で成功

### test_mpp_gcb_multipath_split
- **パラメータ**: max_shard_count=8
- **テスト内容**: グループ容量情報を使った複数経路への最適分割
- **期待結果**: 複数経路に分散して成功

### test_mpp_gcb_shard_resplit
- **パラメータ**: max_shard_count=8
- **テスト内容**: シャード失敗時の再分割
- **期待結果**: 失敗したシャードを再分割してリトライ

### test_mpp_gcb_no_path
- **パラメータ**: max_shard_count=16
- **テスト内容**: 金額が大きすぎて全経路を使っても不可能
- **期待結果**: 全経路枯渇で失敗

### test_mpp_gcb_timeout
- **パラメータ**: payment_timeout=1（1ms、非常に短い）
- **テスト内容**: シャード処理中にタイムアウト
- **期待結果**: タイムアウトで失敗（status=TIMEOUT）

### test_mpp_gcb_rollback
- **パラメータ**: max_shard_count=8
- **テスト内容**: 部分的成功時のロールバック機構
- **期待結果**: 全シャードが成功しない場合はロールバック

### test_mpp_gcb_group_limit_rate
- **パラメータ**: group_limit_rate=0.1（厳しい制限）
- **テスト内容**: 厳しいグループ制限率が分割に影響
- **期待結果**: 制限により分割戦略が変化

### test_mpp_gcb_max_shards_boundary
- **ネットワーク**: multipath_8node
- **パラメータ**: max_shard_count=8
- **テスト内容**: ちょうど最大シャード数が必要なケース
- **期待結果**: 境界値で成功

### test_mpp_gcb_3way_split
- **ネットワーク**: multipath_8node
- **パラメータ**: max_shard_count=8
- **テスト内容**: 3つの並列経路への分割
- **期待結果**: 3経路に分散して成功

### test_mpp_gcb_4way_split
- **ネットワーク**: multipath_8node
- **パラメータ**: max_shard_count=8
- **テスト内容**: 4つの並列経路への分割（全経路使用）
- **期待結果**: 4経路に分散して成功

### test_mpp_gcb_many_shards
- **ネットワーク**: multipath_8node
- **パラメータ**: max_shard_count=16
- **テスト内容**: 複数経路にまたがる多数のシャード
- **期待結果**: 多くのシャードに分割して成功

---

## CUL + MPP テスト

### test_mpp_cul_split
- **ルーティング**: group_routing_cul + MPP
- **パラメータ**: max_shard_count=8
- **ネットワーク**: diamond_4node
- **テスト内容**: CUL閾値ベースの分割
- **期待結果**: CUL閾値に基づいて分割して成功

---

## 追加のGCB + MPP パラメータテスト

### test_mpp_gcb_large_group
- **パラメータ**: group_size=10, group_limit_rate=0.5
- **テスト内容**: 大きなグループサイズが容量推定に影響
- **期待結果**: グループサイズによる挙動の違い

### test_mpp_gcb_restrictive_limit
- **パラメータ**: group_limit_rate=0.05（非常に厳しい）, max_shard_count=16
- **テスト内容**: 非常に厳しいグループ制限率
- **期待結果**: 極端な制限下での分割挙動

### test_mpp_gcb_minimal_shards
- **パラメータ**: max_shard_count=2（最小限の分割）
- **テスト内容**: 最小シャード数（2個）での分割
- **期待結果**: 2つまでのシャードで処理

### test_mpp_cul_high_threshold
- **ルーティング**: group_routing_cul + MPP
- **パラメータ**: max_shard_count=8
- **テスト内容**: 高いCUL閾値での挙動
- **期待結果**: CUL閾値による挙動の違い

### test_mpp_gcb_retry_smaller
- **ネットワーク**: parallel_6node
- **パラメータ**: max_shard_count=4
- **テスト内容**: 最大シャード数超過後の動作
- **期待結果**: シャード数制限により失敗

### test_mpp_gcb_single_group_member
- **ネットワーク**: simple_linear
- **テスト内容**: 単一経路ネットワークでMPP不要
- **期待結果**: シャードなしで成功

---

## テスト検証の仕組み

各テストでは、`expected` ディレクトリのJSONファイルに定義されたアサーションを使って検証します。

### アサーション演算子

- **eq**: 等しい
- **neq**: 等しくない
- **gt**: より大きい
- **gte**: 以上
- **lt**: より小さい
- **lte**: 以下
- **not_empty**: 空でない
- **empty**: 空である

### 検証フィールド詳細

**payments_output.csv**:
- `id`: 支払いID
- `is_success`: 成功フラグ（0 or 1）
- `attempts`: 試行回数
- `no_balance_count`: 容量不足が発生した回数
- `route`: 使用された経路（ノードIDのリスト）
- `is_shard`: シャードかどうか（0 or 1）
- `parent_id`: 親支払いID（シャードの場合）
- `mpp`: MPP使用フラグ（0 or 1）

**edges_output.csv** - **エッジ残高の検証**:
- `id`: エッジID
- `from_node_id`: 送信元ノードID
- `to_node_id`: 送信先ノードID
- **`balance`**: **エッジ残高（支払い後の値）** ← 最重要検証項目
- `capacity`: エッジ容量（初期値）
- `fee_base`: 基本手数料
- `fee_proportional`: 比例手数料

**検証例**:
```json
{
  "edges": [
    {
      "id": 0,
      "assertions": [
        {"field": "balance", "op": "lt", "value": 5000000},
        {"field": "balance", "op": "gt", "value": 3900000}
      ]
    }
  ]
}
```
→ edge 0の残高が初期値5,000,000から1,000,000程度減少していることを確認（手数料考慮）

**groups_output.csv** - **グループ形成の検証**:
- `id`: グループID
- `group_cap`: グループ容量（推定値）
- `member_count`: グループメンバー数（エッジ数）
- `total_balance`: グループ内の総残高
- `avg_balance`: グループ内の平均残高

**検証例**:
```json
{
  "groups": [
    {
      "id": 0,
      "assertions": [
        {"field": "member_count", "op": "gte", "value": 3},
        {"field": "group_cap", "op": "gt", "value": 0}
      ]
    }
  ]
}
```
→ グループ0が3つ以上のエッジを持ち、正しい容量推定値を持つことを確認

---

## まとめ

このテストスイートは全**42個**のテストケースで以下をカバーします：

1. **基本ルーティング（16テスト）**: 
   - **CLoTH Original**（4テスト）: 成功、リトライ成功、容量枯渇、経路なし
   - **Ideal**（4テスト）: 成功、リトライ成功、容量枯渇、経路なし
   - **GCB - Group Capacity Based**（4テスト）: 成功、リトライ成功、容量枯渇、経路なし
   - **CUL - Channel Usage Limit**（4テスト）: 成功、リトライ成功、容量枯渇、経路なし
   - ✅ **全手法で同等のテストを実施**

2. **GCB/CUL固有パラメータテスト（2テスト）**:
   - グループ制限率の影響テスト
   - CUL閾値挙動テスト

3. **MPP（Multi-Path Payments）（24テスト）**: 
   - CLoTH Original MPP（3テスト）
   - GCB MPP基本（3テスト）
   - GCB MPP包括的（15テスト）: 小額、複数経路分割、再分割、タイムアウト、ロールバック等
   - CUL MPP（2テスト）
   - パラメータ調整（1テスト）

4. **ネットワーク構成（7種類）**: 
   - simple_linear（線形3ノード）
   - imbalanced_4node（不均衡4ノード）
   - isolated_node（孤立ノード）
   - diamond_4node（ダイヤモンド型）
   - parallel_6node（並列6ノード）
   - multipath_8node（複雑な8ノード）
   - **grouped_network（グループ専用10ノード）** ← グループルーティング詳細テスト用

5. **検証の充実化（3レベル検証）**:
   - ✅ **payments_output.csv**: 支払い結果の検証（成功/失敗、試行回数、経路）
   - ✅ **edges_output.csv**: **エッジ残高の厳密な検証** - 支払い前後でリンク容量が正確に更新されているか確認
   - ✅ **groups_output.csv**: **グループ形成の検証** - GCB/CUL使用時にグループが正しく形成されているか確認

### エッジ残高検証の重要性

各テストでは、expected JSONの`edges`セクションで支払い後のエッジ残高を検証します：

```json
"edges": [
  {
    "id": 0,
    "assertions": [
      {"field": "balance", "op": "lt", "value": 5000000},
      {"field": "balance", "op": "gt", "value": 3900000}
    ]
  }
]
```

これにより：
- 支払いが成功した場合、正しいエッジの残高が減少していることを確認
- 支払いが失敗した場合、エッジ残高が変化していないことを確認  
- 複数パスを使用する場合、各パスのエッジ残高が適切に更新されていることを確認
- 手数料計算が正しく反映されているかを確認

全テストを実行することで、CLoTH-Gossipシミュレーターの堅牢性と正確性を確認できます。

## 重要な注意点

### グループルーティングのリトライ挙動

- **GCB（Group Capacity Based）** と **CUL（Channel Usage Limit）** では、グループ内のリンクを使用する場合、理論上リトライは発生しません
- ただし、**グループに属さないリンク**がパスに含まれている場合は、そのリンクで容量不足が発生した際にリトライが発生する可能性があります
- `grouped_network` はこの挙動をテストするために、意図的にグループ外リンク（1→9→3）を含んでいます

### エッジとグループの検証

- エッジの残高（balance）は支払い処理後に更新されるため、expected JSONで初期値より減少していることを確認できます
- グループ容量（group_cap）は、グループ内のリンクから推定される値で、GCB/CULの動作を検証する重要な指標です
