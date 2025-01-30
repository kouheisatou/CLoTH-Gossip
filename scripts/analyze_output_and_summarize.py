import csv
import os
import sys
from concurrent.futures import ProcessPoolExecutor

import numpy as np
from matplotlib import pyplot as plt

csv.field_size_limit(200_000_000)
if len(sys.argv) < 1:
    print("python3 analyze_output_and_summarize.py <output_dir>")
    exit(1)
output_root_dir_name = sys.argv[1]


def find_output_dirs(root_dir):
    files = []
    for subdir, _, files_in_subdir in os.walk(root_dir):
        for f in files_in_subdir:
            if f == "payments_output.csv":
                files.append(subdir + "/")
    return files


def save_histogram(data: list, x_label: str, y_label: str, filepath: str, bins):
    return
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
        total_fail_no_alternative_path_num = 0
        time_distribution = []
        retry_distribution = []
        fee_distribution = []
        fee_per_satoshi_distribution = []
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
                fee_per_satoshi_distribution.append(total_fee / amount)
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

            "retry_rate": total_retry_num / total_attempts_num,  # 試行1回あたりの平均リトライ発生回数
            "retry_no_balance_rate": total_retry_no_balance_num / total_attempts_num,  # 試行1回あたりのfail_no_balanceによる平均リトライ発生回数

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

            # 送金手数料(1satoshi送るのにかかる手数料の平均)
            "fee_per_satoshi/average": np.mean(fee_per_satoshi_distribution),
            "fee_per_satoshi/variance": np.var(fee_per_satoshi_distribution),
            "fee_per_satoshi/max": np.max(fee_per_satoshi_distribution),
            "fee_per_satoshi/min": np.min(fee_per_satoshi_distribution),
            "fee_per_satoshi/5-percentile": np.percentile(fee_per_satoshi_distribution, 5),
            "fee_per_satoshi/25-percentile": np.percentile(fee_per_satoshi_distribution, 25),
            "fee_per_satoshi/50-percentile": np.percentile(fee_per_satoshi_distribution, 50),
            "fee_per_satoshi/75-percentile": np.percentile(fee_per_satoshi_distribution, 75),
            "fee_per_satoshi/95-percentile": np.percentile(fee_per_satoshi_distribution, 95),

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
    with open(output_dir_name + 'edges_output.csv', 'r') as csv_edge:
        edges = list(csv.DictReader(csv_edge))

        edge_in_group_num = 0
        locked_balance_and_duration_distribution = []

        for edge in edges:
            edge_fee[edge["id"]] = {"fee_base": edge["fee_base"], "fee_proportional": edge["fee_proportional"]}
            if edge["group"] != "NULL":
                edge_in_group_num += 1
            if edge["locked_balance_and_duration"] != "":
                locked_balance_and_duration = [tuple(map(int, pair.split('x'))) for pair in edge["locked_balance_and_duration"].split("-")]
                total_locked_balance_and_duration = 0
                for (locked_balance, locked_duration) in locked_balance_and_duration:
                    total_locked_balance_and_duration += locked_balance * locked_duration # [millisatoshi*ms]
                locked_balance_and_duration_distribution.append(total_locked_balance_and_duration)

        result = result | {
            "group_cover_rate": edge_in_group_num / len(edges),  # 全エッジに対するグループに属するエッジが占める割合

            # edgeごとの、ロックされた残高×ロックされた時間の合計の平均[millisatoshi*ms]
            "total_locked_balance_duration/average": np.mean(locked_balance_and_duration_distribution),
            "total_locked_balance_duration/variance": np.var(locked_balance_and_duration_distribution),
            "total_locked_balance_duration/max": np.max(locked_balance_and_duration_distribution),
            "total_locked_balance_duration/min": np.min(locked_balance_and_duration_distribution),
            "total_locked_balance_duration/5-percentile": np.percentile(locked_balance_and_duration_distribution, 5),
            "total_locked_balance_duration/25-percentile": np.percentile(locked_balance_and_duration_distribution, 25),
            "total_locked_balance_duration/50-percentile": np.percentile(locked_balance_and_duration_distribution, 50),
            "total_locked_balance_duration/75-percentile": np.percentile(locked_balance_and_duration_distribution, 75),
            "total_locked_balance_duration/95-percentile": np.percentile(locked_balance_and_duration_distribution, 95),
        }

    with open(output_dir_name + 'groups_output.csv', 'r') as csv_group:
        groups = list(csv.DictReader(csv_group))

        group_survival_time_distribution = []
        group_capacity_distribution = []
        cul_distribution = []

        for group in groups:
            try:
                group_capacity_distribution.append(int(group["group_capacity"]))
                if group["is_closed(closed_time)"] != "0":
                    closed_time = int(group["is_closed(closed_time)"])
                    constructed_time = int(group["constructed_time"])
                    group_survival_time_distribution.append(closed_time - constructed_time)
                else:
                    cul_distribution.append(float(group["cul"]))
            except Exception:
                continue

        if len(groups) != 0:
            save_histogram(group_survival_time_distribution, "Group Survival Time", "Frequency", f"{output_dir_name}/group_survival_time_histogram.pdf", 500)
            save_histogram(group_capacity_distribution, "Group Survival Time", "Frequency", f"{output_dir_name}/group_capacity_histogram.pdf", 500)
            save_histogram(cul_distribution, "CUL", "Frequency", f"{output_dir_name}/cul_histogram.pdf", 500)

        result = result | {
            "group_survival_time/average": np.mean(group_survival_time_distribution) if len(group_survival_time_distribution) else "",
            "group_survival_time/var": np.var(group_survival_time_distribution) if len(group_survival_time_distribution) else "",
            "group_survival_time/max": np.max(group_survival_time_distribution) if len(group_survival_time_distribution) else "",
            "group_survival_time/min": np.min(group_survival_time_distribution) if len(group_survival_time_distribution) else "",
            "group_survival_time/5-percentile": np.percentile(group_survival_time_distribution, 5) if len(group_survival_time_distribution) else "",
            "group_survival_time/25-percentile": np.percentile(group_survival_time_distribution, 25) if len(group_survival_time_distribution) else "",
            "group_survival_time/50-percentile": np.percentile(group_survival_time_distribution, 50) if len(group_survival_time_distribution) else "",
            "group_survival_time/75-percentile": np.percentile(group_survival_time_distribution, 75) if len(group_survival_time_distribution) else "",
            "group_survival_time/95-percentile": np.percentile(group_survival_time_distribution, 95) if len(group_survival_time_distribution) else "",

            "group_capacity/average": np.mean(group_capacity_distribution) if len(group_survival_time_distribution) else "",
            "group_capacity/var": np.var(group_capacity_distribution) if len(group_survival_time_distribution) else "",
            "group_capacity/max": np.max(group_capacity_distribution) if len(group_survival_time_distribution) else "",
            "group_capacity/min": np.min(group_capacity_distribution) if len(group_survival_time_distribution) else "",
            "group_capacity/5-percentile": np.percentile(group_capacity_distribution, 5) if len(group_survival_time_distribution) else "",
            "group_capacity/25-percentile": np.percentile(group_capacity_distribution, 25) if len(group_survival_time_distribution) else "",
            "group_capacity/50-percentile": np.percentile(group_capacity_distribution, 50) if len(group_survival_time_distribution) else "",
            "group_capacity/75-percentile": np.percentile(group_capacity_distribution, 75) if len(group_survival_time_distribution) else "",
            "group_capacity/95-percentile": np.percentile(group_capacity_distribution, 95) if len(group_survival_time_distribution) else "",

            "cul/average": np.mean(cul_distribution) if len(group_survival_time_distribution) else "",
            "cul/var": np.var(cul_distribution) if len(group_survival_time_distribution) else "",
            "cul/max": np.max(cul_distribution) if len(group_survival_time_distribution) else "",
            "cul/min": np.min(cul_distribution) if len(group_survival_time_distribution) else "",
            "cul/5-percentile": np.percentile(cul_distribution, 5) if len(group_survival_time_distribution) else "",
            "cul/25-percentile": np.percentile(cul_distribution, 25) if len(group_survival_time_distribution) else "",
            "cul/50-percentile": np.percentile(cul_distribution, 50) if len(group_survival_time_distribution) else "",
            "cul/75-percentile": np.percentile(cul_distribution, 75) if len(group_survival_time_distribution) else "",
            "cul/95-percentile": np.percentile(cul_distribution, 95) if len(group_survival_time_distribution) else "",
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
    cloth_input["request_amt_rate"] = int(cloth_input["average_payment_amount"]) * int(cloth_input["payment_rate"]) # 単位時間に発生する送金リクエスト額[satoshi/sec]
    return cloth_input


def process_output_dir(output_dir):
    row_data = {}
    row_data.update(load_cloth_input(output_dir))
    row_data.update(analyze_output(output_dir))
    print(output_dir)
    return row_data


if __name__ == "__main__":
    output_root_dir_name = sys.argv[1]
    output_dirs = find_output_dirs(output_root_dir_name)

    num_processes = os.cpu_count()

    with ProcessPoolExecutor(max_workers=num_processes) as executor:
        futures = [executor.submit(process_output_dir, output_dir) for output_dir in output_dirs]

        rows = []
        for future in futures:
            rows.append(future.result())

    with open(output_root_dir_name + "/summary.csv", "w", encoding="utf-8") as summary:
        fieldnames = rows[0].keys() if rows else []
        writer = csv.DictWriter(summary, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
