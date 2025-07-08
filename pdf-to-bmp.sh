dir_to_search=$1 # should be "some_directory/"

for DIRECTORY in $dir_to_search*; do
    dir_name=$(basename $DIRECTORY)
    if ! [[ -d "input/$dir_name" ]]; then
        mkdir input/$dir_name output/$dir_name

        files=$(find $DIRECTORY/ -not -name '*prev*' -name '*.pdf')
        for FILE in $files; do
            # convert to png first (loseless)
            output_name=""
            if [[ $FILE =~ "authoritative" ]]; then
                output_name="auth"
            else
                output_name="import"
            fi
            pdftoppm $FILE input/$dir_name/$output_name-page -png -r 150 
        done;

        # convert the png's to bmp's (may be multiple png's based on page count)
        for IMG in input/$dir_name/*; do
            img_name=$(basename "$IMG" .png)
            convert $IMG -define bmp::format=bmp4 -alpha on "input/$dir_name/$img_name.bmp"
            rm $IMG
        done;
    fi;
done;
    
