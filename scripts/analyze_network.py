import csv
import sys

import pandas as pd
from matplotlib import pyplot as plt

# ファイルの読み込み
df = pd.read_csv(sys.argv[1], index_col=0)

# ヒストグラムの作成
plt.figure(dpi=300, figsize=(1.618*4, 4))
plt.hist(df["balance"], bins=100, range=(0, 1.0e+10))
plt.xlabel('Balance [satoshi]')
plt.ylabel('Frequency')

# 平均と中央値の線の描画
plt.axvline(x=df['balance'].mean(), color='green', linestyle='--', linewidth=2, label=f'Mean: {df["balance"].mean():.2f}')
plt.axvline(x=df['balance'].median(), color='red', linestyle='--', linewidth=2, label=f'Median: {df["balance"].median():.2f}')

# 凡例の表示
plt.legend()

# プロットの保存
plt.savefig(f"{sys.argv[1]}_histogram.pdf", bbox_inches="tight")
