# シミュレーションごとのヒストグラム分析

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os
import matplotlib.ticker as ticker


def find_files_in_subdir(filename, root_dir):
    files = []
    for subdir, _, files_in_subdir in os.walk(root_dir):
        for f in files_in_subdir:
            if f == filename:
                files.append(os.path.join(subdir, f))
    return files


# analyze initial network edge capacity distribution
df = pd.read_csv("../CLoTH-Gossip_input オフチェーンネットワーク初期状態/edges_ln.csv", index_col=0)
print(df["balance"].describe())
fig, ax = plt.subplots()
ax.hist(df["balance"], bins=200, range=(0, 1.0e+10))
ax.set_xlabel('Balance')
ax.set_ylabel('Frequency')
ax.set_title("input edges_ln.csv balance histogram")
ax.axvline(x=df['balance'].mean(), color='green', linestyle='--', linewidth=2, label=f'Mean: {df["balance"].mean():.2f}')
ax.axvline(x=df['balance'].median(), color='red', linestyle='--', linewidth=2, label=f'Median: {df["balance"].median():.2f}')
# plt.axvline(x=np.percentile(df['balance'], 25), color='Magenta', linestyle='--', linewidth=2, label=f'25th Percentile: {np.percentile(df["balance"], 25):.2f}')
# plt.axvline(x=np.percentile(df['balance'], 75), color='Magenta', linestyle='--', linewidth=2, label=f'75th Percentile: {np.percentile(df["balance"], 75):.2f}')
ax.legend()
# plt.gca().get_xaxis().get_major_formatter().set_useOffset(False)
# plt.gca().get_xaxis().set_major_locator(ticker.MaxNLocator(integer=True))
plt.show()


# analyze network after simulation edge capacity distribution
files = find_files_in_subdir("edges_output.csv", "./")
for file in files: 
    print(file)
    df = pd.read_csv(file, index_col=0)
    print(df["balance"].describe())
    plt.hist(df["balance"], bins=np.linspace(0, 1.0e+10, 100))
    plt.xlabel('Balance')
    plt.ylabel('Frequency')
    plt.title(f'balance histogram of {file}')
    plt.axvline(x=df['balance'].mean(), color='green', linestyle='--', linewidth=2, label=f'Mean: {df["balance"].mean():.2f}')
    plt.axvline(x=df['balance'].median(), color='red', linestyle='--', linewidth=2, label=f'Median: {df["balance"].median():.2f}')
    plt.legend()
    plt.show()


# グループの生存時間の分布
files = find_files_in_subdir("groups_output.csv", "./CLoTH-Gossip_output/routing_method=group_routing,group_update=true")
for file in files: 
    df = pd.read_csv(file)
    df['group_survival_time'] = df['is_closed(closed_time)'] - df['constructed_time']
    df = df[df['group_survival_time'] >= 0]
    plt.hist(df["group_survival_time"], bins=100)
    plt.xlabel('group_survival_time')
    plt.ylabel('Frequency')
    plt.title(f'group survival time histogram of {file}')
    plt.axvline(x=df['group_survival_time'].mean(), color='green', linestyle='--', linewidth=2, label=f'Mean: {df["group_survival_time"].mean():.2f}')
    plt.axvline(x=df['group_survival_time'].median(), color='red', linestyle='--', linewidth=2, label=f'Median: {df["group_survival_time"].median():.2f}')
    plt.legend()
    plt.show()

# todoグループの生存時間と平均送金額の関係


# グループキャパシティの分布
files = find_files_in_subdir("groups_output.csv", "./CLoTH-Gossip_output/routing_method=group_routing,group_update=true")
for file in files: 
    df = pd.read_csv(file)
    plt.hist(df["group_capacity"], bins=100)
    plt.xlabel('group_capacity')
    plt.ylabel('Frequency')
    plt.title(f'group survival time histogram of {file}')
    plt.axvline(x=df['group_capacity'].mean(), color='green', linestyle='--', linewidth=2, label=f'Mean: {df["group_capacity"].mean():.2f}')
    plt.axvline(x=df['group_capacity'].median(), color='red', linestyle='--', linewidth=2, label=f'Median: {df["group_capacity"].median():.2f}')
    plt.legend()
    plt.show()

