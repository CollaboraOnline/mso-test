#!/bin/bash

make # to make sure pixelbasher executable exists and is up to date

if [[ $# -lt 3 ]]; then
  echo "Usage: $0  <pdf_directories>  <input_directory> <output_directory> [max_pages] [enable_minor_differences]"
  echo "<pdf_directories>: Directory containing PDF files to process."
  echo "<input_directory>: Directory to save the converted PDF files as BMP's."
  echo "<output_directory>: Directory to save output BMP files."
  echo "[max_pages]: Maximum number of pages to process from each PDF file. Default is 5"
  echo "[enable_minor_differences]: Optional flag to enable minor differences in comparison. Default is empty."
  exit 1
fi

pdf_directory=$1
input_directory=$2
output_directory=$3

if [[ $# -ge 4 ]]; then
  echo "Max pages set to $4"
  max_pages=$4
else
  max_pages=5 # default value
fi

if [[ $# -eq 5 ]]; then
  enable_minor_differences=$5
else
  enable_minor_differences="" # default value
fi

# Converts the pdf's to bmp's, this will create input/some_directory/ containing the bmp's
./pdf-to-bmp.sh "$pdf_directory" $max_pages

# Loop over each subdirectory in the input directory
for DIRECTORY in $input_directory/*; do
  dir_name=$(basename $DIRECTORY)
  mkdir -p "$output_directory/$dir_name"

  # Create associative arrays to hold the file paths for each page
  declare -A auth_map
  declare -A import_map

  # Read the auth BMP files and map them to their respective pages
  while IFS= read -r file; do
    page=$(echo "$file" | sed -n 's/.*page-\([0-9]\+\).*/\1/p')
    if [[ -z "$page" ]]; then
        page=1 # Default to page 1 if not specified
    fi
    auth_map["$page"]="$file"
  done < <(find "$DIRECTORY" -name "*auth*.bmp") # passes the find output as input to the while loop

  # Read the import BMP files and map them to their respective pages
  while IFS= read -r file; do
    page=$(echo "$file" | sed -n 's/.*page-\([0-9]\+\).*/\1/p')
    if [[ -z "$page" ]]; then
      page=1
    fi
    import_map["$page"]="$file"
  done < <(find "$DIRECTORY" -name "*import*.bmp")

  echo -e "\nDirectory: $dir_name"

  # Loop through the pages in the auth_map and compare with reciprocal import_map pages
  for page in $(printf "%s\n" "${!auth_map[@]}" | sort -n); do
    auth_file="${auth_map[$page]}"
    import_file="${import_map[$page]}"

    # Check if the corresponding import file exists
    if [[ -n "$import_file" ]]; then
      echo "Diffing $(basename $auth_file) --> $(basename $import_file)"

      output_file="$output_directory/$dir_name/diff-$page.bmp"

      # Run the pixelbasher command to compare the images and generate the diff
      ./pixelbasher $auth_file $import_file $output_file $enable_minor_differences
      echo -e ""
    else
      echo "Missing page for $page"
    fi;
  done;

  echo -e "Finished Diffing: $dir_name\n"

  # Clean up the associative arrays for the next directory
  unset auth_map
  unset import_map
done;
