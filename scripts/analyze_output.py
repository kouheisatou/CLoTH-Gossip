import csv
import os
import sys

import numpy as np

csv.field_size_limit(200_000_000)
if len(sys.argv) < 2:
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
    "mpp/total_count",
    "mpp/success_count",
    "mpp/success_rate",
    "mpp/usage_rate",
    "shard_count/average",
    "shard_count/variance",
    "shard_count/max",
    "shard_count/min",
    "shard_count/50-percentile",
    "mpp_fee/average",
    "mpp_fee/variance",
    "mpp_fee/max",
    "mpp_fee/min",
    "mpp_fee/50-percentile",
    "mpp_fee_per_satoshi/average",
    "mpp_fee_per_satoshi/variance",
    "mpp_fee_per_satoshi/max",
    "mpp_fee_per_satoshi/min",
    "mpp_fee_per_satoshi/50-percentile",
    "non_mpp_fee/average",
    "non_mpp_fee/variance",
    "non_mpp_fee/max",
    "non_mpp_fee/min",
    "non_mpp_fee/50-percentile",
    "non_mpp_fee_per_satoshi/average",
    "non_mpp_fee_per_satoshi/variance",
    "non_mpp_fee_per_satoshi/max",
    "non_mpp_fee_per_satoshi/min",
    "non_mpp_fee_per_satoshi/50-percentile",
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


def _dist_stats(data, prefix):
    if not data:
        return {
            f"{prefix}/average": 0, f"{prefix}/variance": 0,
            f"{prefix}/max": 0, f"{prefix}/min": 0,
            f"{prefix}/5-percentile": 0, f"{prefix}/25-percentile": 0,
            f"{prefix}/50-percentile": 0, f"{prefix}/75-percentile": 0,
            f"{prefix}/95-percentile": 0,
        }
    arr = np.array(data)
    return {
        f"{prefix}/average": np.mean(arr),
        f"{prefix}/variance": np.var(arr),
        f"{prefix}/max": np.max(arr),
        f"{prefix}/min": np.min(arr),
        f"{prefix}/5-percentile": np.percentile(arr, 5),
        f"{prefix}/25-percentile": np.percentile(arr, 25),
        f"{prefix}/50-percentile": np.percentile(arr, 50),
        f"{prefix}/75-percentile": np.percentile(arr, 75),
        f"{prefix}/95-percentile": np.percentile(arr, 95),
    }


def _dist_stats_short(data, prefix):
    if not data:
        return {
            f"{prefix}/average": 0, f"{prefix}/variance": 0,
            f"{prefix}/max": 0, f"{prefix}/min": 0,
            f"{prefix}/50-percentile": 0,
        }
    arr = np.array(data)
    return {
        f"{prefix}/average": np.mean(arr),
        f"{prefix}/variance": np.var(arr),
        f"{prefix}/max": np.max(arr),
        f"{prefix}/min": np.min(arr),
        f"{prefix}/50-percentile": np.percentile(arr, 50),
    }


def _dist_stats_optional(data, prefix):
    if not data:
        return {
            f"{prefix}/average": "", f"{prefix}/var": "",
            f"{prefix}/max": "", f"{prefix}/min": "",
            f"{prefix}/5-percentile": "", f"{prefix}/25-percentile": "",
            f"{prefix}/50-percentile": "", f"{prefix}/75-percentile": "",
            f"{prefix}/95-percentile": "",
        }
    arr = np.array(data)
    return {
        f"{prefix}/average": np.mean(arr),
        f"{prefix}/var": np.var(arr),
        f"{prefix}/max": np.max(arr),
        f"{prefix}/min": np.min(arr),
        f"{prefix}/5-percentile": np.percentile(arr, 5),
        f"{prefix}/25-percentile": np.percentile(arr, 25),
        f"{prefix}/50-percentile": np.percentile(arr, 50),
        f"{prefix}/75-percentile": np.percentile(arr, 75),
        f"{prefix}/95-percentile": np.percentile(arr, 95),
    }


def analyze_output(output_dir_name):
    simulation_time = 0
    result = {}

    with open(output_dir_name + 'payments_output.csv', 'r') as csv_pay:
        reader = csv.reader(csv_pay)
        header = next(reader)
        col = {name: i for i, name in enumerate(header)}
        rows = list(reader)

    # Build lightweight maps for MPP fee calculation
    has_shards = "shards" in col
    has_is_shard = "is_shard" in col
    has_parent = "parent_payment_id" in col
    has_mpp = "mpp" in col
    has_timeout = "timeout_exp" in col

    shard_children = {}  # pid -> [child_ids]
    leaf_fee = {}        # pid -> int (total_fee for leaf/non-MPP payments)

    for row in rows:
        pid = int(row[col["id"]])
        fee_str = row[col["total_fee"]]
        leaf_fee[pid] = int(fee_str) if fee_str else 0
        if has_shards:
            shards_str = row[col["shards"]]
            if shards_str:
                shard_children[pid] = [int(s) for s in shards_str.split('-')]

    # Memoized MPP fee calculation
    mpp_fee_cache = {}

    def calculate_mpp_total_fee(pid):
        if pid in mpp_fee_cache:
            return mpp_fee_cache[pid]
        if pid not in shard_children:
            fee = leaf_fee.get(pid, 0)
        else:
            fee = sum(calculate_mpp_total_fee(sid) for sid in shard_children[pid])
        mpp_fee_cache[pid] = fee
        return fee

    # Filter root payments
    root_payments = []
    for row in rows:
        is_shard = has_is_shard and row[col["is_shard"]] == "1"
        parent_neg = (not has_parent) or row[col["parent_payment_id"]] == "-1"
        if not is_shard or parent_neg:
            root_payments.append(row)

    total_payment_num = len(root_payments)
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

    total_mpp_num = 0
    total_mpp_success_num = 0
    shard_count_distribution = []
    mpp_fee_distribution = []
    mpp_fee_per_satoshi_distribution = []
    non_mpp_fee_distribution = []
    non_mpp_fee_per_satoshi_distribution = []

    for row in root_payments:
        pid = int(row[col["id"]])
        is_success = (row[col["is_success"]] == "1")
        amount = int(row[col["amount"]])
        start_time = int(row[col["start_time"]])
        end_time = int(row[col["end_time"]])
        time = end_time - start_time
        attempts = int(row[col["attempts"]])
        retry = attempts - 1

        mpp_triggered = has_mpp and row[col["mpp"]] == "1"
        shards_str = row[col["shards"]] if has_shards else ""
        is_mpp = mpp_triggered or bool(shards_str)

        mpp_total_fee = 0
        if is_mpp:
            total_mpp_num += 1
            if shards_str:
                shard_count_distribution.append(len(shards_str.split('-')))
            if is_success:
                total_mpp_success_num += 1
                mpp_total_fee = calculate_mpp_total_fee(pid)
                mpp_fee_distribution.append(mpp_total_fee)
                mpp_fee_per_satoshi_distribution.append(mpp_total_fee / amount if amount > 0 else 0)

        if simulation_time < end_time:
            simulation_time = end_time

        if is_success:
            time_success_distribution.append(time)
            total_success_num += 1

            if is_mpp:
                total_fee = mpp_total_fee
            else:
                total_fee = leaf_fee.get(pid, 0)
                non_mpp_fee_distribution.append(total_fee)
                non_mpp_fee_per_satoshi_distribution.append(total_fee / amount if amount > 0 else 0)

            retry_distribution.append(retry)
            fee_distribution.append(total_fee)
            fee_per_satoshi_distribution.append(total_fee / amount if amount > 0 else 0)
            route_str = row[col["route"]]
            if route_str:
                route_len_distribution.append(len(route_str.split('-')))
        else:
            time_fail_distribution.append(time)
            route_str = row[col["route"]]
            if route_str == "" and attempts == 1:
                total_fail_no_path_num += 1
            elif has_timeout and row[col["timeout_exp"]] == "1":
                total_timeout_num += 1
            else:
                total_fail_no_alternative_path_num += 1

        total_attempts_num += attempts
        total_retry_num += retry
        total_retry_no_balance_num += int(row[col["no_balance_count"]])
        time_distribution.append(time)

    result = {
        "simulation_time": simulation_time,
        "success_rate": total_success_num / total_payment_num,
        "fail_no_path_rate": total_fail_no_path_num / total_payment_num,
        "fail_timeout_rate": total_timeout_num / total_payment_num,
        "fail_no_alternative_path_rate": total_fail_no_alternative_path_num / total_payment_num,
        "retry_rate": total_retry_num / total_attempts_num,
        "retry_no_balance_rate": total_retry_no_balance_num / total_attempts_num,

        "mpp/total_count": total_mpp_num,
        "mpp/success_count": total_mpp_success_num,
        "mpp/success_rate": total_mpp_success_num / total_mpp_num if total_mpp_num > 0 else 0,
        "mpp/usage_rate": total_mpp_num / total_payment_num if total_payment_num > 0 else 0,
    }

    result.update(_dist_stats(time_distribution, "time"))
    result.update(_dist_stats(time_success_distribution, "time_success"))
    result.update(_dist_stats(time_fail_distribution, "time_fail"))
    result.update(_dist_stats(retry_distribution, "retry"))
    result.update(_dist_stats(fee_distribution, "fee"))
    result.update(_dist_stats(fee_per_satoshi_distribution, "fee_per_satoshi"))
    result.update(_dist_stats(route_len_distribution, "route_len"))
    result.update(_dist_stats_short(shard_count_distribution, "shard_count"))
    result.update(_dist_stats_short(mpp_fee_distribution, "mpp_fee"))
    result.update(_dist_stats_short(mpp_fee_per_satoshi_distribution, "mpp_fee_per_satoshi"))
    result.update(_dist_stats_short(non_mpp_fee_distribution, "non_mpp_fee"))
    result.update(_dist_stats_short(non_mpp_fee_per_satoshi_distribution, "non_mpp_fee_per_satoshi"))

    # edges_output.csv - stream without materializing
    with open(output_dir_name + 'edges_output.csv', 'r') as csv_edge:
        reader = csv.reader(csv_edge)
        edge_header = next(reader)
        edge_col = {name: i for i, name in enumerate(edge_header)}
        edge_in_group_num = 0
        total_edges = 0
        for row in reader:
            total_edges += 1
            group_val = row[edge_col["group"]]
            if group_val != "NULL" and group_val != "":
                edge_in_group_num += 1

    result["group_cover_rate"] = edge_in_group_num / total_edges

    # groups_output.csv - stream without materializing
    with open(output_dir_name + 'groups_output.csv', 'r') as csv_group:
        reader = csv.reader(csv_group)
        group_header = next(reader)
        group_col = {name: i for i, name in enumerate(group_header)}

        group_survival_time_distribution = []
        cul_distribution = []

        for row in reader:
            try:
                closed_val = row[group_col["is_closed(closed_time)"]]
                if closed_val != "0":
                    closed_time = int(closed_val)
                    constructed_time = int(row[group_col["constructed_time"]])
                    group_survival_time_distribution.append(closed_time - constructed_time)
                else:
                    cul_distribution.append(float(row[group_col["cul_average"]]))
            except Exception as e:
                print(e)
                continue

    result.update(_dist_stats_optional(group_survival_time_distribution, "group_survival_time"))
    result.update(_dist_stats_optional(cul_distribution, "cul_average"))

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

    rows = []
    for output_dir in output_dirs:
        rows.append(process_output_dir(output_dir))

    summary_csv_header = set()
    for row in rows:
        for column in row:
            summary_csv_header.add(column)

    with open(output_root_dir_name + "/summary.csv", "w", encoding="utf-8") as summary:
        writer = csv.DictWriter(summary, fieldnames=sorted(summary_csv_header))
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
