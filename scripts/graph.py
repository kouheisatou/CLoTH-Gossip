import csv
import math
import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

import matplotlib.ticker as mticker

markers = [".", "x", "+", "1", "2", "3", "4", "|", "_", "v", "^", "<", ">", "8", "s", "p", "*", "h", "H", "D", "d"]


def log_tick_formatter(val, pos=None):
    return f"$10^{{{int(val)}}}$"


def csv_to_dict_list(file_path: str):
    dict_list = []
    with open(file_path, 'r', newline='', encoding='utf-8') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            dict_list.append(dict(row))
    return dict_list


def show_3d_graph(csv_file: str,
                  x_key: str,
                  y_key: str,
                  z_key,
                  fix: dict,
                  x_logarithmic_scale: bool = False,
                  y_logarithmic_scale: bool = False,
                  z_logarithmic_scale: bool = False,
                  x_axis: str = None,
                  y_axis: str = None,
                  z_axis: str = None,
                  title: str = None,
                  ):
    lines_data = {}
    for line in csv_to_dict_list(csv_file):

        # err check
        if (x_key not in line) or (y_key not in line) or (x_key in fix) or (y_key in fix) or (z_key in fix):
            print("key error")
            exit(-1)

        # add series
        hit = True
        for key in line:
            for fix_key in fix:
                if key == fix_key and line[key] not in fix[fix_key]:
                    hit = False
        if hit:
            x = float(line[x_key])
            y = float(line[y_key])
            z = float(line[z_key])
            if y not in lines_data:
                lines_data[y] = []
            lines_data[y].append((x, y, z))

    lines_data = dict(sorted({key: sorted(values, key=lambda _x: _x[0]) for key, values in lines_data.items()}.items()))
    print(lines_data)

    fig = plt.figure()
    ax = fig.add_subplot(111, projection='3d')
    if x_logarithmic_scale:
        ax.xaxis.set_major_formatter(mticker.FuncFormatter(log_tick_formatter))
        ax.xaxis.set_major_locator(mticker.MaxNLocator(integer=True))
    if y_logarithmic_scale:
        ax.yaxis.set_major_formatter(mticker.FuncFormatter(log_tick_formatter))
        ax.yaxis.set_major_locator(mticker.MaxNLocator(integer=True))
    if z_logarithmic_scale:
        ax.zaxis.set_major_formatter(mticker.FuncFormatter(log_tick_formatter))
        ax.zaxis.set_major_locator(mticker.MaxNLocator(integer=True))

    for y_value, line_data in lines_data.items():
        xs, ys, zs = zip(*line_data)
        x = np.log10(xs) if x_logarithmic_scale else xs
        y = np.log10(ys) if y_logarithmic_scale else ys
        z = np.log10(zs) if z_logarithmic_scale else zs
        ax.plot(x, y, z, label=f'{y_key}={y_value}', marker=markers[0])

    ax.set_xlabel(x_key if x_axis is None else x_axis)
    ax.set_ylabel(y_key if y_axis is None else y_axis)
    ax.set_zlabel(z_key if z_axis is None else z_axis)
    ax.set_title(z_key + "\n" + str(fix) if title is None else title)

    ax.legend()
    ax.get_legend().remove()
    plt.show()


def show_2d_graph(csv_file: str,
                  x_key: str,
                  y_key: str,
                  series_key: str,
                  fix: dict,
                  x_logarithmic_scale: bool = False,
                  y_logarithmic_scale: bool = False,
                  x_axis: str = None,
                  y_axis: str = None,
                  title: str = None,
                  errorbar_bottom_key: str = None,
                  errorbar_top_key: str = None,
                  variance_key: str = None,
                  ):
    data = {}
    for csv_line in csv_to_dict_list(csv_file):

        if (x_key not in csv_line) or (y_key not in csv_line) or (x_key in fix) or (y_key in fix):
            print("x or y key does not exists")
            exit(-1)
        if (variance_key is not None) and (variance_key not in csv_line):
            print("variance key does not exists in cev")
        if (errorbar_top_key is not None) and (errorbar_top_key not in csv_line) or (errorbar_bottom_key is not None) and (errorbar_bottom_key not in csv_line):
            print("errorbar key does not exists")
            exit(-1)
        if ((errorbar_top_key is not None) or (errorbar_bottom_key is not None)) and not ((errorbar_top_key is not None) and (errorbar_bottom_key is not None)):
            print("errorbar_top_key and errorbar_bottom_key is always both NOT NONE or NONE")
            exit(-1)

        # if current line's specified property is specified value
        hit = True
        for key in csv_line:
            for fix_key in fix:
                if key == fix_key and csv_line[key] not in fix[fix_key]:
                    hit = False

        if hit:
            x = float(csv_line[x_key])
            y = float(csv_line[y_key])
            series = csv_line[series_key]
            if series not in data:
                data[series] = []
            if (variance_key is not None) and (csv_line[variance_key] is not None):
                data[series].append((x, y, float(csv_line[variance_key])))
            elif (errorbar_bottom_key is not None) and (csv_line[errorbar_bottom_key] is not None) and (errorbar_top_key is not None) and (csv_line[errorbar_top_key] is not None):
                data[series].append((x, y, float(csv_line[errorbar_bottom_key]), float(csv_line[errorbar_top_key])))
            else:
                data[series].append((x, y))

    data = dict(sorted({key: sorted(values, key=lambda _x: _x[0]) for key, values in data.items()}.items()))
    print(data)

    fig, ax = plt.subplots()
    if x_logarithmic_scale:
        ax.set_xscale("log")
    if y_logarithmic_scale:
        ax.set_yscale("log")

    count = 0
    for series, line_data in data.items():
        if variance_key is not None:
            x, y, variance = zip(*line_data)
            ax.errorbar(x,
                        y,
                        label=f'{series_key}={series}',
                        marker=markers[count % len(markers)],
                        yerr=list(map(lambda x: math.sqrt(x), (list(variance)))),
                        capsize=3,
                        )
        elif errorbar_bottom_key is not None and errorbar_top_key is not None:
            x, y, errorbar_bottom, errorbar_top = zip(*line_data)
            ax.errorbar(x,
                        y,
                        label=f'{series_key}={series}',
                        marker=markers[count % len(markers)],
                        yerr=[np.subtract(y, errorbar_bottom), np.add(errorbar_top, y)],
                        capsize=3,
                        )
        else:
            x, y = zip(*line_data)
            ax.plot(x, y, label=f'{series_key}={series}', marker=markers[count % len(markers)])
        count += 1

    ax.set_xlabel(x_key if x_axis is None else x_axis)
    ax.set_ylabel(y_key if y_axis is None else y_axis)
    ax.set_title(y_key + "\n" + str(fix) if title is None else title)

    plt.minorticks_on()
    plt.grid()

    ax.legend()

    dirname, _ = os.path.split(csv_file)
    plt.savefig(f"{dirname}/{title}.pdf", bbox_inches="tight")
    # plt.show()

    plt.clf()
    plt.close()


# for i in range(4, 5):  # for light simulation
for i in range(1, 5):  # for full simulation
    avg_pmt_rate = str(10 ** i)
    show_2d_graph(
        sys.argv[1],
        "payment_rate",
        "success_rate",
        "routing_method",
        {
            "average_payment_amount": [avg_pmt_rate],
            "group_cap_update": ["true", ""],
            "routing_method": ["ideal", "channel_update", "group_routing"]
        },
        x_logarithmic_scale=True,
        x_axis="Log base 10 transactions per sec [satoshi]",
        y_axis="Success rate",
        title=f"success_rate (avg_pmt_amt={avg_pmt_rate})",
    )
    show_2d_graph(
        sys.argv[1],
        "payment_rate",
        "time/average",
        series_key="routing_method",
        errorbar_bottom_key="time/5-percentile",
        errorbar_top_key="time/95-percentile",
        fix={
            "average_payment_amount": [avg_pmt_rate],
            "group_cap_update": ["true", ""],
            "routing_method": ["ideal", "channel_update", "group_routing"]
        },
        x_logarithmic_scale=True,
        x_axis="Log base 10 transactions per sec [satoshi]",
        y_axis="Time(average,errorbar=5-95percentile) [ms]",
        title=f"time (avg_pmt_amt={avg_pmt_rate})",
    )
    show_2d_graph(
        sys.argv[1],
        "payment_rate",
        "retry/average",
        errorbar_bottom_key="retry/5-percentile",
        errorbar_top_key="retry/95-percentile",
        series_key="routing_method",
        fix={
            "average_payment_amount": [avg_pmt_rate],
            "group_cap_update": ["true", ""],
            "routing_method": ["ideal", "channel_update", "group_routing"]
        },
        x_logarithmic_scale=True,
        x_axis="Log base 10 transactions per sec [satoshi]",
        y_axis="Retry num(average,errorbar=5-95percentile)",
        title=f"retry_num (avg_pmt_amt={avg_pmt_rate})",
    )
    show_2d_graph(
        sys.argv[1],
        "payment_rate",
        "total_channel_locked_time/average",
        errorbar_bottom_key="total_channel_locked_time/5-percentile",
        errorbar_top_key="total_channel_locked_time/95-percentile",
        series_key="routing_method",
        fix={
            "average_payment_amount": [avg_pmt_rate],
            "group_cap_update": ["true", ""],
            "routing_method": ["ideal", "channel_update", "group_routing"]
        },
        x_logarithmic_scale=True,
        x_axis="Log base 10 transactions per sec [satoshi]",
        y_axis="total_channel_locked_time(average,errorbar=5-95percentile) [ms]",
        title=f"total_channel_locked_time (avg_pmt_amt={avg_pmt_rate})",
    )
    show_2d_graph(
        sys.argv[1],
        "payment_rate",
        "route_len/average",
        series_key="routing_method",
        fix={
            "average_payment_amount": [avg_pmt_rate],
            "group_cap_update": ["true", ""],
            "routing_method": ["ideal", "channel_update", "group_routing"]
        },
        x_logarithmic_scale=True,
        x_axis="Log base 10 transactions per sec [satoshi]",
        y_axis="route_len",
        title=f"route_len (avg_pmt_amt={avg_pmt_rate})",
    )
    show_2d_graph(
        sys.argv[1],
        "payment_rate",
        "fail_timeout_rate",
        series_key="routing_method",
        fix={
            "average_payment_amount": [avg_pmt_rate],
            "group_cap_update": ["true", ""],
            "routing_method": ["ideal", "channel_update", "group_routing"]
        },
        x_logarithmic_scale=True,
        x_axis="Log base 10 transactions per sec [satoshi]",
        y_axis="fail_timeout_rate",
        title=f"fail_timeout_rate (avg_pmt_amt={avg_pmt_rate})",
    )
