#!/bin/bash

mso_dir="$1"

if [ -z "$mso_dir" ]; then
    echo "Pass the directory for the image regression test"
    exit 1
fi

if [ ! -d "$mso_dir/FlameGraph" ]; then
    git clone https://github.com/brendangregg/FlameGraph "$mso_dir/FlameGraph"
fi

# Clean up old perf data
rm -f perf.data
rm -f out.perf
rm -f out.folded
rm -f flamegraph.svg

sudo perf record -F 99 -g -- sh -c '
    find ../download/doc/ -name "*.doc" -execdir basename {} \; \
    | xargs -L1 -I{} python3 '"$mso_dir"'/diff-pdf-page-statistics.py \
        --no_save_overlay \
        --base_file="{}" \
        --image_dump
'

# Process perf data
sudo perf script > out.perf
"$mso_dir/FlameGraph/stackcollapse-perf.pl" out.perf > out.folded
"$mso_dir/FlameGraph/flamegraph.pl" out.folded > flamegraph.svg
