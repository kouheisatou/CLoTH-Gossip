if [[ "$#" -ne 1 ]]; then
  echo "./run_all_simulations.sh <output_dir>"
  exit 0
fi



output_dir="$1/$(date "+%Y%m%d%H%M%S")"
mkdir "$output_dir"
tmp_file="$output_dir/temp.txt"

seed=39
max_processes=8
all_tasks=53



get_value_from_tmp() {
    touch "$tmp_file"
    key=$1
    value=$(grep "^$key=" "$tmp_file" | cut -d '=' -f2)
    if [ -n "$value" ]; then
        :
    else
        value=0
    fi
    echo "$value"
}

set_value_to_tmp() {
    touch "$tmp_file"
    key=$1
    value=$2
    if grep -q "^$key=" "$tmp_file"; then
        sed -i -e "s/$key=.*/$key=$value/" "$tmp_file"
    else
        echo "$key=$value" >> "$tmp_file"
    fi
}

function run_simulation_background() {

    while [ "$(($(get_value_from_tmp n_process)+1))" -gt "$max_processes" ]; do
        sleep 5
    done

    set_value_to_tmp n_process $(($(get_value_from_tmp n_process)+1))
    echo "simulation starts on threads:$(($(get_value_from_tmp n_process)))/$max_processes $2"
    ./run-simulation.sh "$@" > /dev/null 2>&1
    set_value_to_tmp n_process $(($(get_value_from_tmp n_process)-1))

    python3 gen_csv_summary.py "$2/.."
    set_value_to_tmp n_done $(($(get_value_from_tmp n_done)+1))
}


change_target="group_limit_rate"
result_root_dir_1="$output_dir/change_$change_target"
for ((i = 1; i <= 20; i++)); do
  value=$(printf "%.1f" "$(echo "scale=1; $i / 10" | bc)")
  run_simulation_background "$seed" "$result_root_dir_1/$change_target=$value" "$change_target=$value" &
  sleep 5
done


change_target="group_size"
result_root_dir_2="$output_dir/change_$change_target"
for ((i = 2; i <= 20; i++)); do
  run_simulation_background "$seed" "$result_root_dir_2/$change_target=$i" "$change_target=$i" &
  sleep 5
done


change_target="average_payment_amount"
result_root_dir_3="$output_dir/change_$change_target/enable_group_routing=false"
for ((i = 0; i <= 20; i++)); do
  value=$((i*1000))
  if [ "$i" -eq 0 ]; then
      value=100
  fi
  run_simulation_background "$seed" "$result_root_dir_3/$change_target=$value" "enable_group_routing=false" "$change_target=$value" &
  sleep 5
done

change_target="average_payment_amount"
result_root_dir_4="$output_dir/change_$change_target/enable_group_routing=true/group_cap_update=true"
for ((i = 0; i <= 20; i++)); do
  value=$((i*1000))
  if [ "$i" -eq 0 ]; then
      value=100
  fi
  run_simulation_background "$seed" "$result_root_dir_4/$change_target=$value" "enable_group_routing=true" "group_cap_update=true" "$change_target=$value" &
  sleep 5
done

change_target="average_payment_amount"
result_root_dir_5="$output_dir/change_$change_target/enable_group_routing=true/group_cap_update=false"
for ((i = 0; i <= 20; i++)); do
  value=$((i*1000))
  if [ "$i" -eq 0 ]; then
      value=100
  fi
  run_simulation_background "$seed" "$result_root_dir_5/$change_target=$value" "enable_group_routing=true" "group_cap_update=false" "$change_target=$value" &
  sleep 5
done


echo "Waiting until all simulations done."
while [ "$(get_value_from_tmp n_done)" -lt "$all_tasks" ]; do
    sleep 5
    echo -e "\033[A"
    printf "done:$(get_value_from_tmp n_done)/$all_tasks\t processe:$(get_value_from_tmp n_process)/$max_processes\t"
done

echo "All simulations have completed."
rm "$tmp_file"