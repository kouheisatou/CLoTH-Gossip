import csv
import json
import os
import sys

import numpy as np
from matplotlib import pyplot as plt

if len(sys.argv) < 1:
    print("python3 analyze_output_and_summarize.py <output_dir>")
    exit(1)
output_root_dir_name = sys.argv[1]


def find_output_dirs(root_dir):
    files = []
    for subdir, _, files_in_subdir in os.walk(root_dir):
        for f in files_in_subdir:
            if f == "cloth_input.txt":
                files.append(subdir + "/")
    return files


def save_histogram(data: list, x_label: str, y_label: str, filepath: str, bins):
    fig, ax = plt.subplots()
    ax.hist(data, bins=bins)
    ax.set_xlabel(x_label)
    ax.set_ylabel(y_label)
    ax.axvline(x=np.mean(data), color='green', linestyle='--', linewidth=1, label=f'Mean: {np.mean(data):.2f}')
    ax.axvline(x=np.median(data), color='red', linestyle='--', linewidth=1, label=f'Median: {np.median(data):.2f}')
    fig.legend()
    fig.savefig(filepath)
    plt.clf()
    plt.close()


def save_scatter(data_x: list, data_y: list, x_label: str, y_label: str, filepath: str):
    fig, ax = plt.subplots()
    ax.scatter(data_x, data_y)
    ax.set_xlabel(x_label)
    ax.set_ylabel(y_label)
    ax.set_xscale('log')
    ax.set_yscale('log')
    fig.savefig(filepath)
    plt.clf()
    plt.close()


# 1シミュレーションのoutputからその分析結果を得る
def analyze_output(output_dir_name):
    simulation_time = 0
    result = {}
    with open(output_dir_name + 'payments_output.csv', 'r') as csv_pay:
        payments = list(csv.DictReader(csv_pay))

        total_payment_num = len(payments)
        total_attempts_num = 0
        total_success_num = 0
        total_timeout_num = 0
        total_fail_no_path_num = 0
        total_retry_num = 0
        total_retry_no_balance_num = 0
        total_retry_edge_occupied_num = 0
        total_fail_no_alternative_path_num = 0
        time_distribution = []
        retry_distribution = []
        fee_distribution = []
        route_len_distribution = []

        for pay in payments:
            is_success = (pay["is_success"] == "1")
            amount = int(pay["amount"])
            start_time = int(pay["start_time"])
            end_time = int(pay["end_time"])
            time = end_time - start_time
            attempts = int(pay["attempts"])
            retry = attempts - 1

            if simulation_time < end_time:
                simulation_time = end_time

            if is_success:
                total_success_num += 1
                total_fee = int(pay["total_fee"])
                retry_distribution.append(retry)
                fee_distribution.append(total_fee)
                route_len_distribution.append(len(pay['route'].split('-')))
            else:
                if (pay["route"] == "") and (attempts == 1):
                    total_fail_no_path_num += 1
                elif pay["timeout_exp"] == 1:
                    total_timeout_num += 1
                else:
                    total_fail_no_alternative_path_num += 1

            total_attempts_num += attempts
            total_retry_num += retry
            total_retry_no_balance_num += int(pay["no_balance_count"])
            total_retry_edge_occupied_num += int(pay["edge_occupied_count"])
            time_distribution.append(time)

        save_histogram(time_distribution, "Time[ms]", "Frequency", f"{output_dir_name}/time_histogram.pdf", 500)
        save_histogram(retry_distribution, "Retry Num", "Frequency", f"{output_dir_name}/retry_num_histogram.pdf", range(np.min(retry_distribution), np.max(retry_distribution) + 2, 1))
        save_histogram(fee_distribution, "Fee [satoshi]", "Frequency", f"{output_dir_name}/fee_histogram.pdf", 500)
        save_histogram(route_len_distribution, "Route Length", "Frequency", f"{output_dir_name}/route_len_histogram.pdf", range(np.min(route_len_distribution), np.max(route_len_distribution) + 2, 1))

        result = result | {
            "simulation_time": simulation_time,  # シミュレーション時間

            "success_rate": total_success_num / total_payment_num,  # 送金成功率
            "fail_no_path_rate": total_fail_no_path_num / total_payment_num,  # 送金前に送金経路なしと判断され送金失敗した確率
            "fail_timeout_rate": total_timeout_num / total_payment_num,  # timeoutによる送金失敗率
            "fail_no_alternative_path_rate": total_fail_no_alternative_path_num / total_payment_num,  # 送金失敗とリンク除外を繰り返し他結果送金経路がなくなったため送金失敗した確率

            "retry_rate": total_retry_num / total_attempts_num,  # 試行1回あたりのリトライ発生回数
            "retry_no_balance_rate": total_retry_no_balance_num / total_attempts_num,  # 試行1回あたりのfail_no_balanceによるリトライ発生回数
            "retry_edge_occupied_rate": total_retry_edge_occupied_num / total_attempts_num,  # 試行1回あたりのfail_edge_occupiedによるリトライ発生回数

            # 送金開始から送金が完了するまでにかかる時間
            "time/average": np.mean(time_distribution),
            "time/variance": np.var(time_distribution),
            "time/max": np.max(time_distribution),
            "time/min": np.min(time_distribution),
            "time/5-percentile": np.percentile(time_distribution, 5),
            "time/25-percentile": np.percentile(time_distribution, 25),
            "time/50-percentile": np.percentile(time_distribution, 50),
            "time/75-percentile": np.percentile(time_distribution, 75),
            "time/95-percentile": np.percentile(time_distribution, 95),

            # 送金1回あたりのリトライ回数
            "retry/average": np.mean(retry_distribution),
            "retry/variance": np.var(retry_distribution),
            "retry/max": np.max(retry_distribution),
            "retry/min": np.min(retry_distribution),
            "retry/5-percentile": np.percentile(retry_distribution, 5),
            "retry/25-percentile": np.percentile(retry_distribution, 25),
            "retry/50-percentile": np.percentile(retry_distribution, 50),
            "retry/75-percentile": np.percentile(retry_distribution, 75),
            "retry/95-percentile": np.percentile(retry_distribution, 95),

            # 送金手数料
            "fee/average": np.mean(fee_distribution),
            "fee/variance": np.var(fee_distribution),
            "fee/max": np.max(fee_distribution),
            "fee/min": np.min(fee_distribution),
            "fee/5-percentile": np.percentile(fee_distribution, 5),
            "fee/25-percentile": np.percentile(fee_distribution, 25),
            "fee/50-percentile": np.percentile(fee_distribution, 50),
            "fee/75-percentile": np.percentile(fee_distribution, 75),
            "fee/95-percentile": np.percentile(fee_distribution, 95),

            # 送金経路長
            "route_len/average": np.mean(route_len_distribution),
            "route_len/variance": np.var(route_len_distribution),
            "route_len/max": np.max(route_len_distribution),
            "route_len/min": np.min(route_len_distribution),
            "route_len/5-percentile": np.percentile(route_len_distribution, 5),
            "route_len/25-percentile": np.percentile(route_len_distribution, 25),
            "route_len/50-percentile": np.percentile(route_len_distribution, 50),
            "route_len/75-percentile": np.percentile(route_len_distribution, 75),
            "route_len/95-percentile": np.percentile(route_len_distribution, 95),
        }

    edge_fee = {}  # edge_idとそのエッジの手数料の紐付け
    with open(output_dir_name + 'edges_output.csv', 'r') as csv_group:
        edges = list(csv.DictReader(csv_group))
        edge_in_group_num = 0
        for edge in edges:
            edge_fee[edge["id"]] = {"fee_base": edge["fee_base"], "fee_proportional": edge["fee_proportional"]}
            if edge["group"] != "NULL":
                edge_in_group_num += 1
        result = result | {
            "group_cover_rate": edge_in_group_num / len(edges),  # 全エッジに対するグループに属するエッジが占める割合
        }

    with open(output_dir_name + 'channels_output.csv', 'r') as csv_channel:
        channels = list(csv.DictReader(csv_channel))
        channel_locked_time_distribution = []  # チャネルロック合計時間ヒストグラムのグラフデータ
        base_fee_distribution = []  # チャネルの手数料とチャネルロック合計時間の相関関係グラフデータ
        proportional_fee_distribution = []  # チャネルの手数料とチャネルロック合計時間の相関関係グラフデータ
        for channel in channels:
            locked_time = int(channel["total_lock_time"])
            channel_locked_time_distribution.append(locked_time)
            base_fee_distribution.append(int(edge_fee[channel["edge1"]]["fee_base"]))
            proportional_fee_distribution.append(int(edge_fee[channel["edge1"]]["fee_proportional"]))
            channel_locked_time_distribution.append(locked_time)
            base_fee_distribution.append(int(edge_fee[channel["edge2"]]["fee_base"]))
            proportional_fee_distribution.append(int(edge_fee[channel["edge2"]]["fee_proportional"]))

        # save_scatter(base_fee_distribution, channel_locked_time_distribution, "Base fee [satoshi]", "Total Chanel Locked Time [ms]", f"{output_dir_name}/base_fee_scatter.pdf")
        # save_scatter(proportional_fee_distribution, channel_locked_time_distribution, "Base fee [satoshi]", "Total Chanel Locked Time [ms]", f"{output_dir_name}/proportional_fee_scatter.pdf")
        save_histogram(channel_locked_time_distribution, "Total Channel Locked Time [ms]", "Frequency", f"{output_dir_name}/channel_lock_time_histogram.pdf", 500)


        result = result | {
            # "correlation_locktime_basefee": np.corrcoef(channel_locked_time_distribution, base_fee_distribution)[0, 1],  # チャネルの手数料(base_fee)とチャネルロック合計時間の相関係数
            # "correlation_locktime_propfee": np.corrcoef(channel_locked_time_distribution, proportional_fee_distribution)[0, 1],  # チャネルの手数料(proportional_fee)とチャネルロック合計時間の相関係数

            # 各チャネルのロックされていた時間合計
            "total_channel_locked_time/average": np.mean(channel_locked_time_distribution),
            "total_channel_locked_time/variance": np.var(channel_locked_time_distribution),
            "total_channel_locked_time/max": np.max(channel_locked_time_distribution),
            "total_channel_locked_time/min": np.min(channel_locked_time_distribution),
            "total_channel_locked_time/5-percentile": np.percentile(channel_locked_time_distribution, 5),
            "total_channel_locked_time/25-percentile": np.percentile(channel_locked_time_distribution, 25),
            "total_channel_locked_time/50-percentile": np.percentile(channel_locked_time_distribution, 50),
            "total_channel_locked_time/75-percentile": np.percentile(channel_locked_time_distribution, 75),
            "total_channel_locked_time/95-percentile": np.percentile(channel_locked_time_distribution, 95),

            # チャネルがロックされていた時間はシミュレーション全体でどれぐらいの割合を占めるのか
            "channel_locked_time_ratio/average": np.mean(channel_locked_time_distribution) / simulation_time,
            "channel_locked_time_ratio/variance": np.var(channel_locked_time_distribution) / simulation_time,
            "channel_locked_time_ratio/max": np.max(channel_locked_time_distribution) / simulation_time,
            "channel_locked_time_ratio/min": np.min(channel_locked_time_distribution) / simulation_time,
            "channel_locked_time_ratio/5-percentile": np.percentile(channel_locked_time_distribution, 5) / simulation_time,
            "channel_locked_time_ratio/25-percentile": np.percentile(channel_locked_time_distribution, 25) / simulation_time,
            "channel_locked_time_ratio/50-percentile": np.percentile(channel_locked_time_distribution, 50) / simulation_time,
            "channel_locked_time_ratio/75-percentile": np.percentile(channel_locked_time_distribution, 75) / simulation_time,
            "channel_locked_time_ratio/95-percentile": np.percentile(channel_locked_time_distribution, 95) / simulation_time,
        }

    with open(output_dir_name + 'groups_output.csv', 'r') as csv_group:
        groups = list(csv.DictReader(csv_group))

        group_survival_time_distribution = []
        group_capacity_distribution = []
        cul_distribution = []

        for group in groups:
            try:
                cul_distribution.append(float(group["cul"]))
                group_capacity_distribution.append(int(group["group_capacity"]))
                if group["is_closed(closed_time)"] != "0":
                    closed_time = int(group["is_closed(closed_time)"])
                    constructed_time = int(group["constructed_time"])
                    group_survival_time_distribution.append(closed_time - constructed_time)
            except Exception:
                continue

        if len(groups) != 0:
            save_histogram(group_survival_time_distribution, "Group Survival Time", "Frequency", f"{output_dir_name}/group_survival_time_histogram.pdf", 500)
            save_histogram(group_capacity_distribution, "Group Survival Time", "Frequency", f"{output_dir_name}/group_capacity_histogram.pdf", 500)
            save_histogram(cul_distribution, "CUL", "Frequency", f"{output_dir_name}/cul_histogram.pdf", 500)

        result = result | {
            "group_survival_time/average": np.mean(group_survival_time_distribution) if len(groups) else "",
            "group_survival_time/var": np.var(group_survival_time_distribution) if len(groups) else "",
            "group_survival_time/max": np.max(group_survival_time_distribution) if len(groups) else "",
            "group_survival_time/min": np.min(group_survival_time_distribution) if len(groups) else "",
            "group_survival_time/5-percentile": np.percentile(group_survival_time_distribution, 5) if len(groups) else "",
            "group_survival_time/25-percentile": np.percentile(group_survival_time_distribution, 25) if len(groups) else "",
            "group_survival_time/50-percentile": np.percentile(group_survival_time_distribution, 50) if len(groups) else "",
            "group_survival_time/75-percentile": np.percentile(group_survival_time_distribution, 75) if len(groups) else "",
            "group_survival_time/95-percentile": np.percentile(group_survival_time_distribution, 95) if len(groups) else "",

            "group_capacity/average": np.mean(group_capacity_distribution) if len(groups) else "",
            "group_capacity/var": np.var(group_capacity_distribution) if len(groups) else "",
            "group_capacity/max": np.max(group_capacity_distribution) if len(groups) else "",
            "group_capacity/min": np.min(group_capacity_distribution) if len(groups) else "",
            "group_capacity/5-percentile": np.percentile(group_capacity_distribution, 5) if len(groups) else "",
            "group_capacity/25-percentile": np.percentile(group_capacity_distribution, 25) if len(groups) else "",
            "group_capacity/50-percentile": np.percentile(group_capacity_distribution, 50) if len(groups) else "",
            "group_capacity/75-percentile": np.percentile(group_capacity_distribution, 75) if len(groups) else "",
            "group_capacity/95-percentile": np.percentile(group_capacity_distribution, 95) if len(groups) else "",

            "cul/average": np.mean(cul_distribution) if len(groups) else "",
            "cul/var": np.var(cul_distribution) if len(groups) else "",
            "cul/max": np.max(cul_distribution) if len(groups) else "",
            "cul/min": np.min(cul_distribution) if len(groups) else "",
            "cul/5-percentile": np.percentile(cul_distribution, 5) if len(groups) else "",
            "cul/25-percentile": np.percentile(cul_distribution, 25) if len(groups) else "",
            "cul/50-percentile": np.percentile(cul_distribution, 50) if len(groups) else "",
            "cul/75-percentile": np.percentile(cul_distribution, 75) if len(groups) else "",
            "cul/95-percentile": np.percentile(cul_distribution, 95) if len(groups) else "",
        }

    return result


def load_cloth_input(output_dir_name):
    cloth_input = {}
    with open(output_dir_name + "cloth_input.txt", 'r') as cloth_input_file:
        for line in cloth_input_file:
            line = line.strip()
            if line and not line.startswith('#'):
                key, value = line.split('=')
                cloth_input[key.strip()] = value.strip()
    return cloth_input


output_dirs = find_output_dirs(output_root_dir_name)
with open(output_root_dir_name + "/summary.csv", "w", encoding="utf-8") as summary:
    rows = []
    for output_dir in output_dirs:
        row_data = {}
        row_data.update(load_cloth_input(output_dir))
        row_data.update(analyze_output(output_dir))
        rows.append(row_data)
        print(output_dir)

    fieldnames = rows[0].keys()
    writer = csv.DictWriter(summary, fieldnames=fieldnames)
    writer.writeheader()
    for row in rows:
        writer.writerow(row)
        print(row)
