#!/bin/bash

if [[ "$#" -ne 1 ]]; then
  echo "./run_all_simulations.sh <output_dir>"
  exit 0
fi

output_dir="$1/$(date "+%Y%m%d%H%M%S")"
mkdir "$output_dir"

seed=39
max_processes=8

queue=()
running_processes=0
completed_simulations=0
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
}

function display_progress() {
    local progress=$((completed_simulations * 100 / total_simulations))
    local progress_bar=$(printf "[%-${progress}s%-$((100 - progress))s]" "#" "")

    if [ "$progress" -eq 0 ]; then
        printf "\rProgress: [%-50s] %d%%\t Time remaining --:--\t" $(printf "%0.s#" $(seq 1 $((progress / 2)))) "$progress"
    else
        local elapsed_time=$(( $(date +%s) - start_time ))
        local estimated_completion_time=$(( elapsed_time * 100 / progress - elapsed_time ))
        local remaining_minutes=$(( estimated_completion_time / 60 ))
        local remaining_seconds=$(( estimated_completion_time % 60 ))
        printf "\rProgress: [%-50s] %d%%\t Time remaining %02d:%02d\t" $(printf "%0.s#" $(seq 1 $((progress / 2)))) "$progress" "$remaining_minutes" "$remaining_seconds"
    fi
}

for ((i = 1; i <= 5; i++)); do
    ave_pmt_amt=$((10 ** i))
    for ((j = 1; j <= 5; j++)); do
        var_pmt_amt=$((10 ** j))

        for ((k = 2; k <= 10; k++)); do
            group_size="$k"
            enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=false/average_payment_amount=$ave_pmt_amt/variance_payment_amount=$var_pmt_amt                       log_broadcast_msg=true enable_group_routing=false                       average_payment_amount=$ave_pmt_amt variance_payment_amount=$var_pmt_amt group_size=$group_size"
            enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=true/group_cap_update=true/average_payment_amount=$ave_pmt_amt/variance_payment_amount=$var_pmt_amt  log_broadcast_msg=true enable_group_routing=true group_cap_update=true  average_payment_amount=$ave_pmt_amt variance_payment_amount=$var_pmt_amt group_size=$group_size"
            enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=true/group_cap_update=false/average_payment_amount=$ave_pmt_amt/variance_payment_amount=$var_pmt_amt log_broadcast_msg=true enable_group_routing=true group_cap_update=false average_payment_amount=$ave_pmt_amt variance_payment_amount=$var_pmt_amt group_size=$group_size"
        done

        for k in $(seq -3.0 0.5 1.0); do
            group_limit_rate=$(python3 -c "print('{:.4f}'.format(10**$k))")
            enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=false/average_payment_amount=$ave_pmt_amt/variance_payment_amount=$var_pmt_amt                       log_broadcast_msg=true enable_group_routing=false                       average_payment_amount=$ave_pmt_amt variance_payment_amount=$var_pmt_amt group_limit_rate=$group_limit_rate(10^$k)"
            enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=true/group_cap_update=true/average_payment_amount=$ave_pmt_amt/variance_payment_amount=$var_pmt_amt  log_broadcast_msg=true enable_group_routing=true group_cap_update=true  average_payment_amount=$ave_pmt_amt variance_payment_amount=$var_pmt_amt group_limit_rate=$group_limit_rate(10^$k)"
            enqueue_simulation "./run-simulation.sh $seed $output_dir/enable_group_routing=true/group_cap_update=false/average_payment_amount=$ave_pmt_amt/variance_payment_amount=$var_pmt_amt log_broadcast_msg=true enable_group_routing=true group_cap_update=false average_payment_amount=$ave_pmt_amt variance_payment_amount=$var_pmt_amt group_limit_rate=$group_limit_rate(10^$k)"
        done
    done
done

# Process the queue
display_progress
while [ "${#queue[@]}" -gt 0 ] || [ "$running_processes" -gt 0 ]; do
    process_queue
    display_progress
    sleep 1
done

python3 gen_csv_summary.py "$output_dir"
echo -e "\nAll simulations have completed."
