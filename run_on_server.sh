if [[ "$#" -ne 5 ]]; then
  echo "./run_on_server.sh <ip> <user> <password> <install_dir(remote_full_path)> <command>"
  exit 0
fi

sshpass -p "$3" ssh "$2@$1" 'mkdir -p '"$4"
sshpass -p "$3" rsync -q --exclude='result' --exclude='cmake-build-debug' --exclude='cloth.dSYM' --exclude='.idea' --exclude='.git' --exclude='.cmake' -r --delete --checksum -v -e ssh ./ "$2@$1:$4"
sshpass -p "$3" ssh "$2@$1" 'cd '"$4;$5"
