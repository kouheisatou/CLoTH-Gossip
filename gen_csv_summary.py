import os
import json
import csv
import sys


def json_to_csv(json_files, output_csv_path):

    row_data_list = []
    header = []

    # CSVファイルを開き、ヘッダー行を書き込む
    with open(output_csv_path, 'w', newline='', encoding='utf-8') as csvfile:

        # 各 JSON ファイルを読み込み、CSVファイルに書き込む
        for json_file in json_files:
            with open(json_file, 'r', encoding='utf-8') as f:
                json_data = json.load(f)

            # 各キーに対応する値を取得
            group_enabled = "group_cover_rate" in json_data["group"] or "accuracy" in json_data["group"]
            row_data = {
                'filename': os.path.abspath(json_file),
                'Success.Mean': json_data['Success']['Mean'],
                'FailNoPath.Mean': json_data['FailNoPath']['Mean'],
                'FailNoBalance.Mean': json_data['FailNoBalance']['Mean'],
                'Time.Mean': json_data['Time']['Mean'],
                'Attempts.Mean': json_data['Attempts']['Mean'],
                'RouteLength.Mean': json_data['RouteLength']['Mean'],
                'group.group_cover_rate': json_data['group']["group_cover_rate"] if group_enabled else "",
                'group.accuracy.mean': json_data['group']["accuracy"]['mean'] if group_enabled else ""
            }

            try:
                cloth_input_path = os.path.dirname(json_file) + "/cloth_input.txt"
                with open(cloth_input_path, 'r') as file:
                    for line in file:
                        line = line.strip()
                        if line and not line.startswith('#'):
                            key, value = line.split('=')
                            row_data[key.strip()] = value.strip()
            except:
                continue

            row_data_list.append(row_data)
            header = row_data.keys()

        writer = csv.DictWriter(csvfile, fieldnames=header)
        writer.writeheader()
        for row_data in row_data_list:
            writer.writerow(row_data)

    print("Output summary was generated in " + output_csv_path)


def find_files_in_subdir(root_dir):
    files = []
    for subdir, _, files_in_subdir in os.walk(root_dir):
        for f in files_in_subdir:
            if f == "cloth_output.json":
                files.append(os.path.join(subdir, f))
    return files


if len(sys.argv) != 2:
    print("python3 gen_csv_summary.py <output_dir>")

# result.json ファイルを読み込む
files = find_files_in_subdir(sys.argv[1])
json_to_csv(files, sys.argv[1] + "/summary.csv")
