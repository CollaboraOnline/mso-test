#!/bin/bash

# Usage: cd history && /path/to/mso/create-flamegraph.sh /path/to/mso
# The file structure required:
# |--- history/
# |    |--- ext/
# |--- download/
# |    |--- ext/
# |--- converted/
# |    |--- ext/
# To run the script, you need to be inside the history/ directory
# To view the flamegraph, open history/flamegraph.svg in a web browser

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

make -C "$mso_dir" clean && make -C "$mso_dir" debug

sudo perf record -F 99 -g -- sh -c '
    find ../download/doc/ -name "*.doc" -execdir basename {} \; \
    | xargs -L1 -I{} python3 '"$mso_dir"'/diff-pdf-page-statistics.py \
        --base_file="{}" \
        --image_dump
'

# Process perf data
sudo perf script > out.perf
"$mso_dir/FlameGraph/stackcollapse-perf.pl" out.perf > out.folded
"$mso_dir/FlameGraph/flamegraph.pl" out.folded > flamegraph.svg
