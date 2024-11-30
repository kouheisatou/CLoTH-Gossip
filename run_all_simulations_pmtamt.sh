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
total_simulations=0
start_time=$(date +%s)

function enqueue_simulation() {
    queue+=("$@")
    ((total_simulations++))
}

function process_queue() {
    while [ "$running_processes" -lt "$max_processes" ] && [ "${#queue[@]}" -gt 0 ]; do
        eval "${queue[0]}" &
        queue=("${queue[@]:1}")
        ((running_processes++))
    done

    wait -n || true
    ((running_processes--))
}

function display_progress() {
    if [ "$total_simulations" -eq 0 ]; then
        return 0
    fi

    done_simulations=0

    while [ "$done_simulations" -lt "$total_simulations" ]; do
        progress_summary=""
        total_progress=0

        # read each simulation progresses
        IFS=$'\n' read -r -d '' -a simulation_progress_files <<< $(find "$output_dir" -type f -name "progress.tmp")
        done_simulations=0
        for file in "${simulation_progress_files[@]}"; do
            progress=$(cat "$file")
            if [ "$progress" = "1" ]; then
              done_simulations=$((done_simulations + 1))
            elif [ "$progress" = "" ]; then
              progress="0"
            fi
            total_progress=$(printf "%.5f" "$(echo "scale=4; $total_progress + $progress / $total_simulations" | bc)")
            progress_summary="$progress_summary$(printf "%3d%% %s" "$(printf "%.0f" "$(echo "$progress*100" | bc)")" "$file")\n"
        done

        # build progress bar
        progress_bar_len=$(printf "%0.s#" $(seq 1 $(printf "%.0f" "$(echo "$total_progress * 100 / 2" | bc)")))
        progress_bar=""
        if [ $(python3 -c "print($total_progress==0)") = "True" ]; then
            progress_bar=$(printf "Progress: [%-50s] 0%%\t%d/%d\t Time remaining --:--" "" "$done_simulations" "$total_simulations")
        else
            elapsed_time=$(( $(date +%s) - start_time ))
            estimated_completion_time=$(python3 -c "print(int($elapsed_time / $total_progress - $elapsed_time))")
            remaining_minutes=$(( estimated_completion_time / 60 ))
            remaining_seconds=$(( estimated_completion_time % 60 ))
            progress_bar=$(printf "Progress: [%-50s] %0.1f%%\t%d/%d\t Time remaining %02d:%02d" "$progress_bar_len" "$(echo "scale=1; $total_progress * 100" | bc)" "$done_simulations" "$total_simulations" "$remaining_minutes" "$remaining_seconds")
        fi

        echo -e "$progress_summary$progress_bar"
        sleep 1
    done
}

for i in $(seq 1.0 0.2 5.0); do

    avg_pmt_amt=$(python3 -c "print('{:.0f}'.format(10**$i))")
    group_size=5
    group_limit_rate=0.1

    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=group_routing/average_payment_amount=$avg_pmt_amt     $dijkstra_cache_dir/method=group_routing,seed=$seed,avg_pmt_amt=$avg_pmt_amt     routing_method=group_routing  payment_timeout=-1 n_payments=50000 mpp=0 average_payment_amount=$avg_pmt_amt group_size=$group_size group_limit_rate=$group_limit_rate"
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=cloth_original/average_payment_amount=$avg_pmt_amt    $dijkstra_cache_dir/method=cloth_original,seed=$seed,avg_pmt_amt=$avg_pmt_amt    routing_method=cloth_original payment_timeout=-1 n_payments=50000 mpp=0 average_payment_amount=$avg_pmt_amt"
    enqueue_simulation "./run-simulation.sh $seed $output_dir/routing_method=ideal/average_payment_amount=$avg_pmt_amt             $dijkstra_cache_dir/method=ideal,seed=$seed,avg_pmt_amt=$avg_pmt_amt             routing_method=ideal          payment_timeout=-1 n_payments=50000 mpp=0 average_payment_amount=$avg_pmt_amt"

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
