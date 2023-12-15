if [[ "$#" -ne 1 ]]; then
  echo "./run_all_simulations.sh <result_dir>"
  exit 0
fi

result_dir="$1/$(date "+%Y%m%d%H%M%S")"
root_dir=$(pwd)

change_target="group_limit_rate"
result_sub_dir="$result_dir/change_$change_target"
for ((i = 1; i <= 10; i++)); do
  cd "$root_dir"
  current=$(echo "scale=1; $i / 10" | bc)
  value=$(printf "%.1f" $current)
  folder_name="$result_sub_dir/$change_target=$value"
  environment_dir="$folder_name/environment"
  result_sub_sub_dir="$folder_name/result"
  mkdir -p "$environment_dir"
  mkdir -p "$result_sub_sub_dir"
  rsync -av -q --exclude='result' --exclude='cmake-build-debug' --exclude='cloth.dSYM' --exclude='.idea' --exclude='.git' --exclude='.cmake' "./" "$environment_dir"
  sed -i -e "s/$change_target=.*/$change_target=$value/" "$environment_dir/cloth_input.txt"
  cp "$environment_dir/cloth_input.txt" "$folder_name"
  cd "$environment_dir"
  cmake .
  make
  ./run-simulation.sh 39 "$result_sub_sub_dir/"
done
python3 gen_csv_summary.py result_sub_dir

change_target="group_size"
result_sub_dir="$result_dir/change_$change_target"
for ((i = 1; i <= 10; i++)); do
  cd "$root_dir"
  value="$i"
  folder_name="$result_sub_dir/$change_target=$value"
  environment_dir="$folder_name/environment"
  result_sub_sub_dir="$folder_name/result"
  mkdir -p "$environment_dir"
  mkdir -p "$result_sub_sub_dir"
  rsync -av -q --exclude='result' --exclude='cmake-build-debug' --exclude='cloth.dSYM' --exclude='.idea' --exclude='.git' --exclude='.cmake' "./" "$environment_dir"
  sed -i -e "s/$change_target=.*/$change_target=$value/" "$environment_dir/cloth_input.txt"
  cp "$environment_dir/cloth_input.txt" "$folder_name"
  cd "$environment_dir"
  cmake .
  make
  ./run-simulation.sh 39 "$result_sub_sub_dir/"
done
python3 gen_csv_summary.py result_sub_dir

change_target="average_payment_amount"
result_sub_dir="$result_dir/change_$change_target/enable_group_routing=true"
for ((i = 0; i <= 10; i++)); do
  cd "$root_dir"
  value=$((i*1000))
  if [ "$i" -eq 0 ]; then
      value=100
  fi
  folder_name="$result_sub_dir/$change_target=$value"
  environment_dir="$folder_name/environment"
  result_sub_sub_dir="$folder_name/result"
  mkdir -p "$environment_dir"
  mkdir -p "$result_sub_sub_dir"
  rsync -av -q --exclude='result' --exclude='cmake-build-debug' --exclude='cloth.dSYM' --exclude='.idea' --exclude='.git' --exclude='.cmake' "./" "$environment_dir"
  sed -i -e "s/$change_target=.*/$change_target=$value/" "$environment_dir/cloth_input.txt"
  sed -i -e "s/enable_group_routing=.*/enable_group_routing=false/" "$environment_dir/cloth_input.txt"
  cp "$environment_dir/cloth_input.txt" "$folder_name"
  cd "$environment_dir"
  cmake .
  make
  ./run-simulation.sh 39 "$result_sub_sub_dir/"
done
python3 gen_csv_summary.py result_sub_dir

change_target="average_payment_amount"
result_sub_dir="$result_dir/change_$change_target/enable_group_routing=true"
for ((i = 0; i <= 10; i++)); do
  cd "$root_dir"
  value=$((i*1000))
  if [ "$i" -eq 0 ]; then
      value=100
  fi
  folder_name="$result_sub_dir/$change_target=$value"
  environment_dir="$folder_name/environment"
  result_sub_sub_dir="$folder_name/result"
  mkdir -p "$environment_dir"
  mkdir -p "$result_sub_sub_dir"
  rsync -av -q --exclude='result' --exclude='cmake-build-debug' --exclude='cloth.dSYM' --exclude='.idea' --exclude='.git' --exclude='.cmake' "./" "$environment_dir"
  sed -i -e "s/$change_target=.*/$change_target=$value/" "$environment_dir/cloth_input.txt"
  sed -i -e "s/enable_group_routing=.*/enable_group_routing=true/" "$environment_dir/cloth_input.txt"
  cp "$environment_dir/cloth_input.txt" "$folder_name"
  cd "$environment_dir"
  cmake .
  make
  ./run-simulation.sh 39 "$result_sub_sub_dir/"
done
python3 gen_csv_summary.py result_sub_dir
