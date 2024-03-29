import csv
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


def show_3d_graph(csv_file: str, x_key: str, y_key: str, z_key, fix: dict, x_logarithmic_scale: bool = False, y_logarithmic_scale: bool = False, z_logarithmic_scale: bool = False, x_axis: str = None, y_axis: str = None, z_axis: str = None, title: str = None):
    lines_data = {}
    for line in csv_to_dict_list(csv_file):

        # err check
        if (x_key not in line) or (y_key not in line) or (x_key in fix) or (y_key in fix) or (z_key in fix):
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
            # z = float(line[z_key])
            z = float(line[z_key]) - 1.0 if z_key == "Attempts.Mean" else float(line[z_key])
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


def show_2d_graph(csv_file: str, x_key: str, y_key: str, series_key: str, fix: dict, x_logarithmic_scale: bool = False, y_logarithmic_scale: bool = False, x_axis: str = None, y_axis: str = None, title: str = None):
    lines_data = {}
    for line in csv_to_dict_list(csv_file):

        if (x_key not in line) or (y_key not in line) or (x_key in fix) or (y_key in fix):
            exit(-1)

        hit = True
        for key in line:
            for fix_key in fix:
                if key == fix_key and line[key] not in fix[fix_key]:
                    hit = False

        if hit:
            x = float(line[x_key])
            # y = float(line[y_key])
            y = float(line[y_key]) - 1.0 if y_key == "Attempts.Mean" else float(line[y_key])
            series_value = line[series_key]
            if series_value not in lines_data:
                lines_data[series_value] = []
            lines_data[series_value].append((x, y))

    lines_data = dict(sorted({key: sorted(values, key=lambda _x: _x[0]) for key, values in lines_data.items()}.items()))
    print(lines_data)

    fig, ax = plt.subplots()
    if x_logarithmic_scale:
        ax.set_xscale("log")
    if y_logarithmic_scale:
        ax.set_yscale("log")

    count = 0
    for series_value, line_data in lines_data.items():
        x, y = zip(*line_data)
        ax.plot(x, y, label=f'{series_key}={series_value}', marker=markers[count % len(markers)])
        count += 1

    ax.set_xlabel(x_key if x_axis is None else x_axis)
    ax.set_ylabel(y_key if y_axis is None else y_axis)
    ax.set_title(y_key + "\n" + str(fix) if title is None else title)

    plt.minorticks_on()
    plt.grid()

    ax.legend()
    plt.show()


show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_size",
    "Success.Mean",
    {
        "routing_method": ["group_routing"],
        "group_limit_rate": ["0.1000"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=False,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group size",
    z_axis="Success rate",
    title="",
)
show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_size",
    "FailNoPath.Mean",
    {
        "routing_method": ["group_routing"],
        "group_limit_rate": ["0.1000"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=False,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group size",
    z_axis="Fail no path rate",
    title="",
)
show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_size",
    "FailNoBalance.Mean",
    {
        "routing_method": ["group_routing"],
        "group_limit_rate": ["0.1000"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=False,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group size",
    z_axis="Fail no balance rate",
    title="",
)
show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_size",
    "Attempts.Mean",
    {
        "routing_method": ["group_routing"],
        "group_limit_rate": ["0.1000"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=False,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group size",
    z_axis="Average number of retries",
    title="",
)
show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_size",
    "group.group_cover_rate",
    {
        "routing_method": ["group_routing"],
        "group_limit_rate": ["0.1000"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=False,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group size",
    z_axis="Group cover rate",
    title="",
)
show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_size",
    "group.accuracy.mean",
    {
        "routing_method": ["group_routing"],
        "group_limit_rate": ["0.1000"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=False,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group size",
    z_axis="Accuracy",
    title="",
)

show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_limit_rate",
    "Success.Mean",
    {
        "routing_method": ["group_routing"],
        "group_size": ["5"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=True,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group limit rate",
    z_axis="Success rate",
    title="",
)
show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_limit_rate",
    "FailNoPath.Mean",
    {
        "routing_method": ["group_routing"],
        "group_size": ["5"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=True,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group limit rate",
    z_axis="Fail no path rate",
    title="",
)
show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_limit_rate",
    "FailNoBalance.Mean",
    {
        "routing_method": ["group_routing"],
        "group_size": ["5"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=True,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group limit rate",
    z_axis="Fail no balance rate",
    title="",
)
show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_limit_rate",
    "Attempts.Mean",
    {
        "routing_method": ["group_routing"],
        "group_size": ["5"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=True,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group limit rate",
    z_axis="Average number of retries",
    title="",
)
show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_limit_rate",
    "group.group_cover_rate",
    {
        "routing_method": ["group_routing"],
        "group_size": ["5"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=True,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group limit rate",
    z_axis="Group cover rate",
    title="",
)
show_3d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group_limit_rate",
    "group.accuracy.mean",
    {
        "routing_method": ["group_routing"],
        "group_size": ["5"],
        "group_cap_update": ["true"]
    },
    x_logarithmic_scale=True,
    y_logarithmic_scale=True,
    z_logarithmic_scale=False,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group limit rate",
    z_axis="Accuracy",
    title="",
)

show_2d_graph(
    sys.argv[1],
    "average_payment_amount",
    "Success.Mean",
    "routing_method",
    {
        "group_size": ["5", ""],
        "group_limit_rate": ["0.1000", ""],
        "group_cap_update": ["true", ""],
        "routing_method": ["ideal", "channel_update", "group_routing"]
    },
    x_logarithmic_scale=True,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Success rate",
    title="",
)
show_2d_graph(
    sys.argv[1],
    "average_payment_amount",
    "FailNoPath.Mean",
    "routing_method",
    {
        "group_size": ["5", ""],
        "group_limit_rate": ["0.1000", ""],
        "group_cap_update": ["true", ""],
        "routing_method": ["ideal", "channel_update", "group_routing"]
    },
    x_logarithmic_scale=True,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Fail no path rate",
    title="",
)
show_2d_graph(
    sys.argv[1],
    "average_payment_amount",
    "FailNoBalance.Mean",
    "routing_method",
    {
        "group_size": ["5", ""],
        "group_limit_rate": ["0.1000", ""],
        "group_cap_update": ["true", ""],
        "routing_method": ["ideal", "channel_update", "group_routing"]
    },
    x_logarithmic_scale=True,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Fail no balance rate",
    title="",
)
show_2d_graph(
    sys.argv[1],
    "average_payment_amount",
    "Attempts.Mean",
    "routing_method",
    {
        "group_size": ["5", ""],
        "group_limit_rate": ["0.1000", ""],
        "group_cap_update": ["true", ""],
        "routing_method": ["ideal", "channel_update", "group_routing"]
    },
    x_logarithmic_scale=True,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Average number of retries",
    title="",
)
show_2d_graph(
    sys.argv[1],
    "average_payment_amount",
    "RouteLength.Mean",
    "routing_method",
    {
        "group_size": ["5", ""],
        "group_limit_rate": ["0.1000", ""],
        "group_cap_update": ["true", ""],
        "routing_method": ["ideal", "channel_update", "group_routing"]
    },
    x_logarithmic_scale=True,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Average route length",
    title="",
)
show_2d_graph(
    sys.argv[1],
    "average_payment_amount",
    "Time.Mean",
    "routing_method",
    {
        "group_size": ["5", ""],
        "group_limit_rate": ["0.1000", ""],
        "group_cap_update": ["true", ""],
        "routing_method": ["ideal", "channel_update", "group_routing"]
    },
    x_logarithmic_scale=True,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Time [ms]",
    title="",
)
show_2d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group.accuracy.mean",
    "routing_method",
    {
        "group_size": ["5", ""],
        "group_limit_rate": ["0.1000"],
        "group_cap_update": ["true"],
        "routing_method": ["group_routing"]
    },
    x_logarithmic_scale=True,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Accuracy of group capacity",
    title="",
)
show_2d_graph(
    sys.argv[1],
    "average_payment_amount",
    "group.group_cover_rate",
    "routing_method",
    {
        "group_size": ["5", ""],
        "group_limit_rate": ["0.1000"],
        "group_cap_update": ["true"],
        "routing_method": ["group_routing"]
    },
    x_logarithmic_scale=True,
    x_axis="Log base 10 average payment amount [satoshi]",
    y_axis="Group cover rate",
    title="",
)
