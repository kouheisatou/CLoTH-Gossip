import csv
import os
import sys
from concurrent.futures import ProcessPoolExecutor

import numpy as np
from matplotlib import pyplot as plt

csv.field_size_limit(200_000_000)
if len(sys.argv) < 1:
    print("python3 analyze_output.py <output_dir>")
    exit(1)
output_root_dir_name = sys.argv[1]

SUMMARY_CSV_HEADER = [
    "simulation_id",
    "generate_network_from_file",
    "nodes_filename",
    "channels_filename",
    "edges_filename",
    "n_additional_nodes",
    "n_channels_per_node",
    "capacity_per_channel",
    "faulty_node_probability",
    "generate_payments_from_file",
    "payment_timeout",
    "average_payment_forward_interval",
    "variance_payment_forward_interval",
    "routing_method",
    "group_size",
    "group_limit_rate",
    "group_cap_update",
    "group_broadcast_delay",
    "payments_filename",
    "payment_rate",
    "n_payments",
    "average_payment_amount",
    "variance_payment_amount",
    "average_max_fee_limit",
    "variance_max_fee_limit",
    "enable_fake_balance_update",
    "mpp",
    "seed",
    "request_amt_rate",
    "simulation_time",
    "success_rate",
    "fail_no_path_rate",
    "fail_timeout_rate",
    "fail_no_alternative_path_rate",
    "retry_rate",
    "retry_no_balance_rate",
    "time/average",
    "time/variance",
    "time/max",
    "time/min",
    "time/5-percentile",
    "time/25-percentile",
    "time/50-percentile",
    "time/75-percentile",
    "time/95-percentile",
    "time_success/average",
    "time_success/variance",
    "time_success/max",
    "time_success/min",
    "time_success/5-percentile",
    "time_success/25-percentile",
    "time_success/50-percentile",
    "time_success/75-percentile",
    "time_success/95-percentile",
    "time_fail/average",
    "time_fail/variance",
    "time_fail/max",
    "time_fail/min",
    "time_fail/5-percentile",
    "time_fail/25-percentile",
    "time_fail/50-percentile",
    "time_fail/75-percentile",
    "time_fail/95-percentile",
    "retry/average",
    "retry/variance",
    "retry/max",
    "retry/min",
    "retry/5-percentile",
    "retry/25-percentile",
    "retry/50-percentile",
    "retry/75-percentile",
    "retry/95-percentile",
    "fee/average",
    "fee/variance",
    "fee/max",
    "fee/min",
    "fee/5-percentile",
    "fee/25-percentile",
    "fee/50-percentile",
    "fee/75-percentile",
    "fee/95-percentile",
    "fee_per_satoshi/average",
    "fee_per_satoshi/variance",
    "fee_per_satoshi/max",
    "fee_per_satoshi/min",
    "fee_per_satoshi/5-percentile",
    "fee_per_satoshi/25-percentile",
    "fee_per_satoshi/50-percentile",
    "fee_per_satoshi/75-percentile",
    "fee_per_satoshi/95-percentile",
    "route_len/average",
    "route_len/variance",
    "route_len/max",
    "route_len/min",
    "route_len/5-percentile",
    "route_len/25-percentile",
    "route_len/50-percentile",
    "route_len/75-percentile",
    "route_len/95-percentile",
    "group_cover_rate",
    "total_locked_balance_duration/average",
    "total_locked_balance_duration/variance",
    "total_locked_balance_duration/max",
    "total_locked_balance_duration/min",
    "total_locked_balance_duration/5-percentile",
    "total_locked_balance_duration/25-percentile",
    "total_locked_balance_duration/50-percentile",
    "total_locked_balance_duration/75-percentile",
    "total_locked_balance_duration/95-percentile",
    "group_survival_time/average",
    "group_survival_time/var",
    "group_survival_time/max",
    "group_survival_time/min",
    "group_survival_time/5-percentile",
    "group_survival_time/25-percentile",
    "group_survival_time/50-percentile",
    "group_survival_time/75-percentile",
    "group_survival_time/95-percentile",
    "group_capacity/average",
    "group_capacity/var",
    "group_capacity/max",
    "group_capacity/min",
    "group_capacity/5-percentile",
    "group_capacity/25-percentile",
    "group_capacity/50-percentile",
    "group_capacity/75-percentile",
    "group_capacity/95-percentile",
    "cul/average",
    "cul/var",
    "cul/max",
    "cul/min",
    "cul/5-percentile",
    "cul/25-percentile",
    "cul/50-percentile",
    "cul/75-percentile",
    "cul/95-percentile",
]


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
        total_fail_no_alternative_path_num = 0
        time_distribution = []
        time_success_distribution = []
        time_fail_distribution = []
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
                time_success_distribution.append(time)
                total_success_num += 1
                total_fee = int(pay["total_fee"])
                retry_distribution.append(retry)
                fee_distribution.append(total_fee)
                fee_per_satoshi_distribution.append(total_fee / amount)
                route_len_distribution.append(len(pay['route'].split('-')))
            else:
                time_fail_distribution.append(time)
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

        # save_histogram(time_distribution, "Time[ms]", "Frequency", f"{output_dir_name}/time_histogram.pdf", 500)
        # save_histogram(retry_distribution, "Retry Num", "Frequency", f"{output_dir_name}/retry_num_histogram.pdf", range(np.min(retry_distribution), np.max(retry_distribution) + 2, 1))
        # save_histogram(fee_distribution, "Fee [satoshi]", "Frequency", f"{output_dir_name}/fee_histogram.pdf", 500)
        # save_histogram(route_len_distribution, "Route Length", "Frequency", f"{output_dir_name}/route_len_histogram.pdf", range(np.min(route_len_distribution), np.max(route_len_distribution) + 2, 1))

        result = result | {
            "simulation_time": simulation_time,  # シミュレーション時間

            "success_rate": total_success_num / total_payment_num,  # 送金成功率
            "fail_no_path_rate": total_fail_no_path_num / total_payment_num,  # 送金前に送金経路なしと判断され送金失敗した確率
            "fail_timeout_rate": total_timeout_num / total_payment_num,  # timeoutによる送金失敗率
            "fail_no_alternative_path_rate": total_fail_no_alternative_path_num / total_payment_num,  # 送金失敗とリンク除外を繰り返し他結果送金経路がなくなったため送金失敗した確率

            "retry_rate": total_retry_num / total_attempts_num,  # 試行1回あたりの平均リトライ発生回数
            "retry_no_balance_rate": total_retry_no_balance_num / total_attempts_num,  # 試行1回あたりのfail_no_balanceによる平均リトライ発生回数

            # 送金開始から送金が完了するまでにかかる時間（送金の成否に関わらず全ての平均）
            "time/average": np.mean(time_distribution),
            "time/variance": np.var(time_distribution),
            "time/max": np.max(time_distribution),
            "time/min": np.min(time_distribution),
            "time/5-percentile": np.percentile(time_distribution, 5),
            "time/25-percentile": np.percentile(time_distribution, 25),
            "time/50-percentile": np.percentile(time_distribution, 50),
            "time/75-percentile": np.percentile(time_distribution, 75),
            "time/95-percentile": np.percentile(time_distribution, 95),

            # 送金開始から送金が完了するまでにかかる時間（送金の成否に関わらず全ての平均）
            "time_success/average": np.mean(time_success_distribution),
            "time_success/variance": np.var(time_success_distribution),
            "time_success/max": np.max(time_success_distribution),
            "time_success/min": np.min(time_success_distribution),
            "time_success/5-percentile": np.percentile(time_success_distribution, 5),
            "time_success/25-percentile": np.percentile(time_success_distribution, 25),
            "time_success/50-percentile": np.percentile(time_success_distribution, 50),
            "time_success/75-percentile": np.percentile(time_success_distribution, 75),
            "time_success/95-percentile": np.percentile(time_success_distribution, 95),

            # 送金開始から送金が完了するまでにかかる時間（送金の成否に関わらず全ての平均）
            "time_fail/average": np.mean(time_fail_distribution),
            "time_fail/variance": np.var(time_fail_distribution),
            "time_fail/max": np.max(time_fail_distribution),
            "time_fail/min": np.min(time_fail_distribution),
            "time_fail/5-percentile": np.percentile(time_fail_distribution, 5),
            "time_fail/25-percentile": np.percentile(time_fail_distribution, 25),
            "time_fail/50-percentile": np.percentile(time_fail_distribution, 50),
            "time_fail/75-percentile": np.percentile(time_fail_distribution, 75),
            "time_fail/95-percentile": np.percentile(time_fail_distribution, 95),

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

    with open(output_dir_name + 'edges_output.csv', 'r') as csv_edge:
        edges = list(csv.DictReader(csv_edge))

        edge_in_group_num = 0

        for edge in edges:
            if edge["group"] != "NULL" and edge["group"] != "":
                edge_in_group_num += 1

        result = result | {
            "group_cover_rate": edge_in_group_num / len(edges),  # 全エッジに対するグループに属するエッジが占める割合
        }

    with open(output_dir_name + 'groups_output.csv', 'r') as csv_group:
        groups = list(csv.DictReader(csv_group))

        group_survival_time_distribution = []
        cul_distribution = []

        for group in groups:
            try:
                if group["is_closed(closed_time)"] != "0":
                    closed_time = int(group["is_closed(closed_time)"])
                    constructed_time = int(group["constructed_time"])
                    group_survival_time_distribution.append(closed_time - constructed_time)
                else:
                    cul_distribution.append(float(group["cul_average"]))
            except Exception as e:
                print(e)
                continue

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

            "cul_average/average": np.mean(cul_distribution) if len(group_survival_time_distribution) else "",
            "cul_average/var": np.var(cul_distribution) if len(group_survival_time_distribution) else "",
            "cul_average/max": np.max(cul_distribution) if len(group_survival_time_distribution) else "",
            "cul_average/min": np.min(cul_distribution) if len(group_survival_time_distribution) else "",
            "cul_average/5-percentile": np.percentile(cul_distribution, 5) if len(group_survival_time_distribution) else "",
            "cul_average/25-percentile": np.percentile(cul_distribution, 25) if len(group_survival_time_distribution) else "",
            "cul_average/50-percentile": np.percentile(cul_distribution, 50) if len(group_survival_time_distribution) else "",
            "cul_average/75-percentile": np.percentile(cul_distribution, 75) if len(group_survival_time_distribution) else "",
            "cul_average/95-percentile": np.percentile(cul_distribution, 95) if len(group_survival_time_distribution) else "",
        }

    return result


def load_cloth_input(output_dir_name):
    cloth_input = {}
    with open(output_dir_name + "cloth_input.txt", 'r') as cloth_input_file:
        for line in cloth_input_file:
            line = line.strip()
            if line and not line.startswith('#'):
                key, value = line.split('=')
                cloth_input["#param/" + key.strip()] = value.strip()
    return cloth_input


def process_output_dir(output_dir):
    relative_path = output_dir.replace(output_root_dir_name, "")
    if relative_path.startswith("/"):
        relative_path = relative_path[1:]
    row_data = {"simulation_id": relative_path}
    row_data.update(load_cloth_input(output_dir))
    try:
        row_data.update(analyze_output(output_dir))
    except Exception as e:
        print("FAILED SIMULATION : " + output_dir, e, file=sys.stderr)
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

    summary_csv_header = set()
    for row in rows:
        for column in row:
            summary_csv_header.add(column)

    with open(output_root_dir_name + "/summary.csv", "w", encoding="utf-8") as summary:
        writer = csv.DictWriter(summary, fieldnames=sorted(summary_csv_header))
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
