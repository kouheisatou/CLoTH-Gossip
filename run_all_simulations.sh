#!/bin/bash

if [[ "$#" -ne 1 ]]; then
  echo "./run_all_simulations.sh <output_dir>"
  exit 0
fi

output_dir="$1/$(date "+%Y%m%d%H%M%S")"
install_dir="$(pwd)"
mkdir "$output_dir"

seed=39
max_processes=8

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
        completed_simulations_from_tmp_file=$(cat "$completed_simulations_tmp_file")
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

for i in $(seq 1.0 0.5 5.0); do
    avg_pmt_amt=$(python3 -c "print('{:.0f}'.format(10**$i))")
    var_pmt_amt=$((10 * avg_pmt_amt))

    enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=false/average_payment_amount=$avg_pmt_amt                                                                 $install_dir/dijkstra_cache_$avg_pmt_amt    log_broadcast_msg=true payment_rate=100 n_payments=50000 enable_group_routing=false                          average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt"

    for ((k = 2; k <= 10; k++)); do
        group_size="$k"
        enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=true/group_cap_update=true/average_payment_amount=$avg_pmt_amt/group_size=$group_size                 $install_dir/dijkstra_cache_$avg_pmt_amt    log_broadcast_msg=true payment_rate=100 n_payments=50000 enable_group_routing=true group_cap_update=true     average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_size=$group_size"
        enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=true/group_cap_update=false/average_payment_amount=$avg_pmt_amt/group_size=$group_size                $install_dir/dijkstra_cache_$avg_pmt_amt    log_broadcast_msg=true payment_rate=100 n_payments=50000 enable_group_routing=true group_cap_update=false    average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_size=$group_size"
    done

    for k in $(seq -3.0 0.5 1.0); do
        group_limit_rate=$(python3 -c "print('{:.4f}'.format(10**$k))")
        enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=true/group_cap_update=true/average_payment_amount=$avg_pmt_amt/group_limit_rate=$group_limit_rate     $install_dir/dijkstra_cache_$avg_pmt_amt    log_broadcast_msg=true payment_rate=100 n_payments=50000 enable_group_routing=true group_cap_update=true     average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_limit_rate=$group_limit_rate"
        enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=true/group_cap_update=false/average_payment_amount=$avg_pmt_amt/group_limit_rate=$group_limit_rate    $install_dir/dijkstra_cache_$avg_pmt_amt    log_broadcast_msg=true payment_rate=100 n_payments=50000 enable_group_routing=true group_cap_update=false    average_payment_amount=$avg_pmt_amt variance_payment_amount=$var_pmt_amt group_limit_rate=$group_limit_rate"
    done
done

# Process the queue
display_progress &
while [ "${#queue[@]}" -gt 0 ] || [ "$running_processes" -gt 0 ]; do
    process_queue
    sleep 1
done
wait
echo -e "\nAll simulations have completed."
python3 gen_csv_summary.py "$output_dir"
echo "START : $(date --date @"$start_time")"
echo "  END : $(date)"
