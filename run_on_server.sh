if [[ "$#" -ne 4 ]]; then
  echo "./run_on_server.sh <ip> <user> <pass> <output_dir(full_path)>"
  exit 0
fi

root_dir="$4/$(date "+%Y%m%d%H%M%S")/"
output_dir="$root_dir/result/"
install_dir="$root_dir/environment/"

sshpass -p "$3" ssh "$2@$1" 'mkdir -p '"$output_dir"'; mkdir -p '"$install_dir"''
sshpass -p "$3" rsync -r --delete --checksum -v -e ssh ./ "$2@$1:$install_dir"
sshpass -p "$3" ssh "$2@$1" 'mkdir -p '"$output_dir"'; cd '"$install_dir"'; cp cloth_input.txt '"$root_dir"'; cmake .; make; GSL_RNG_SEED=39 ./CLoTH_Gossip '"$output_dir"'/; python3 batch-means.py '"$output_dir"'/'
