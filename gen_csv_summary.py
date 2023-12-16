import os
import json
import csv
import sys


def json_to_csv(json_files, output_csv_path):
    # ヘッダー行のフィールド名を定義
    fieldnames = [
        "filename",
        'Success.Mean', 'FailNoPath.Mean', 'FailNoBalance.Mean',
        'Time.Mean', 'Attempts.Mean', 'RouteLength.Mean',
        'group.group_cover_proportion', 'group.accuracy.mean'
    ]

    # CSVファイルを開き、ヘッダー行を書き込む
    with open(output_csv_path, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()

        # 各 JSON ファイルを読み込み、CSVファイルに書き込む
        for json_file in json_files:
            with open(json_file, 'r', encoding='utf-8') as f:
                json_data = json.load(f)

            # 各キーに対応する値を取得
            group_enabled = "group_cover_proportion" in json_data["group"] or "accuracy" in json_data["group"]
            row_data = {
                'filename': os.path.abspath(json_file),  # ファイル名を取得
                'Success.Mean': json_data['Success']['Mean'],
                'FailNoPath.Mean': json_data['FailNoPath']['Mean'],
                'FailNoBalance.Mean': json_data['FailNoBalance']['Mean'],
                'Time.Mean': json_data['Time']['Mean'],
                'Attempts.Mean': json_data['Attempts']['Mean'],
                'RouteLength.Mean': json_data['RouteLength']['Mean'],
                'group.group_cover_proportion': json_data['group']["group_cover_proportion"] if group_enabled else "",
                'group.accuracy.mean': json_data['group']["accuracy"]['mean'] if group_enabled else ""
            }

            # CSVファイルに書き込む
            writer.writerow(row_data)
    print("Output summary was generated in " + output_csv_path)
    
def find_files_in_subfolders(root_folder, relative_path):
    file_paths = []

    # ルートフォルダ内のすべてのサブフォルダを取得
    for folder_name in os.listdir(root_folder):
        folder_path = os.path.join(root_folder, folder_name)
        if os.path.isdir(folder_path):
            # 指定された相対パスのファイルパスを検索
            file_path = os.path.join(folder_path, relative_path)
            if os.path.exists(file_path):
                file_paths.append(file_path)

    return file_paths


if len(sys.argv) != 2:
    print("python3 gen_csv_summary.py <result_dir>")

# result.json ファイルを読み込む
result_data = find_files_in_subfolders(sys.argv[1], "result/cloth_output.json")
json_to_csv(result_data, sys.argv[1] + "/summary.csv")
