#!/usr/bin/env python3
"""
cloth_originalで成功したpaymentに絞って、同じ設定のgroup_routing_culとidealとのfee比較を行うスクリプト
MPP有効時のfee削減効果を測定する

使用方法:
  1. ディレクトリから比較を実行:
     python3 compare_fee_mpp_successful.py <parent_dir1> [parent_dir2] ...

     例: python3 compare_fee_mpp_successful.py /Volumes/kohei/result
     例: python3 compare_fee_mpp_successful.py /Volumes/kohei/result1 /Volumes/kohei/result2

     動作:
     - 指定されたparent_dir下のディレクトリを探索
     - summary.csvがあるディレクトリのみを対象（シミュレーション完了済み）
     - すべてのoutput_dirから比較結果を統合して平均を計算

     出力:
     - fee_comparison_multi_all.csv: 全output_dirを統合した詳細比較結果
     - fee_scatter_multi_all.png: 統合データの送金額ごとの平均fee比較グラフ

  2. CSVファイルからグラフを作成:
     python3 compare_fee_mpp_successful.py --csv <csv_file> [output_png]

     例: python3 compare_fee_mpp_successful.py --csv fee_comparison_multi_all.csv

統計情報:
  - cloth_originalで成功したMPP paymentのみを対象
  - group_routing_culとidealで同じpayment IDが成功したものを比較
  - fee削減量、削減率、削減状況の統計を計算
"""

import csv
import os
import sys
import json
import re
from collections import defaultdict

csv.field_size_limit(200_000_000)

# matplotlibとnumpyのインポート（エラーハンドリング付き）
try:
    # numpyの互換性問題を回避: VisibleDeprecationWarningを事前に定義
    import warnings
    warnings.filterwarnings('ignore')

    # numpyを先にインポートして、VisibleDeprecationWarningを追加
    import numpy
    if not hasattr(numpy, 'VisibleDeprecationWarning'):
        import types
        numpy.VisibleDeprecationWarning = type('VisibleDeprecationWarning', (Warning,), {})

    # matplotlibのインポート前にnumpyモジュールにVisibleDeprecationWarningを追加
    import sys
    if 'numpy' in sys.modules:
        numpy_module = sys.modules['numpy']
        if not hasattr(numpy_module, 'VisibleDeprecationWarning'):
            numpy_module.VisibleDeprecationWarning = type('VisibleDeprecationWarning', (Warning,), {})

    import matplotlib
    matplotlib.use('Agg')  # GUIバックエンドを使わない
    import matplotlib.pyplot as plt
    HAS_MATPLOTLIB = True
except Exception as e:
    HAS_MATPLOTLIB = False
    print(f"警告: matplotlibのインポートに失敗しました: {e}")
    import traceback
    traceback.print_exc()
    print("\nグラフを作成するには、以下のコマンドでインストールしてください:")
    print("  pip install --break-system-packages --upgrade numpy matplotlib")


def calculate_mpp_total_fee(payment, payments_by_id):
    """MPP paymentの総feeを計算（シャードのfeeを再帰的に合計）"""
    shards_str = payment.get("shards", "")
    if not shards_str:
        # リーフシャードまたは非MPP payment
        fee_str = payment.get("total_fee", "")
        return int(fee_str) if fee_str else 0

    total_fee = 0
    shard_ids = shards_str.split('-')
    for shard_id_str in shard_ids:
        shard_id = int(shard_id_str)
        if shard_id in payments_by_id:
            shard = payments_by_id[shard_id]
            total_fee += calculate_mpp_total_fee(shard, payments_by_id)
    return total_fee


def load_payments(output_dir):
    """payments_output.csvを読み込み、payment IDでマッピング"""
    payments_by_id = {}
    root_payments = []

    payments_file = os.path.join(output_dir, 'payments_output.csv')
    if not os.path.exists(payments_file):
        return {}, []

    # ファイルがディレクトリの場合はスキップ
    if os.path.isdir(payments_file):
        print(f"警告: {payments_file} はディレクトリです（スキップ）")
        return {}, []

    try:
        with open(payments_file, 'r') as f:
            payments = list(csv.DictReader(f))

            # 全paymentをIDでマッピング
            for p in payments:
                payment_id = int(p["id"])
                payments_by_id[payment_id] = p

            # root paymentのみを抽出（シャードではない）
            root_payments = [
                p for p in payments
                if p.get("is_shard", "0") == "0" or p.get("parent_payment_id", "-1") == "-1"
            ]
    except Exception as e:
        print(f"警告: {payments_file} の読み込み中にエラーが発生しました: {e}")
        return {}, []

    return payments_by_id, root_payments


def get_payment_fee(payment, payments_by_id):
    """paymentのfeeを取得（MPPの場合はシャードのfeeを合計）"""
    mpp_triggered = (payment.get("mpp", "0") == "1")
    shards = payment.get("shards", "")
    is_mpp = mpp_triggered or bool(shards)

    if is_mpp:
        return calculate_mpp_total_fee(payment, payments_by_id)
    else:
        fee_str = payment.get("total_fee", "")
        return int(fee_str) if fee_str else 0


def is_mpp_payment(payment):
    """MPPが使用されたpaymentかどうかを判定"""
    mpp_triggered = (payment.get("mpp", "0") == "1")
    shards = payment.get("shards", "")
    return mpp_triggered or bool(shards)


def compare_fees(cloth_original_dir, group_routing_cul_dir, ideal_dir=None):
    """cloth_original、group_routing_cul、idealのfeeを比較"""

    try:
        # paymentデータを読み込み
        cloth_payments_by_id, cloth_root_payments = load_payments(cloth_original_dir)
        group_payments_by_id, group_root_payments = load_payments(group_routing_cul_dir)

        if not cloth_root_payments or not group_root_payments:
            print(f"警告: データが見つかりません")
            print(f"  cloth_original: {len(cloth_root_payments)} payments")
            print(f"  group_routing_cul: {len(group_root_payments)} payments")
            return None

        ideal_payments_by_id = {}
        if ideal_dir:
            ideal_payments_by_id, _ = load_payments(ideal_dir)

        # cloth_originalで成功したpaymentを抽出（MPP有効時のみ）
        cloth_successful_mpp = []
        for payment in cloth_root_payments:
            is_success = (payment["is_success"] == "1")
            if is_success and is_mpp_payment(payment):
                cloth_successful_mpp.append(payment)

        print(f"cloth_originalで成功したMPP payment数: {len(cloth_successful_mpp)}")

        # 同じpayment IDでgroup_routing_culとidealのfeeを取得
        comparison_results = []
        matched_count = 0
        not_found_count = 0

        for cloth_payment in cloth_successful_mpp:
            payment_id = int(cloth_payment["id"])
            cloth_fee = get_payment_fee(cloth_payment, cloth_payments_by_id)

            if payment_id not in group_payments_by_id:
                not_found_count += 1
                continue

            group_payment = group_payments_by_id[payment_id]
            group_fee = get_payment_fee(group_payment, group_payments_by_id)

            amount = int(cloth_payment["amount"])
            fee_reduction = cloth_fee - group_fee
            fee_reduction_rate = (fee_reduction / cloth_fee * 100) if cloth_fee > 0 else 0

            result = {
                "payment_id": payment_id,
                "sender_id": cloth_payment["sender_id"],
                "receiver_id": cloth_payment["receiver_id"],
                "amount": amount,
                "cloth_fee": cloth_fee,
                "group_fee": group_fee,
                "ideal_fee": None,
                "fee_reduction": fee_reduction,
                "fee_reduction_rate": fee_reduction_rate,
                "cloth_is_success": cloth_payment["is_success"] == "1",
                "group_is_success": group_payment["is_success"] == "1",
                "ideal_is_success": None,
                "cloth_mpp": is_mpp_payment(cloth_payment),
                "group_mpp": is_mpp_payment(group_payment),
                "ideal_mpp": None,
            }

            if ideal_payments_by_id and payment_id in ideal_payments_by_id:
                ideal_payment = ideal_payments_by_id[payment_id]
                result["ideal_fee"] = get_payment_fee(ideal_payment, ideal_payments_by_id)
                result["ideal_is_success"] = ideal_payment["is_success"] == "1"
                result["ideal_mpp"] = is_mpp_payment(ideal_payment)

            comparison_results.append(result)
            matched_count += 1

        print(f"マッチしたpayment数: {matched_count}")
        print(f"見つからなかったpayment数: {not_found_count}")

        return comparison_results
    except Exception as e:
        print(f"警告: fee比較中にエラーが発生しました: {e}")
        print(f"  cloth_original: {cloth_original_dir}")
        print(f"  group_routing_cul: {group_routing_cul_dir}")
        return None


def _print_fee_stats(label, fees):
    print(f"\n【{label}のfee】")
    print(f"  平均: {sum(fees) / len(fees):.2f} satoshi")
    print(f"  中央値: {sorted(fees)[len(fees) // 2]} satoshi")
    print(f"  最小値: {min(fees)} satoshi")
    print(f"  最大値: {max(fees)} satoshi")
    print(f"  合計: {sum(fees)} satoshi")


def print_statistics(comparison_results):
    """統計情報を表示"""
    if not comparison_results:
        print("比較結果がありません")
        return

    # cloth + group 両方成功したpaymentを対象にする
    successful_comparisons = [
        r for r in comparison_results
        if r["cloth_is_success"] and r["group_is_success"]
    ]

    if not successful_comparisons:
        print("cloth_originalとgroup_routing_culで成功したpaymentがありません")
        return

    print("\n" + "="*80)
    print("統計情報（cloth_originalで成功したMPP paymentのみを対象）")
    print("="*80)

    cloth_fees = [r["cloth_fee"] for r in successful_comparisons]
    group_fees = [r["group_fee"] for r in successful_comparisons]
    fee_reductions = [r["fee_reduction"] for r in successful_comparisons]
    fee_reduction_rates = [r["fee_reduction_rate"] for r in successful_comparisons]

    print(f"\n比較対象payment数: {len(successful_comparisons)}")

    _print_fee_stats("cloth_original (LN)", cloth_fees)
    _print_fee_stats("group_routing_cul (GCB)", group_fees)

    ideal_comps = [r for r in successful_comparisons if r["ideal_is_success"]]
    if ideal_comps:
        _print_fee_stats("ideal", [r["ideal_fee"] for r in ideal_comps])

    print(f"\n【fee削減量 (LN→GCB)】")
    print(f"  平均削減量: {sum(fee_reductions) / len(fee_reductions):.2f} satoshi")
    print(f"  中央値削減量: {sorted(fee_reductions)[len(fee_reductions) // 2]} satoshi")
    print(f"  最小削減量: {min(fee_reductions)} satoshi")
    print(f"  最大削減量: {max(fee_reductions)} satoshi")
    print(f"  合計削減量: {sum(fee_reductions)} satoshi")

    print(f"\n【fee削減率 (LN→GCB)】")
    print(f"  平均削減率: {sum(fee_reduction_rates) / len(fee_reduction_rates):.2f}%")
    print(f"  中央値削減率: {sorted(fee_reduction_rates)[len(fee_reduction_rates) // 2]:.2f}%")
    print(f"  最小削減率: {min(fee_reduction_rates):.2f}%")
    print(f"  最大削減率: {max(fee_reduction_rates):.2f}%")

    # 削減できたpayment数
    reduced_count = sum(1 for r in fee_reductions if r > 0)
    increased_count = sum(1 for r in fee_reductions if r < 0)
    same_count = sum(1 for r in fee_reductions if r == 0)

    print(f"\n【削減状況 (LN→GCB)】")
    print(f"  feeが削減されたpayment数: {reduced_count} ({reduced_count/len(successful_comparisons)*100:.1f}%)")
    print(f"  feeが増加したpayment数: {increased_count} ({increased_count/len(successful_comparisons)*100:.1f}%)")
    print(f"  feeが同じpayment数: {same_count} ({same_count/len(successful_comparisons)*100:.1f}%)")


def save_detailed_results(comparison_results, output_file):
    """詳細な比較結果をCSVファイルに保存（すべての結果を保存）"""
    if not comparison_results:
        return

    all_results = []
    for r in comparison_results:
        result_row = {
            "payment_id": r["payment_id"],
            "sender_id": r["sender_id"],
            "receiver_id": r["receiver_id"],
            "amount": r["amount"],
            "cloth_fee": r["cloth_fee"],
            "group_fee": r["group_fee"],
            "ideal_fee": r["ideal_fee"] if r["ideal_fee"] is not None else "",
            "cloth_is_success": r["cloth_is_success"],
            "group_is_success": r["group_is_success"],
            "ideal_is_success": r["ideal_is_success"] if r["ideal_is_success"] is not None else "",
            "cloth_mpp": r["cloth_mpp"],
            "group_mpp": r["group_mpp"],
            "ideal_mpp": r["ideal_mpp"] if r["ideal_mpp"] is not None else "",
        }

        # fee_reductionとfee_reduction_rateはcloth+group両方成功した場合のみ計算
        if r["cloth_is_success"] and r["group_is_success"]:
            result_row["fee_reduction"] = r["fee_reduction"]
            result_row["fee_reduction_rate"] = r["fee_reduction_rate"]
        else:
            result_row["fee_reduction"] = ""
            result_row["fee_reduction_rate"] = ""

        all_results.append(result_row)

    if not all_results:
        print("保存するデータがありません")
        return

    fieldnames = [
        "payment_id", "sender_id", "receiver_id", "amount",
        "cloth_fee", "group_fee", "ideal_fee",
        "fee_reduction", "fee_reduction_rate",
        "cloth_is_success", "group_is_success", "ideal_is_success",
        "cloth_mpp", "group_mpp", "ideal_mpp",
    ]

    with open(output_file, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(all_results)

    total_count = len(all_results)
    cloth_success = sum(1 for r in all_results if r["cloth_is_success"])
    group_success = sum(1 for r in all_results if r["group_is_success"])
    ideal_success = sum(1 for r in all_results if r["ideal_is_success"] is True)

    print(f"\n詳細結果を保存しました: {output_file}")
    print(f"  総payment数: {total_count}")
    print(f"  cloth_original成功: {cloth_success}")
    print(f"  group_routing_cul成功: {group_success}")
    print(f"  ideal成功: {ideal_success}")


def create_fee_scatter_plot(comparison_results, output_file, title_suffix=""):
    """送金額ごとの平均feeをプロット（全て成功したもののみ）"""
    if not HAS_MATPLOTLIB:
        print("matplotlibが利用できないため、グラフは作成されません")
        return

    if not comparison_results:
        return

    successful_comparisons = [
        r for r in comparison_results
        if r["cloth_is_success"] and r["group_is_success"]
    ]

    if not successful_comparisons:
        print("グラフ作成用のデータがありません")
        return

    create_average_fee_plot(successful_comparisons, output_file,
                            f'Average Fee Comparison by Payment Amount{title_suffix}')


def load_comparison_csv(csv_file):
    """比較結果のCSVファイルを読み込む（全て成功したもののみ）"""
    results = []
    with open(csv_file, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            cloth_success = row.get('cloth_is_success', '').lower() == 'true'
            group_success = row.get('group_is_success', '').lower() == 'true'

            if cloth_success and group_success:
                entry = {
                    'amount': int(row['amount']),
                    'cloth_fee': int(row['cloth_fee']),
                    'group_fee': int(row['group_fee']),
                    'ideal_fee': None,
                    'ideal_is_success': None,
                    'payment_id': int(row['payment_id']),
                }
                ideal_fee_str = row.get('ideal_fee', '')
                ideal_success_str = row.get('ideal_is_success', '').lower()
                if ideal_fee_str and ideal_success_str == 'true':
                    entry['ideal_fee'] = int(ideal_fee_str)
                    entry['ideal_is_success'] = True
                results.append(entry)
    return results


def create_average_fee_plot(results, output_file, title=""):
    """送金額ごとの平均feeをプロット（LN, GCB, idealの3本線）"""
    if not HAS_MATPLOTLIB:
        print("matplotlibが利用できないため、グラフは作成されません")
        return False

    if not results:
        print("データがありません")
        return False

    import numpy as np

    amounts = np.array([r['amount'] / 1000 for r in results])  # millisatoshi -> satoshi
    cloth_fees = np.array([r['cloth_fee'] for r in results])
    group_fees = np.array([r['group_fee'] for r in results])

    # idealデータがある行のみ（成功済み）
    ideal_mask_bool = np.array([
        r.get('ideal_fee') is not None and r.get('ideal_is_success') is True
        for r in results
    ])
    ideal_fees_raw = np.array([
        r['ideal_fee'] if (r.get('ideal_fee') is not None and r.get('ideal_is_success') is True) else 0
        for r in results
    ])

    # 送金額を線形スケールでビンに分ける（10000〜100000 satoshi、10000刻み）
    bin_centers_target = np.arange(10000, 100001, 10000)  # [10000, 20000, ..., 100000]
    bins = np.arange(5000, 105001, 10000)  # [5000, 15000, 25000, ..., 105000]

    bin_indices = np.digitize(amounts, bins)
    bin_centers = []
    cloth_avg_fees = []
    group_avg_fees = []
    ideal_avg_fees = []
    bin_counts = []

    for i in range(1, len(bins)):
        mask = (bin_indices == i)
        if np.sum(mask) > 0:
            bin_centers.append(bin_centers_target[i-1])
            cloth_avg_fees.append(np.mean(cloth_fees[mask]))
            group_avg_fees.append(np.mean(group_fees[mask]))
            ideal_bin_mask = mask & ideal_mask_bool
            ideal_avg_fees.append(np.mean(ideal_fees_raw[ideal_bin_mask]) if np.sum(ideal_bin_mask) > 0 else None)
            bin_counts.append(np.sum(mask))

    bin_centers = np.array(bin_centers)
    cloth_avg_fees = np.array(cloth_avg_fees)
    group_avg_fees = np.array(group_avg_fees)

    # グラフを作成
    fig, ax = plt.subplots(figsize=(12, 8))

    ax.plot(bin_centers, cloth_avg_fees, 'o-', label='CLoTH-Original / LN (avg)',
            color='blue', linewidth=2, markersize=6)
    ax.plot(bin_centers, group_avg_fees, 's-', label='GROUP_ROUTING_CUL / GCB (avg)',
            color='red', linewidth=2, markersize=6)

    ideal_x = [bin_centers[j] for j, v in enumerate(ideal_avg_fees) if v is not None]
    ideal_y = [v for v in ideal_avg_fees if v is not None]
    if ideal_x:
        ax.plot(ideal_x, ideal_y, '^-', label='Ideal (avg)',
                color='green', linewidth=2, markersize=6)

    # 軸の設定
    ax.set_xlabel('Payment Amount (satoshi)', fontsize=12)
    ax.set_ylabel('Average Fee (satoshi)', fontsize=12)
    if title:
        ax.set_title(title, fontsize=14, fontweight='bold')
    else:
        ax.set_title('Average Fee Comparison by Payment Amount', fontsize=14, fontweight='bold')
    ax.legend(fontsize=11, loc='upper left')
    ax.grid(True, alpha=0.3, linestyle='--')

    ax.set_xlim(10000, 100000)
    ax.set_xticks(np.arange(10000, 100001, 10000))
    ax.set_ylim(bottom=0)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()

    print(f"グラフを保存しました: {output_file}")
    print(f"  ビン数: {len(bin_centers)}")
    if bin_counts:
        print(f"  各ビンのサンプル数: 最小={min(bin_counts)}, 最大={max(bin_counts)}, 平均={np.mean(bin_counts):.1f}")
    return True


def find_output_dirs_with_summary(parent_dir):
    """
    parent_dir下のディレクトリで、summary.csvがあるものを探す
    日付時刻形式に限らず、summary.csvが存在すれば有効とする
    """
    output_dirs = []

    if not os.path.isdir(parent_dir):
        return output_dirs

    try:
        for item in os.listdir(parent_dir):
            item_path = os.path.join(parent_dir, item)

            # ディレクトリで、summary.csvがある
            if (os.path.isdir(item_path) and
                os.path.isfile(os.path.join(item_path, 'summary.csv'))):
                output_dirs.append(item_path)
    except Exception as e:
        print(f"警告: {parent_dir} の読み込み中にエラーが発生しました: {e}")

    # ソート
    output_dirs.sort()
    return output_dirs


def find_matching_dirs(root_dir):
    """同じ設定のcloth_original、group_routing_cul、idealのディレクトリを探す"""
    cloth_dirs = {}
    group_dirs = {}
    ideal_dirs = {}

    try:
        for subdir, _, files in os.walk(root_dir):
            if 'cloth_input.txt' in files:
                cloth_input_file = os.path.join(subdir, 'cloth_input.txt')
                try:
                    with open(cloth_input_file, 'r') as f:
                        params = {}
                        for line in f:
                            line = line.strip()
                            if line and not line.startswith('#'):
                                if '=' in line:
                                    key, value = line.split('=', 1)
                                    params[key.strip()] = value.strip()

                        routing_method = params.get('routing_method', '')
                        # 設定を識別するキーを作成（routing_method以外の主要パラメータ）
                        key_params = {
                            'average_payment_amount': params.get('average_payment_amount', ''),
                            'seed': params.get('seed', ''),
                            'mpp': params.get('mpp', ''),
                        }
                        key = tuple(sorted(key_params.items()))

                        if routing_method == 'cloth_original':
                            cloth_dirs[key] = subdir + '/'
                        elif routing_method == 'group_routing_cul':
                            group_dirs[key] = subdir + '/'
                        elif routing_method == 'ideal':
                            ideal_dirs[key] = subdir + '/'
                except Exception as e:
                    print(f"警告: {cloth_input_file} の読み込み中にエラーが発生しました: {e}")
                    continue
    except Exception as e:
        print(f"警告: {root_dir} の探索中にエラーが発生しました: {e}")

    # cloth + group が揃っているものを返す（idealはあれば含める）
    matching_pairs = []
    for key in cloth_dirs:
        if key in group_dirs:
            matching_pairs.append((cloth_dirs[key], group_dirs[key], ideal_dirs.get(key), key))

    return matching_pairs


def main():
    if len(sys.argv) < 2:
        print("使用方法:")
        print("  1. ディレクトリから比較を実行:")
        print("     python3 compare_fee_mpp_successful.py <parent_dir1> [parent_dir2] ...")
        print("  2. CSVファイルからグラフを作成:")
        print("     python3 compare_fee_mpp_successful.py --csv <csv_file> [output_png]")
        print("\n例:")
        print("  python3 compare_fee_mpp_successful.py /Volumes/kohei/result")
        print("  python3 compare_fee_mpp_successful.py --csv fee_comparison_multi_all.csv")
        sys.exit(1)

    # CSVファイルからグラフを作成するモード
    if sys.argv[1] == '--csv':
        if len(sys.argv) < 3:
            print("エラー: CSVファイルを指定してください")
            print("使用方法: python3 compare_fee_mpp_successful.py --csv <csv_file> [output_png]")
            sys.exit(1)

        csv_file = sys.argv[2]
        if not os.path.exists(csv_file):
            print(f"エラー: ファイルが見つかりません: {csv_file}")
            sys.exit(1)

        # 出力ファイル名を決定
        if len(sys.argv) >= 4:
            output_file = sys.argv[3]
        else:
            # CSVファイル名から自動生成
            base_name = os.path.splitext(csv_file)[0]
            output_file = f"{base_name}_scatter.png"

        # CSVファイルを読み込む
        print(f"CSVファイルを読み込み中: {csv_file}")
        results = load_comparison_csv(csv_file)
        print(f"読み込んだデータ数: {len(results)}")

        # グラフを作成
        title = f"Average Fee Comparison by Payment Amount"
        create_average_fee_plot(results, output_file, title)
        return

    parent_dirs = sys.argv[1:]

    # 各ディレクトリの存在確認
    for parent_dir in parent_dirs:
        if not os.path.isdir(parent_dir):
            print(f"エラー: ディレクトリが見つかりません: {parent_dir}")
            sys.exit(1)

    # 全ての親ディレクトリからoutput_dirsを収集
    all_output_dirs = []
    for parent_dir in parent_dirs:
        output_dirs = find_output_dirs_with_summary(parent_dir)
        print(f"{parent_dir} から {len(output_dirs)} 個のoutput_dirを発見")
        all_output_dirs.extend(output_dirs)

    if not all_output_dirs:
        print(f"エラー: 指定されたディレクトリ下にsummary.csvがあるディレクトリが見つかりません")
        sys.exit(1)

    print(f"\n見つかった総output_dir数: {len(all_output_dirs)}")
    for d in all_output_dirs:
        print(f"  - {d}")
    print()

    # 各output_dirから比較結果を収集
    all_results = []
    total_matching_pairs = 0

    for output_dir in all_output_dirs:
        print(f"\n{'='*80}")
        print(f"処理中: {output_dir}")
        print(f"{'='*80}\n")

        # マッチするディレクトリペアを探す
        matching_pairs = find_matching_dirs(output_dir)

        if not matching_pairs:
            print(f"  警告: マッチするディレクトリが見つかりません（スキップ）")
            continue

        print(f"  見つかったマッチング数: {len(matching_pairs)}")
        total_matching_pairs += len(matching_pairs)

        # 各セットについて比較を実行
        for cloth_dir, group_dir, ideal_dir, key_params in matching_pairs:
            comparison_results = compare_fees(cloth_dir, group_dir, ideal_dir)

            if comparison_results:
                all_results.extend(comparison_results)

    if not all_results:
        print("\n比較結果が得られませんでした")
        sys.exit(1)

    # 統合結果を出力
    print(f"\n\n{'='*80}")
    print(f"統合統計（{len(parent_dirs)}個の親ディレクトリから{len(all_output_dirs)}個のoutput_dir、{total_matching_pairs}個のセットを統合）")
    print(f"{'='*80}")
    print_statistics(all_results)

    # 統合結果を保存（最初の親ディレクトリに出力）
    output_base_dir = parent_dirs[0]
    output_file = os.path.join(output_base_dir, "fee_comparison_multi_all.csv")
    save_detailed_results(all_results, output_file)

    # 統合グラフを作成
    graph_output = os.path.join(output_base_dir, "fee_scatter_multi_all.png")
    create_fee_scatter_plot(all_results, graph_output, f" (Aggregated: {len(all_output_dirs)} output_dirs)")


if __name__ == "__main__":
    main()
