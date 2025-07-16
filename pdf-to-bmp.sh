#!/bin/bash

dir_to_search=$1 # should be "some_directory/"
max_pages=5 # this is as image dump has mismatched pages, and it seems to be that upto 5, the pages are identical
scale_factor=4

# This script processes PDF files in a specified directory, converting them to BMP images.
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <image_dump_directory> [max_pages]"
    exit 1
fi

if [[ $# -eq 2 ]]; then
    max_pages=$2
fi

process_folder() {
    dir_to_search=$1
    dir_name=$(basename $dir_to_search)

    # Check if the directory has already been processed
    if [[ -d "input/$dir_name" ]]; then
        echo "PDF's inside of $dir_to_search have already been processed"
        return
    fi

    # Create the input directory for the processed files
    mkdir -p input/$dir_name

    # Find all PDF files in the directory, excluding those with 'prev' in their name
    files=$(find $dir_to_search/ -not -name '*prev*' -name '*.pdf')

    # Convert each PDF file to PNG images first
    for FILE in $files; do
        output_name=""
        # Authoritative files are the MSO files, and import and the CO files.
        if [[ $FILE =~ "authoritative" ]]; then
            output_name="auth"
        else
            output_name="import"
        fi
        convert $FILE \
        -density 600 \
        -filter box \
        -resize 75% \
        -colorspace Gray \
        -define bmp::format=bmp4 \
        -background white \
        -alpha remove \
        -alpha on \
        "input/$dir_name/$output_name-page.bmp"
        # First remove the alpha chanel, turn it off so that background is not transparent, and then turn it on again to have the background white.
    done;
    echo "Proccessed $dir_name"
}

# Check if the provided directory contains PDF files directly or subfolders
result="$(find "$dir_to_search" -maxdepth 1 -type f -name '*.pdf')"
if [[ ${#result} > 0 ]] then # if there are PDF files directly in the directory
    process_folder $dir_to_search
else
    for DIRECTORY in $dir_to_search*; do # process each subdirectory
        process_folder $DIRECTORY
    done
fi
