#!/bin/bash

make # make sure pixelbasher executable exists

# need to loop over individual subdirectory
for DIRECTORY in input/*; do
  dir_name=$(basename $DIRECTORY)
  # loop over in each subdirectory just the auth pages and get their reciprical import page

  declare -A auth_map
  declare -A import_map

  while IFS= read -r file; do
    page=$(echo "$file" | grep -oP 'page-\K[0-9]+' | sed 's/^0*//') # gets the page number from the file
    auth_map["$page"]="$file"
  done < <(find "$DIRECTORY" -name "*auth*.bmp") # passes the find output as input to the while loop

  while IFS= read -r file; do
    page=$(echo "$file" | grep -oP 'page-\K[0-9]+' | sed 's/^0*//')
    import_map["$page"]="$file"
  done < <(find "$DIRECTORY" -name "*import*.bmp")

  for page in $(printf "%s\n" "${!auth_map[@]}" | sort -n); do
    auth_file="${auth_map[$page]}"
    import_file="${import_map[$page]}"

    if [[ -n "$import_file" ]]; then
      echo "$auth_file ---- $import_file"
      # add the script to compare the files
      enable_minor_differences=$1
      echo $page
      output_file="output/$dir_name/diff-$page.bmp"
      ./pixelbasher $auth_file $import_file $output_file $enable_minor_differences

    else
      echo "Missing page for $page"
    fi;
  done;

  unset auth_map
  unset import_map
done;
