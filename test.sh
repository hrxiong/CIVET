mkdir INDEX
root_dir="path to dir"

dataset_dir=sample_data
minl=256
maxl=512
W=26
H=26
window_size=20

query_file=${root_dir}/data/${dataset_dir}/query
data_file=${root_dir}/data/${dataset_dir}/data
index_dir=${root_dir}/INDEX/${dataset_dir}/
log_dir=${root_dir}/log/${dataset_dir}/

rm -rf ${index_dir}
mkdir ${index_dir}
mkdir ${log_dir}
./USIndex --convert-data --dataset ${data_file} --queries ${query_file}
./USIndex --dataset ${data_file} --queries ${query_file} --index-path ${index_dir} \
        --window-size ${window_size} --W ${W} --H ${H} --minl ${minl} --maxl ${maxl} 
./USIndex --use-index --dataset ${data_file} --queries ${query_file} --index-path ${index_dir} \
        --W ${W} --H ${H} --minl ${minl} --maxl ${maxl} 