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
    printf "\rProgress: [%-50s] %d%%" $(printf "%0.s#" $(seq 1 $((progress / 2)))) "$progress"
}

# Simulation 1
change_target="group_limit_rate"
result_root_dir="$output_dir/change_$change_target"
for ((i = 1; i <= 20; i++)); do
    value=$(printf "%.1f" "$(echo "scale=1; $i / 10" | bc)")
    enqueue_simulation "./run-simulation.sh $seed \"$result_root_dir/$change_target=$value\" \"$change_target=$value\"; python3 gen_csv_summary.py $result_root_dir;"
done

# Simulation 2
change_target="group_size"
result_root_dir="$output_dir/change_$change_target"
for ((i = 2; i <= 20; i++)); do
    enqueue_simulation "./run-simulation.sh $seed \"$result_root_dir/$change_target=$i\" \"$change_target=$i\""
done

# Simulation 3
change_target="average_payment_amount"
result_root_dir="$output_dir/change_$change_target/enable_group_routing=false"
for ((i = 0; i <= 10; i++)); do
    value=$((i*1000))
    if [ "$i" -eq 0 ]; then
        value=100
    fi
    enqueue_simulation "./run-simulation.sh $seed \"$result_root_dir/$change_target=$value\" \"enable_group_routing=false\" \"$change_target=$value\"; python3 gen_csv_summary.py $result_root_dir;"
done

# Simulation 4
change_target="average_payment_amount"
result_root_dir="$output_dir/change_$change_target/enable_group_routing=true/group_cap_update=true"
for ((i = 0; i <= 10; i++)); do
    value=$((i*1000))
    if [ "$i" -eq 0 ]; then
        value=100
    fi
    enqueue_simulation "./run-simulation.sh $seed \"$result_root_dir/$change_target=$value\" \"enable_group_routing=true\" \"group_cap_update=true\" \"$change_target=$value\"; python3 gen_csv_summary.py $result_root_dir;"
done

# Simulation 5
change_target="average_payment_amount"
result_root_dir="$output_dir/change_$change_target/enable_group_routing=true/group_cap_update=false"
for ((i = 0; i <= 10; i++)); do
    value=$((i*1000))
    if [ "$i" -eq 0 ]; then
        value=100
    fi
    enqueue_simulation "./run-simulation.sh $seed \"$result_root_dir/$change_target=$value\" \"enable_group_routing=true\" \"group_cap_update=false\" \"$change_target=$value\"; python3 gen_csv_summary.py $result_root_dir;"
done

# Process the queue
display_progress
while [ "${#queue[@]}" -gt 0 ] || [ "$running_processes" -gt 0 ]; do
    process_queue
    display_progress
    sleep 5
done

echo -e "\nAll simulations have completed."
