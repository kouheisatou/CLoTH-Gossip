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


def save_histogram(data: list, title: str, x_label: str, y_label: str, filepath: str, bins):
    fig, ax = plt.subplots()
    ax.hist(data, bins=bins)
    ax.set_xlabel(x_label)
    ax.set_ylabel(y_label)
    ax.set_title(title)
    ax.axvline(x=np.mean(data), color='green', linestyle='--', linewidth=1, label=f'Mean: {np.mean(data):.2f}')
    ax.axvline(x=np.median(data), color='red', linestyle='--', linewidth=1, label=f'Median: {np.median(data):.2f}')
    fig.legend()
    fig.savefig(filepath)
    plt.clf()
    plt.close()


# 1シミュレーションのoutputからその分析結果を得る
def analyze_output(output_dir_name):
    result = {}
    with open(output_dir_name + 'payments_output.csv', 'r') as csv_pay:
        payments = list(csv.DictReader(csv_pay))

        total_num = len(payments)
        total_success_num = 0
        total_fail_num = 0
        total_fail_no_balance_num = 0
        total_fail_no_path_num = 0
        time_distribution = []
        retry_distribution = []
        fee_distribution = []
        route_len_distribution = []

        for pay in payments:
            is_success = (pay["is_success"] == "1")
            fail_no_path = (not is_success) and (pay["route"] == "-1")
            fail_no_balance = (not is_success) and (pay["no_balance_count"] != "0")

            amount = int(pay["amount"])
            start_time = int(pay["start_time"])
            end_time = int(pay["end_time"])
            time = end_time - start_time
            retry = int(pay["attempts"]) - 1

            if is_success:
                total_success_num += 1
                total_fee = int(pay["total_fee"])
                retry_distribution.append(retry)
                time_distribution.append(time)
                fee_distribution.append(total_fee)
                route_len_distribution.append(len(pay['route'].split('-')))
            else:
                total_fail_num += 1
                if fail_no_balance:
                    total_fail_no_balance_num += 1
                    time_distribution.append(time)
                    route_len_distribution.append(len(pay['route'].split('-')))
                elif fail_no_path:
                    total_fail_no_path_num += 1

        save_histogram(time_distribution, "Histogram of Transaction Elapsed Time", "Time[s]", "Frequency", f"{output_dir_name}/time_histogram.pdf", 500)
        save_histogram(retry_distribution, "Histogram of Retry Num", "Retry Num", "Frequency", f"{output_dir_name}/retry_num_histogram.pdf", range(np.min(retry_distribution), np.max(retry_distribution), 1))
        save_histogram(fee_distribution, "Histogram of Fee", "Fee", "Frequency", f"{output_dir_name}/fee_histogram.pdf", 500)
        save_histogram(route_len_distribution, "Histogram of Route Length", "Route Length", "Frequency", f"{output_dir_name}/route_len_histogram.pdf", range(np.min(route_len_distribution), np.max(route_len_distribution), 1))

        result = result | {
            "success_rate": total_success_num / total_num,
            "fail_rate": total_fail_num / total_num,
            "fail_no_balance_rate": total_fail_no_balance_num / total_num,
            "fail_no_path_rate": total_fail_no_path_num / total_num,

            "time/average": np.mean(time_distribution),
            "time/variance": np.var(time_distribution),
            "time/max": np.max(time_distribution),
            "time/min": np.min(time_distribution),
            "time/25-percentile": np.percentile(time_distribution, 25),
            "time/50-percentile": np.percentile(time_distribution, 50),
            "time/75-percentile": np.percentile(time_distribution, 75),

            "retry/average": np.mean(retry_distribution),
            "retry/variance": np.var(retry_distribution),
            "retry/max": np.max(retry_distribution),
            "retry/min": np.min(retry_distribution),
            "retry/25-percentile": np.percentile(retry_distribution, 25),
            "retry/50-percentile": np.percentile(retry_distribution, 50),
            "retry/75-percentile": np.percentile(retry_distribution, 75),

            "fee/average": np.mean(fee_distribution),
            "fee/variance": np.var(fee_distribution),
            "fee/max": np.max(fee_distribution),
            "fee/min": np.min(fee_distribution),
            "fee/25-percentile": np.percentile(fee_distribution, 25),
            "fee/50-percentile": np.percentile(fee_distribution, 50),
            "fee/75-percentile": np.percentile(fee_distribution, 75),

            "route_len/average": np.mean(route_len_distribution),
            "route_len/variance": np.var(route_len_distribution),
            "route_len/max": np.max(route_len_distribution),
            "route_len/min": np.min(route_len_distribution),
            "route_len/25-percentile": np.percentile(route_len_distribution, 25),
            "route_len/50-percentile": np.percentile(route_len_distribution, 50),
            "route_len/75-percentile": np.percentile(route_len_distribution, 75),
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
