#!/bin/bash

# Usage: cd history && /path/to/mso/create-flamegraph.sh /path/to/mso [document_name.doc]
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
ext="$2"
doc_name="$3"

if [ -z "$mso_dir" ]; then
    echo "Pass the directory for the image regression test"
    exit 1
fi

if [ -z "$ext" ]; then
    echo "No file extension passed, for example 'pptx', 'ppt, 'docx', 'doc'"
    exit 1
elif [ "$ext" != "pptx" ] && [ "$ext" != "ppt" ] && [ "$ext" != "docx" ] && [ "$ext" != "doc" ]; then
    echo "File extension must be 'pptx', 'ppt', 'docx' or 'doc'"
    exit 1
fi

if [ -z "$doc_name" ]; then
    doc_name="*.doc"
    echo "No document name passed, defaulting to *.doc"
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
    find ../download/'"$ext"'/ -name '"$doc_name"' -execdir basename {} \; \
    | xargs -L1 -I{} python3 '"$mso_dir"'/diff-pdf-page-statistics.py \
        --base_file="{}" \
        --image_dump
'

# Process perf data
sudo perf script > out.perf
"$mso_dir/FlameGraph/stackcollapse-perf.pl" out.perf > out.folded
"$mso_dir/FlameGraph/flamegraph.pl" out.folded > ../converted/flamegraph.svg

# Files were created with sudo, change ownership to the user, so that they can be deleted later
sudo chown -R $USER:$USER ../converted
sudo chown -R $USER:$USER ../download
sudo chown -R $USER:$USER ../history