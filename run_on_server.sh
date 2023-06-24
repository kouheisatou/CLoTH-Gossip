if [[ "$#" -ne 3 ]]; then
  echo "run_on_server.sh <ip> <user> <pass>"
  exit 0
fi

sshpass -p "$3" ssh "$2"@"$1" 'rm -rf /home/kohei/git/LightningGossipSimulator/'
sshpass -p "$3" rsync -r -v -e ssh /Users/kohei/CLionProjects/LightningGossipSimulator/ "$2"@"$1":/home/kohei/git/LightningGossipSimulator/
sshpass -p "$3" ssh "$2"@"$1" 'cd /home/kohei/git/LightningGossipSimulator && make build && ./run-simulation.sh 39 ~/log/LightningGossipSimulatorOutput/'
