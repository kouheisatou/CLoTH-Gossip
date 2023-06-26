if [[ "$#" -ne 5 ]]; then
  echo "run_on_server.sh <ip> <user> <pass> <install_dir(full_path)> <threads_num>"
  exit 0
fi

sshpass -p "$3" rsync -r --delete -v -e ssh ./ "$2@$1:$4"
sshpass -p "$3" ssh "$2"@"$1" 'mkdir -p ~/log/LightningGossipSimulatorOutput/; cd '"$4"'; make build; ./run-simulation.sh 39 ~/log/LightningGossipSimulatorOutput/ '"$5"
