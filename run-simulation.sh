if [ $# -lt 3 ]
then
    echo "usage: $0 <seed> <output-directory> <dijkstra_cache_filename> [setting_key=setting_value]"
    exit
fi

seed="$1"
environment_dir="$2/environment"
result_dir="$2"
dijkstra_cache_filename="$3"

mkdir -p "$environment_dir"
mkdir -p "$result_dir"
mkdir -p "$result_dir/log"

rsync -av -q --exclude='result' --exclude='cmake-build-debug' --exclude='cloth.dSYM' --exclude='.idea' --exclude='.git' --exclude='.cmake' "." "$environment_dir"

for arg in "${@:4}"; do
    key="${arg%=*}"
    value="${arg#*=}"
    sed -i -e "s/$key=.*/$key=$value/" "$environment_dir/cloth_input.txt"
done

cp "$environment_dir/cloth_input.txt" "$2"
cd "$environment_dir"
cmake . &> "$result_dir/log/cmake.log"
make &> "$result_dir/log/make.log"

GSL_RNG_SEED="$seed"  ./CLoTH_Gossip "$result_dir/" "$dijkstra_cache_filename" &> "$result_dir/log/cloth.log"
cat "$result_dir/output.log"
echo "seed=$seed" >> "$result_dir/cloth_input.txt"

rm -Rf "$environment_dir"
