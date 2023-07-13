if [ $# -lt 3 ]
then
    echo "usage: $0 <seed> <output-directory> <threads_num>"
    exit
fi

GSL_RNG_SEED=$1  ./cloth $2 $3

python3 batch-means.py $2
