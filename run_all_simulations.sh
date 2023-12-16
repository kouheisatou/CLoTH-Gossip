if [[ "$#" -ne 1 ]]; then
  echo "./run_all_simulations.sh <output_dir>"
  exit 0
fi

seed=39

output_dir="$1/$(date "+%Y%m%d%H%M%S")"

function run_simulation_background() {

    max_processes=4
    running_processes=$(pgrep -c -f "./CLoTH_Gossip")

    while [ "$running_processes" -ge "$max_processes" ]; do
        sleep 5
        running_processes=$(pgrep -c -f "./CLoTH_Gossip")
    done

    echo "simulation starts on threads:$running_processes/$max_processes $2"
    ./run-simulation.sh "$@" > /dev/null 2>&1

    python3 gen_csv_summary.py "$2/.."
}

change_target="group_limit_rate"
result_root_dir_1="$output_dir/change_$change_target"
for ((i = 1; i <= 10; i++)); do
  value=$(printf "%.1f" "$(echo "scale=1; $i / 10" | bc)")
  run_simulation_background "$seed" "$result_root_dir_1/$change_target=$value" "$change_target=$value" &
  sleep 5
done


change_target="group_size"
result_root_dir_2="$output_dir/change_$change_target"
for ((i = 1; i <= 10; i++)); do
  run_simulation_background "$seed" "$result_root_dir_2/$change_target=$i" "$change_target=$i" &
  sleep 5
done


change_target="average_payment_amount"
result_root_dir_3="$output_dir/change_$change_target/enable_group_routing=false"
for ((i = 0; i <= 10; i++)); do
  value=$((i*1000))
  if [ "$i" -eq 0 ]; then
      value=100
  fi
  run_simulation_background "$seed" "$result_root_dir_3/$change_target=$value" "enable_group_routing=false" "$change_target=$value" &
  sleep 5
done

change_target="average_payment_amount"
result_root_dir_4="$output_dir/change_$change_target/enable_group_routing=true/group_cap_update=true"
for ((i = 0; i <= 10; i++)); do
  value=$((i*1000))
  if [ "$i" -eq 0 ]; then
      value=100
  fi
  run_simulation_background "$seed" "$result_root_dir_4/$change_target=$value" "enable_group_routing=true" "group_cap_update=true" "$change_target=$value" &
  sleep 5
done

change_target="average_payment_amount"
result_root_dir_5="$output_dir/change_$change_target/enable_group_routing=true/group_cap_update=false"
for ((i = 0; i <= 10; i++)); do
  value=$((i*1000))
  if [ "$i" -eq 0 ]; then
      value=100
  fi
  run_simulation_background "$seed" "$result_root_dir_5/$change_target=$value" "enable_group_routing=true" "group_cap_update=false" "$change_target=$value" &
  sleep 5
done
