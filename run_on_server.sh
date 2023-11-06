if [[ "$#" -ne 5 ]]; then
  echo "./run_on_server.sh <ip> <user> <pass> <install_dir(full_path)> <output_dir>"
  exit 0
fi

sshpass -p "$3" rsync -r --delete --checksum -v -e ssh ./ "$2@$1:$4"
sshpass -p "$3" ssh "$2@$1" 'mkdir -p '"$5"'; cd '"$4"'; cmake .; make; GSL_RNG_SEED=39 ./CLoTH_Gossip '"$5"'/; python3 batch-means.py '"$5"'/'
