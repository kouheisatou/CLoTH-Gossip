#ifndef CLOTH_H
#define CLOTH_H

#include <stdint.h>
#include "heap.h"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

enum routing_method {
    CLOTH_ORIGINAL,
    CHANNEL_UPDATE,
    GROUP_ROUTING,
    IDEAL,
    GROUP_ROUTING_CUL
};

struct network_params {
    long n_nodes;
    long n_channels;
    long capacity_per_channel;
    double faulty_node_prob;

    /**
     * Possible values: true or false.
     * It indicates whether the network of the simulation is generated randomly (generate_network_from_file=false) or it is taken from csv files (generate_network_from_file=true).
     */
    unsigned int network_from_file;

    /**
     * In case generate_network_from_file=true, the names of the csv files where nodes of the network are taken from.
     * See the templates of these files in nodes_template.csv.
     */
    char nodes_filename[256];

    /**
     * In case generate_network_from_file=true, the names of the csv files where channels of the network are taken from.
     * See the templates of these files in channels_template.csv.
     */
    char channels_filename[256];

    /**
     * In case generate_network_from_file=true, the names of the csv files where edges of the network are taken from.
     * See the templates of these files in edges_template.csv.
     */
    char edges_filename[256];

    /**
     * ネットワークからの送金を行う際のタイムアウト時間 [ms]
     * -1を設定すると送金タイムアウトを無効化する
     * タイムアウトすると送金失敗し、payments_output.csvのis_timeoutが1になる
     */
    unsigned int payment_timeout; // set -1 to disable payment timeout

    /**
     * 送金の平均遅延時間 [ms]
     * 送金の遅延時間は平均値と分散で決定される
     */
    unsigned int average_payment_forward_interval;

    /**
     * 送金の遅延時間の分散 [ms]
     */
    unsigned int variance_payment_forward_interval;

    /**
     * ルーティング方法
     * CLOTH_ORIGINAL: CLoTHに初期実装されていたルーティング。LNのルーティングを忠実に再現したもの。
     * CHANNEL_UPDATE: LNのchannel_updateを利用したルーティング（と思っていたもの）。間違っているので未使用。
     * GROUP_ROUTING: edgeグループを用いたルーティング。min_cap_limitとmax_cap_limitをグループごとに設定し、グループ内のedgeはこの範囲を必ず満たす。
     * IDEAL: edgeの残高をそのままルーティングに利用する。最大性能比較用手法なので、プライバシーを一切考慮していない。
     * GROUP_ROUTING_CUL: edgeグループを用いたルーティング。edgeごとにcul_threshold_factorを設定し、グループに属するedgeのCULは必ずこの閾値以下になるようにする。min_cap_limitとmax_cap_limitは使わない。
     */
    enum routing_method routing_method;

    /**
     * グループ内のedgeで送金が行われた時に毎回グループ更新を行うか否か
     * falseの場合、グループ構築時のみgroup_capをが計算される
     * MUST be set if routing_method is GROUP_ROUTING or GROUP_ROUTING_CUL
     */
    unsigned int group_cap_update;

    /**
     * グループ更新メッセージのブロードキャスト遅延
     * MUST be set if routing_method is GROUP_ROUTING or GROUP_ROUTING_CUL
     */
    unsigned int group_broadcast_delay;

    /**
     * グループに属するedge数
     * MUST be set if routing_method is GROUP_ROUTING or GROUP_ROUTING_CUL
     */
    int group_size;

    /**
     * グループのmax_cap_limitとmin_cap_limitをグループ構築者の構築時balanceから比率で機械的に決定する
     * max_cap_limit = constructor_balance * (1 + group_limit_rate)
     * min_cap_limit = constructor_balance * (1 - group_limit_rate)
     * MUST be set if routing_method is GROUP_ROUTING
     */
    float group_limit_rate;

    /**
     * グループ更新時、自リンクが2連続でグループの最小値だった場合、プライバシー向上のため、嘘の値でグループ容量を更新する
     * if set to 1, update group cap with fake value
     * MUST be set if routing_method is GROUP_ROUTING or GROUP_ROUTING_CUL
     */
    unsigned int enable_fake_balance_update;

    /**
     * GROUP_ROUTING_CULで利用するとき、edgeごとに設定されるcul_threshold_factorをランダムに設定するためのベータ分布パラメータα
     * MUST be set if generate_from_network is true
     * generate_from_network=falseかつ指定しない場合、cul_threshold_factorはedges_ln.csvの値を利用される
     */
    double cul_threshold_factor_dist_alpha;

    /**
     * GROUP_ROUTING_CULで利用するとき、edgeごとに設定されるcul_threshold_factorをランダムに設定するためのベータ分布パラメータβ
     * MUST be set if generate_from_network is true
     * generate_from_network=falseかつ指定しない場合、cul_threshold_factorはedges_ln.csvの値を利用される
     */
    double cul_threshold_factor_dist_beta;
};

struct payments_params {
    double inverse_payment_rate;
    long n_payments;
    double amount_mu; // average_payment_amount [satoshi]
    double amount_sigma; // variance_payment_amount [satoshi]
    unsigned int payments_from_file;
    char payments_filename[256];
    unsigned int mpp;
    double max_fee_limit_mu; // average_max_fee_limit [satoshi]
    double max_fee_limit_sigma; // variance_max_fee_limit [satoshi]
};

struct simulation {
    uint64_t current_time; //milliseconds
    struct heap *events;
    gsl_rng *random_generator;
};

#endif
