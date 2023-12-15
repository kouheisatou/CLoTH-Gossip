if [ $# -lt 2 ]
then
    echo "usage: $0 <seed> <output-directory>"
    exit
fi

GSL_RNG_SEED=$1  ./CLoTH_Gossip $2

python3 batch-means.py $2
