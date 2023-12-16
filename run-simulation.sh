if [ $# -lt 3 ]
then
    echo "usage: $0 <seed> <output-directory> [setting_key=setting_value]"
    exit
fi

environment_dir="$2/environment"
result_dir="$2/result"

mkdir -p "$environment_dir"
mkdir -p "$result_dir"

rsync -av -q --exclude='result' --exclude='cmake-build-debug' --exclude='cloth.dSYM' --exclude='.idea' --exclude='.git' --exclude='.cmake' "./" "$environment_dir"

for arg in "${@:3}"; do
    key="${arg%=*}"
    value="${arg#*=}"
    echo "setting_key=: $key, setting_value=: $value"
    sed -i -e "s/$key=.*/$key=$value/" "$environment_dir/cloth_input.txt"
done

cp "$environment_dir/cloth_input.txt" "$2"
cd "$environment_dir"
cmake .
make

GSL_RNG_SEED=$1  ./CLoTH_Gossip "$result_dir/"

python3 batch-means.py "$result_dir/"
