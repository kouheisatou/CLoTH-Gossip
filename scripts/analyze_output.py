import csv
import json
import os
import sys

import numpy as np

if len(sys.argv) < 1:
    print("python3 analyze_output.py <output_dir>")
    exit(1)
output_root_dir_name = sys.argv[1]


def find_output_dirs(root_dir):
    files = []
    for subdir, _, files_in_subdir in os.walk(root_dir):
        for f in files_in_subdir:
            if f == "cloth_input.txt":
                files.append(subdir + "/")
    return files


# 1シミュレーションのoutputからその分析結果を得る
def analyze_output(output_dir_name):
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

        success_rate = total_success_num / total_num
        fail_rate = total_fail_num / total_num
        fail_no_balance_rate = total_fail_no_balance_num / total_num
        fail_no_path_rate = total_fail_no_path_num / total_num
        avg_time = np.mean(time_distribution)
        avg_retry = np.mean(retry_distribution)
        avg_fee = np.mean(fee_distribution)
        avg_route_len = np.mean(route_len_distribution)

        result = {
            "success_rate": success_rate,
            "fail_rate": fail_rate,
            "fail_no_balance_rate": fail_no_balance_rate,
            "fail_no_path_rate": fail_no_path_rate,
            "avg_time": avg_time,
            "avg_retry": avg_retry,
            "avg_fee": avg_fee,
            "avg_route_len": avg_route_len,
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
with open(output_root_dir_name + "summary.csv", "w", encoding="utf-8") as summary:
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
