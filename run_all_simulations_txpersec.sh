#!/bin/bash

if [[ "$#" -lt 1 ]]; then
  echo "./run_all_simulations.sh <seed> <output_dir> <import_from_dijkstra_cache_dir>"
  exit 0
fi

seed="$1"

output_dir="$2/$(date "+%Y%m%d%H%M%S")"
mkdir "$output_dir"

dijkstra_cache_dir="$output_dir/dijkstra_cache"
mkdir "$dijkstra_cache_dir"
if [ "$#" -eq 3 ]; then
    cp -r "$3/." "$dijkstra_cache_dir/"
fi

max_processes=32

queue=()
running_processes=0
completed_simulations=0
completed_simulations_tmp_file="$output_dir/completed_simulations"
total_simulations=0
start_time=$(date +%s)

function enqueue_simulation() {
    queue+=("$@")
    ((total_simulations++))
}

function process_queue() {
    while [ "$running_processes" -lt "$max_processes" ] && [ "${#queue[@]}" -gt 0 ]; do
        eval "${queue[0]}" > /dev/null 2>&1 &
        queue=("${queue[@]:1}")
        ((running_processes++))
    done

    wait -n || true
    ((running_processes--))
    ((completed_simulations++))
    echo "$completed_simulations" > "$completed_simulations_tmp_file"
}

function display_progress() {
    if [ "$total_simulations" -eq 0 ]; then
        return 0
    fi

    last_updated=0
    last_estimated_time=0
    count=0

    completed_simulations_from_tmp_file=0
    echo "$completed_simulations_from_tmp_file" > "$completed_simulations_tmp_file"

    while [ "$completed_simulations_from_tmp_file" -lt "$total_simulations" ]; do

        text=$(cat "$completed_simulations_tmp_file")
        if [ -n "$text" ]; then
            completed_simulations_from_tmp_file="$text"
        fi

        progress=$(python3 -c "print($completed_simulations_from_tmp_file / $total_simulations)")
        progress_bar=$(printf "%0.s#" $(seq 1 $((completed_simulations_from_tmp_file * 100 / total_simulations / 2))))

        if [ "$completed_simulations_from_tmp_file" -eq 0 ]; then
            printf "\rProgress: [%-50s] 0%%\t%d/%d\t Time remaining --:--\t" "" "$completed_simulations_from_tmp_file" "$total_simulations"
        else
            elapsed_time=$(( $(date +%s) - start_time ))
            estimated_completion_time=$(python3 -c "print(int($elapsed_time / $progress - $elapsed_time))")
            if [ "$completed_simulations_from_tmp_file" -eq "$last_updated" ]; then
                estimated_completion_time=$((last_estimated_time - count))
                count=$((count + 1))
            else
                last_updated="$completed_simulations_from_tmp_file"
                last_estimated_time="$estimated_completion_time"
                count=0
            fi
            remaining_minutes=$(( estimated_completion_time / 60 ))
            remaining_seconds=$(( estimated_completion_time % 60 ))
            printf "\rProgress: [%-50s] %0.1f%%\t%d/%d\t Time remaining %02d:%02d          " "$progress_bar" "$(echo "scale=1; $progress * 100" | bc)" "$completed_simulations_from_tmp_file" "$total_simulations" "$remaining_minutes" "$remaining_seconds"
        fi
        sleep 1
    done
}

#avg_pmt_amt="10000"  # value such that success_rate=0.8~0.6 when payment_rate=10
#avg_pmt_amt="1000"  # value such that success_rate=0.95 when payment_rate=10, 948.9813JPY(2024/05/12)
var_pmt_amt="10000"
avg_pmt_amt="44700"  # based on statics https://river.com/learn/files/river-lightning-report-2023.pdf?ref=blog.river.com, $11.84(August 2023)
n_payments="50000"  # based on simulation settings used by CLoTH paper https://www.sciencedirect.com/science/article/pii/S2352711021000613
#for j in $(seq 1.0 0.5 5.0); do
for j in $(seq 0.0 0.2 2.8); do
    payment_rate=$(python3 -c "print('{:.0f}'.format(10**($j)))")
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=ideal/payment_rate=$payment_rate             $dijkstra_cache_dir/method=ideal,seed=$seed,n_payments=$n_payments,avg_pmt_amt=$avg_pmt_amt,var_pmt_amt=$var_pmt_amt           payment_rate=$payment_rate n_payments=$n_payments mpp=0 routing_method=ideal          group_cap_update=        average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_size=  group_limit_rate="
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=channel_update/payment_rate=$payment_rate    $dijkstra_cache_dir/method=channel_update,seed=$seed,n_payments=$n_payments,avg_pmt_amt=$avg_pmt_amt,var_pmt_amt=$var_pmt_amt  payment_rate=$payment_rate n_payments=$n_payments mpp=0 routing_method=channel_update group_cap_update=        average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_size=  group_limit_rate="
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=group_routing/payment_rate=$payment_rate     $dijkstra_cache_dir/method=group_routing,seed=$seed,n_payments=$n_payments,avg_pmt_amt=$avg_pmt_amt,var_pmt_amt=$var_pmt_amt   payment_rate=$payment_rate n_payments=$n_payments mpp=0 routing_method=group_routing  group_cap_update=true    average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_size=5 group_limit_rate=0.1"
#    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=cloth_original/payment_rate=$payment_rate    $dijkstra_cache_dir/method=cloth_original,seed=$seed,n_payments=$n_payments,avg_pmt_amt=$avg_pmt_amt,var_pmt_amt=$var_pmt_amt  payment_rate=$payment_rate n_payments=$n_payments mpp=0 routing_method=cloth_original group_cap_update=        average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_size=  group_limit_rate="
done
for j in $(seq 3.0 0.05 5.0); do
    payment_rate=$(python3 -c "print('{:.0f}'.format(10**($j)))")
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=ideal/payment_rate=$payment_rate             $dijkstra_cache_dir/method=ideal,seed=$seed,n_payments=$n_payments,avg_pmt_amt=$avg_pmt_amt,var_pmt_amt=$var_pmt_amt           payment_rate=$payment_rate n_payments=$n_payments mpp=0 routing_method=ideal          group_cap_update=        average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_size=  group_limit_rate="
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=channel_update/payment_rate=$payment_rate    $dijkstra_cache_dir/method=channel_update,seed=$seed,n_payments=$n_payments,avg_pmt_amt=$avg_pmt_amt,var_pmt_amt=$var_pmt_amt  payment_rate=$payment_rate n_payments=$n_payments mpp=0 routing_method=channel_update group_cap_update=        average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_size=  group_limit_rate="
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=group_routing/payment_rate=$payment_rate     $dijkstra_cache_dir/method=group_routing,seed=$seed,n_payments=$n_payments,avg_pmt_amt=$avg_pmt_amt,var_pmt_amt=$var_pmt_amt   payment_rate=$payment_rate n_payments=$n_payments mpp=0 routing_method=group_routing  group_cap_update=true    average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_size=5 group_limit_rate=0.1"
#    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=cloth_original/payment_rate=$payment_rate    $dijkstra_cache_dir/method=cloth_original,seed=$seed,n_payments=$n_payments,avg_pmt_amt=$avg_pmt_amt,var_pmt_amt=$var_pmt_amt  payment_rate=$payment_rate n_payments=$n_payments mpp=0 routing_method=cloth_original group_cap_update=        average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_size=  group_limit_rate="
done

# Process the queue
display_progress &
while [ "${#queue[@]}" -gt 0 ] || [ "$running_processes" -gt 0 ]; do
    process_queue
    sleep 1
done
wait
echo -e "\nAll simulations have completed. \nOutputs saved at $output_dir"
python3 scripts/analyze_output_and_summarize.py "$output_dir"
end_time=$(date +%s)
echo "START : $(date --date @"$start_time")"
echo "  END : $(date --date @"$end_time")"
echo " TIME : $((end_time - start_time))"
