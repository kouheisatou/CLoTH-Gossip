#!/bin/bash

if [[ "$#" -lt 1 ]]; then
  echo "./run_all_simulations.sh <output_dir> <import_from_dijkstra_cache_dir>"
  exit 0
fi

output_dir="$1/$(date "+%Y%m%d%H%M%S")"
mkdir "$output_dir"

dijkstra_cache_dir="$output_dir/dijkstra_cache"
mkdir "$dijkstra_cache_dir"
if [ "$#" -eq 2 ]; then
    cp -r "$2/." "$dijkstra_cache_dir/"
fi

seed=39
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
            printf "\rProgress: [%-50s] %0.1f%%\t%d/%d\t Time remaining %02d:%02d\t" "$progress_bar" "$(echo "scale=1; $progress * 100" | bc)" "$completed_simulations_from_tmp_file" "$total_simulations" "$remaining_minutes" "$remaining_seconds"
        fi
        sleep 1
    done
}

for i in $(seq 0 100 5000); do
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=group_routing/average_payment_amount=100/broadcast_delay=$i     $dijkstra_cache_dir/method=group_routing,avg_pmt_amt=100     group_broadcast_delay=$i n_payments=50000 mpp=0 routing_method=group_routing   average_payment_amount=100     "
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=group_routing/average_payment_amount=1000/broadcast_delay=$i    $dijkstra_cache_dir/method=group_routing,avg_pmt_amt=1000    group_broadcast_delay=$i n_payments=50000 mpp=0 routing_method=group_routing   average_payment_amount=1000    "
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=group_routing/average_payment_amount=10000/broadcast_delay=$i   $dijkstra_cache_dir/method=group_routing,avg_pmt_amt=10000   group_broadcast_delay=$i n_payments=50000 mpp=0 routing_method=group_routing   average_payment_amount=10000   "
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=group_routing/average_payment_amount=100000/broadcast_delay=$i  $dijkstra_cache_dir/method=group_routing,avg_pmt_amt=100000  group_broadcast_delay=$i n_payments=50000 mpp=0 routing_method=group_routing   average_payment_amount=100000  "
done

# Process the queue
display_progress &
while [ "${#queue[@]}" -gt 0 ] || [ "$running_processes" -gt 0 ]; do
    process_queue
    sleep 1
done
wait
echo -e "\nAll simulations have completed."
python3 scripts/analyze_output_and_summarize.py "$output_dir"
echo "START : $(date --date @"$start_time")"
echo "  END : $(date)"
