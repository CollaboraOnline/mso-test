#!/bin/bash

dir_to_search=$1 # should be "some_directory/"
max_pages=5 # this is as image dump has mismatched pages, and it seems to be that upto 5, the pages are identical

if [[ $# -lt 1 ]]; then
    echo "Pass the image dump directory"
    exit 1
fi

if [[ $# -eq 2 ]]; then
    max_pages=$2
fi


process_folder() {
    dir_to_search=$1
    dir_name=$(basename $dir_to_search)


    if [[ -d "input/$dir_name" ]]; then
        echo "PDF's inside of $dir_to_search have already been processed"
        return
    fi

    mkdir input/$dir_name output/$dir_name

    files=$(find $dir_to_search/ -not -name '*prev*' -name '*.pdf')

    for FILE in $files; do
        # convert to png first (loseless)
        output_name=""
        if [[ $FILE =~ "authoritative" ]]; then
            output_name="auth"
        else
            output_name="import"
        fi
        pdftoppm $FILE input/$dir_name/$output_name-page -png -r 150 -f 1 -l $max_pages
    done;

    # convert the png's to bmp's (may be multiple png's based on page count)
    for IMG in input/$dir_name/*; do
        img_name=$(basename "$IMG" .png)
        convert $IMG -colorspace Gray -define bmp::format=bmp4 -alpha on "input/$dir_name/$img_name.bmp"
        rm $IMG
    done;
}

result="$(find "$dir_to_search" -maxdepth 1 -type f -name '*.pdf')"
if [[ ${#result} > 0 ]] then # the directory provided contains pdf's not subfolders
    process_folder $dir_to_search
else
    for DIRECTORY in $dir_to_search*; do
        process_folder $DIRECTORY
    done
fi
