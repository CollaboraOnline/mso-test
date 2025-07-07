filepath=$1
filename=$(basename $filepath)

mkdir input/$filename output/$filename
pdftoppm $filepath input/$filename/output -png -r 300

for img in input/$filename/*.png; do
    basename=$(basename "$img" .png)
    convert "$img" -define bmp:format=bmp4 -alpha on "input/$filename/${basename}.bmp"
    rm $img
done
