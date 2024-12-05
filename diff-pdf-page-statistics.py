#!/usr/bin/python3

#NOTE: you probably need to increase the cache allowed: /etc/ImageMagick-*/policy.xml
#      <policy domain="resource" name="disk" value="16GiB"/>

# find docx/ -name "*.docx" -execdir basename {} \; | xargs -L1 -I{} python3 ../diff-pdf-page-statistics.py --no_save_overlay  --base_file="{}"
#       - delete any CSV files. Delete the import/export folders in converted
#       - rename converted DOC/PPT/XLS pdfs from .docx_mso.pdf to .doc_mso.pdf, etc.

# WARNING: this is not a tool for administrative statistics; there are just too many false positives for counts of red dots to be meaningful.
# It is only meant to be useful for QA in identifying regressions.

# Given:
#   - a document (in the "download/file_type/" folder)
#   - an "authoritative" _mso.PDF of how the document should look (also in the "download/file_type/" folder)
#   - a Collabora .PDF of the document (in the "converted/original_file_type" folder)
#   - a Microsoft .original_file_type_mso.PDF of the Collabora-round-tripped file (in the "converted/original_file_type" folder)
#   - history folder: a copy of the converted folder from a previous Collabora version (identifying the commit range to search for regressions)
#
# Running the tool:
#   - cd into the history folder (or specify the folder with --history_dir=)
#   - ../diff-pdf-page-statistics.py --base_file=document_name.ext
#   - look at the import/export overlay PNG results in the converted folder
#
# False positives:
#   - automatically updating fields: dates, =rand(), slide date/time ...
#   - different font subsitutions

import argparse
import os
import wand # pip install wand && OS_INSTALLER install imagemagick
from wand.image import Image
from wand.exceptions import PolicyError
from wand.exceptions import CacheError
import time

MAX_PAGES = 10 # limit PDF comparison to the first ten pages

def printdebug(debug, *args, **kwargs):
    """
    A conditional debug print function.
    Prints messages only if the DEBUG variable is True.

    Parameters:
        *args: Positional arguments to pass to print().
        **kwargs: Keyword arguments to pass to print().
    """
    if debug:
        print(*args, **kwargs)


def main():
    parser = argparse.ArgumentParser(description="Look for import and export regressions.")
    parser.add_argument("--base_file", default="lorem ipsum.docx")
    parser.add_argument("--history_dir", default=".")
    parser.add_argument("--no_save_overlay", action="store_true") # default is false
    parser.add_argument("--debug", action="store_true") # default is false
    args = parser.parse_args()

    DEBUG = args.debug

    if (
        args.base_file == 'forum-mso-de-108371.xlsx' # =rand()
    ):
        print("SKIPPING FILE", args.base_file, ": determined to be unusable for testing...")
        exit(0)

    print ("Processing: ", args.base_file)
    base_dir = "./"
    if args.history_dir == "." and not os.path.isdir('download') and os.path.isdir(os.path.join("..", 'download')):
        base_dir = "../"
    file_ext = os.path.splitext(args.base_file)

    MS_ORIG = os.path.join(base_dir, "download", file_ext[1][1:], args.base_file + "_mso.pdf")
    if not os.path.isfile(MS_ORIG):
        print ("original PDF file [" + MS_ORIG +"] not found")
        exit (1)
    LO_ORIG = os.path.join(base_dir, "converted", file_ext[1][1:], file_ext[0] + ".pdf")
    if not os.path.isfile(LO_ORIG):
        print ("Collabora PDF file [" + LO_ORIG +"] not found")
        exit (1)
    MS_CONV = os.path.join(base_dir, "converted", file_ext[1][1:], args.base_file + "_mso.pdf")
    if not os.path.isfile(MS_CONV):
        print ("MS converted PDF file [" + MS_CONV +"] not found")
        exit (1)

    # This tool is not very useful without having a previous run to compare against.
    # However, it still can create overlay images, which might be of some value.
    LO_PREV = os.path.join(args.history_dir, file_ext[1][1:], file_ext[0] + ".pdf")
    IS_FILE_LO_PREV = os.path.isfile(LO_PREV)
    MS_PREV = os.path.join(args.history_dir, file_ext[1][1:], args.base_file + "_mso.pdf")
    IS_FILE_MS_PREV = os.path.isfile(MS_PREV)

    IMPORT_DIR = os.path.join(base_dir, "converted", "import", file_ext[1][1:])
    if not os.path.isdir(IMPORT_DIR):
        os.makedirs(IMPORT_DIR)

    EXPORT_DIR = os.path.join(base_dir, "converted", "export", file_ext[1][1:])
    if not os.path.isdir(EXPORT_DIR):
        os.makedirs(EXPORT_DIR)

    IMPORT_COMPARE_DIR = os.path.join(base_dir, "converted", "import-compare", file_ext[1][1:])
    if not os.path.isdir(IMPORT_COMPARE_DIR):
        os.makedirs(IMPORT_COMPARE_DIR)

    EXPORT_COMPARE_DIR = os.path.join(base_dir, "converted", "export-compare", file_ext[1][1:])
    if not os.path.isdir(EXPORT_COMPARE_DIR):
        os.makedirs(EXPORT_COMPARE_DIR)

    try:
        # The "correct" PDF: created by MS Word of the original file
        MS_ORIG_PDF = Image(filename=MS_ORIG, resolution=150)

        # A PDF of how it is displayed in Writer - to be compared to MS_ORIG
        LO_ORIG_PDF = Image(filename=LO_ORIG, resolution=150)

        # A PDF of how MS Word displays Writer's round-tripped file - to be compared to MS_ORIG
        MS_CONV_PDF = Image(filename=MS_CONV, resolution=150)

        # A historical version of how it was displayed in Writer
        LO_PREV_PDF = Image()
        LO_PREV_PAGES = MAX_PAGES
        if IS_FILE_LO_PREV:
            LO_PREV_PDF = Image(filename=LO_PREV, resolution=150)
            LO_PREV_PAGES = len(LO_PREV_PDF.sequence)

        # A historical version of how the round-tripped file was displayed in Word
        MS_PREV_PDF = Image()
        MS_PREV_PAGES = MAX_PAGES
        if IS_FILE_MS_PREV:
            MS_PREV_PDF = Image(filename=MS_PREV, resolution=150)
            MS_PREV_PAGES=len(MS_PREV_PDF.sequence)

    except PolicyError:
        print("Warning: Operation not allowed due to security policy restrictions for PDF files.")
        print("Please modify the '/etc/ImageMagick-6/policy.xml' file to allow PDF processing.")
        print("<policy domain=\"coder\" rights=\"read\" pattern=\"PDF\" />")
        exit(1)
    except CacheError as e:
        print("Exception message: ", str(e))
        print("You probably need to increase the cache allowed in /etc/ImageMagick-6/policy.xml")
        print("<policy domain=\"resource\" name=\"disk\" value=\"16GiB\"/>")
        exit(1)

    pages = min(MAX_PAGES, len(MS_ORIG_PDF.sequence), len(LO_ORIG_PDF.sequence), len(MS_CONV_PDF.sequence), LO_PREV_PAGES, MS_PREV_PAGES)
    printdebug(DEBUG, "DEBUG ", args.base_file, " pages[", pages, "] ", MAX_PAGES, len(MS_ORIG_PDF.sequence), len(LO_ORIG_PDF.sequence), len(MS_CONV_PDF.sequence), len(LO_PREV_PDF.sequence), len(MS_PREV_PDF.sequence))

    MS_ORIG_SIZE = []    # total number of pixels on the page
    MS_ORIG_CONTENT = [] # the number of non-background pixels
    LO_ORIG_SIZE = []
    LO_ORIG_CONTENT = []
    MS_CONV_SIZE = []
    MS_CONV_CONTENT = []
    LO_PREV_SIZE = []
    LO_PREV_CONTENT = []
    MS_PREV_SIZE = []
    MS_PREV_CONTENT = []

    RED_COLOR = []      # the exact color of red: used as the histogram key
    IMPORT_RED = []     # the number of red pixels on the page
    EXPORT_RED = []
    PREV_IMPORT_RED = []
    PREV_EXPORT_RED = []

    for pgnum in range(0, pages):
        with MS_ORIG_PDF.sequence[pgnum] as page: # need this 'with' clause so that MS_ORIG_PDF is actually updated with the following changes
            MS_ORIG_SIZE.append(page.height * page.width)
            page.alpha_channel = 'remove'         # so that 'red' will be painted as 'red' and not some transparent-ized shade of red
            page.quantize(2)                      # reduced to two colors (assume background and non-background)
            page.opaque_paint('black', 'red', fuzz=MS_ORIG_PDF.quantum_range * 0.95)
            HIST_COLORS = list(page.histogram.keys())
            HIST_PIXELS = list(page.histogram.values())
            MS_ORIG_CONTENT.append(min(HIST_PIXELS))
            printdebug(DEBUG, "DEBUG MS_WORD_ORIG ", args.base_file, " colorspace[" + page.colorspace +"] size[", MS_ORIG_SIZE[pgnum], "] content[", MS_ORIG_CONTENT[pgnum], "] PIXELS ", HIST_PIXELS, " COLORS ", HIST_COLORS)

            # assuming that the background is at least 50%. Might be a bad assumption - especially with presentations.
            # NOTE: this logic might not be necessary any more. I used it before removing the transparency - so perhaps I can always trust that 'red' will be 'red' now
            if MS_ORIG_CONTENT[pgnum] == HIST_PIXELS[0]:
                RED_COLOR.append(HIST_COLORS[0])
            else:
                RED_COLOR.append(HIST_COLORS[1])
            printdebug(DEBUG, "DEBUG: RED_COLOR  ", RED_COLOR[pgnum].normalized_string, " pixels[",MS_ORIG_CONTENT[pgnum] == HIST_PIXELS[0],"][", MS_ORIG_CONTENT[pgnum],"][",HIST_PIXELS[0],"]")

    # Composed image: overlay red MS_ORIG with LO_ORIG
    IMPORT_IMAGE = MS_ORIG_PDF.clone()
    # Composed image: overlay red MS_ORIG with MS_CONV
    EXPORT_IMAGE = MS_ORIG_PDF.clone()

    # Composed image: overlay red MS_ORIG with LO_PREV
    PREV_IMPORT_IMAGE = MS_ORIG_PDF.clone()
    # Composed image: overlay red MS_ORIG with MS_PREV
    PREV_EXPORT_IMAGE = MS_ORIG_PDF.clone()

    # Composed image: overlay red LO_ORIG with LO_PREV
    # This is the visual key to the whole tool. The overlay should be identical except for import fixes or regressions
    IMPORT_COMPARE_IMAGE = LO_ORIG_PDF.clone()
    # Composed image: overlay red MS_CONV with MS_PREV
    # This is the visual key to the whole tool. The overlay should be identical except for export fixes or regressions
    EXPORT_COMPARE_IMAGE = MS_CONV_PDF.clone()

    for pgnum in range(0, pages):
        tmp = LO_ORIG_PDF.clone()  # don't make changes to these PDF pages - just get statistics...
        with tmp.sequence[pgnum] as page:
            page.quantize(2)
            LO_ORIG_SIZE.append(page.height * page.width)
            LO_ORIG_CONTENT.append(min(list(page.histogram.values()))) # assuming that the background is more than 50%
            printdebug(DEBUG, "DEBUG LO_ORIG[", pgnum, "] size[", LO_ORIG_SIZE[pgnum], "] content[", LO_ORIG_CONTENT[pgnum], "] percent[", (LO_ORIG_CONTENT[pgnum] / LO_ORIG_SIZE[pgnum]), "] colorspace[", page.colorspace, "] background[", page.background_color, "] ", list(page.histogram.values()), list(page.histogram.keys()))

        tmp = MS_CONV_PDF.clone()
        with tmp.sequence[pgnum] as page:
            page.quantize(2)
            MS_CONV_SIZE.append(page.height * page.width)
            MS_CONV_CONTENT.append(min(list(page.histogram.values())))
            printdebug(DEBUG, "DEBUG MS_CONV[", pgnum, "] size[", MS_CONV_SIZE[pgnum], "] content[", MS_CONV_CONTENT[pgnum], "] ", list(page.histogram.values()), list(page.histogram.keys()), " percent[", MS_CONV_CONTENT[pgnum] / MS_CONV_SIZE[pgnum], "]")

        if IS_FILE_LO_PREV:
            tmp = LO_PREV_PDF.clone()
            with tmp.sequence[pgnum] as page:
                page.quantize(2)
                LO_PREV_SIZE.append(page.height * page.width)
                LO_PREV_CONTENT.append(min(list(page.histogram.values())))
                printdebug(DEBUG, "DEBUG LO_PREV[", pgnum, "] size[", LO_PREV_SIZE[pgnum], "] content[", LO_PREV_CONTENT[pgnum], "] ", list(page.histogram.values()), list(page.histogram.keys()), " percent[", LO_PREV_CONTENT[pgnum] / LO_PREV_SIZE[pgnum], "]")

        if IS_FILE_MS_PREV:
            tmp = MS_PREV_PDF.clone()
            with tmp.sequence[pgnum] as page:
                page.quantize(2)
                MS_PREV_SIZE.append(page.height * page.width)
                MS_PREV_CONTENT.append(min(list(page.histogram.values())))
                printdebug(DEBUG, "DEBUG MS_PREV[", pgnum, "] size[", MS_PREV_SIZE[pgnum], "] content[", MS_PREV_CONTENT[pgnum], "] ", list(page.histogram.values()), list(page.histogram.keys()), " percent[", MS_PREV_CONTENT[pgnum] / MS_PREV_SIZE[pgnum], "]")


        with IMPORT_IMAGE.sequence[pgnum] as page:
            LO_ORIG_PDF.transparent_color(LO_ORIG_PDF.background_color, 0, fuzz=LO_ORIG_PDF.quantum_range * 0.05)
            LO_ORIG_PDF.sequence[pgnum].transform_colorspace('gray')
            page.composite(LO_ORIG_PDF.sequence[pgnum])  # overlay (red) MS_ORIG with LO_ORIG
            page.merge_layers('flatten')
            page.alpha_channel = 'remove'
        IMPORT_RED.append(0)
        try:
            IMPORT_RED[pgnum] = IMPORT_IMAGE.sequence[pgnum].histogram[RED_COLOR[pgnum]]
        except:
            printdebug(DEBUG, "IMPORT EXCEPTION: could not get red color from page ", pgnum)#, list(IMPORT_IMAGE.sequence[pgnum].histogram.keys()))

        with EXPORT_IMAGE.sequence[pgnum] as page:
            MS_CONV_PDF.transparent_color(MS_CONV_PDF.background_color, 0, fuzz=MS_CONV_PDF.quantum_range * 0.05)
            MS_CONV_PDF.sequence[pgnum].transform_colorspace('gray')
            page.composite(MS_CONV_PDF.sequence[pgnum]) # overlay (red) MS_ORIG with MS_CONV
            page.merge_layers('flatten')
            page.alpha_channel = 'remove'
        EXPORT_RED.append(0)
        try:
            EXPORT_RED[pgnum] = EXPORT_IMAGE.sequence[pgnum].histogram[RED_COLOR[pgnum]]
        except:
            printdebug(DEBUG, "EXPORT EXCEPTION: could not get red color from page ", pgnum)# , list(EXPORT_IMAGE.sequence[pgnum].histogram.keys()))

        PREV_IMPORT_RED.append(0)
        if IS_FILE_LO_PREV:
            with PREV_IMPORT_IMAGE.sequence[pgnum] as page:
                LO_PREV_PDF.transparent_color(LO_PREV_PDF.background_color, 0, fuzz=LO_PREV_PDF.quantum_range * 0.05)
                LO_PREV_PDF.sequence[pgnum].transform_colorspace('gray')
                page.composite(LO_PREV_PDF.sequence[pgnum]) # overlay (red) MS_ORIG with LO_PREV
                page.merge_layers('flatten')
                page.alpha_channel = 'remove'
            try:
                PREV_IMPORT_RED[pgnum] = PREV_IMPORT_IMAGE.sequence[pgnum].histogram[RED_COLOR[pgnum]]
            except:
                printdebug(DEBUG, "PREV_IMPORT EXCEPTION: could not get red color from page ", pgnum)#, list(PREV_IMPORT_IMAGE.sequence[pgnum].histogram.keys()))

            with IMPORT_COMPARE_IMAGE.sequence[pgnum] as page:
                page.alpha_channel = 'remove'
                page.quantize(2)
                page.opaque_paint('black', 'red', fuzz=LO_ORIG_PDF.quantum_range * 0.95)
                page.composite(LO_PREV_PDF.sequence[pgnum]) # overlay (red) LO_ORIG with LO_PREV
                page.merge_layers('flatten')

        PREV_EXPORT_RED.append(0)
        if IS_FILE_MS_PREV:
            with PREV_EXPORT_IMAGE.sequence[pgnum] as page:
                MS_PREV_PDF.transparent_color(MS_PREV_PDF.background_color, 0, fuzz=MS_PREV_PDF.quantum_range * 0.05)
                MS_PREV_PDF.sequence[pgnum].transform_colorspace('gray')
                page.composite(MS_PREV_PDF.sequence[pgnum]) # overlay (red) MS_ORIG with MS_PREV
                page.merge_layers('flatten')
                page.alpha_channel = 'remove'
            try:
                PREV_EXPORT_RED[pgnum] = PREV_EXPORT_IMAGE.sequence[pgnum].histogram[RED_COLOR[pgnum]]
            except:
                printdebug(DEBUG, "PREV_EXPORT EXCEPTION: could not get red color from page ", pgnum)#, list(PREV_EXPORT_IMAGE.sequence[pgnum].histogram.keys()))

            with EXPORT_COMPARE_IMAGE.sequence[pgnum] as page:
                page.alpha_channel = 'remove'
                page.quantize(2)
                page.opaque_paint('black', 'red', fuzz=MS_CONV_PDF.quantum_range * 0.95)
                page.composite(MS_PREV_PDF.sequence[pgnum]) # overlay (red) MS_CONV with MS_PREV
                page.merge_layers('flatten')


    FORCE_SAVE = False
    if (
        (IS_FILE_LO_PREV and IMPORT_RED > PREV_IMPORT_RED)
        or (IS_FILE_MS_PREV and EXPORT_RED > PREV_EXPORT_RED)
    ):
        FORCE_SAVE = True
    if args.no_save_overlay == False or FORCE_SAVE:
        printdebug(DEBUG, "DEBUG saving " + args.base_file +" IMPORT["+ str(IMPORT_RED)+ "] PREV["+str(PREV_IMPORT_RED) +"] EXPORT["+str(EXPORT_RED)+"] PREV["+str(PREV_EXPORT_RED)+"]")
        for pageToSave in range(0, pages):
            with Image(IMPORT_IMAGE.sequence[pageToSave]) as img_to_save:
                file_name=os.path.join(IMPORT_DIR, args.base_file + f"_import-{pageToSave}.png")
                img_to_save.save(filename=file_name)
            with Image(EXPORT_IMAGE.sequence[pageToSave]) as img_to_save:
                file_name=os.path.join(EXPORT_DIR, args.base_file + f"_export-{pageToSave}.png")
                img_to_save.save(filename=file_name)

            if IS_FILE_LO_PREV:
                with Image(PREV_IMPORT_IMAGE.sequence[pageToSave]) as img_to_save:
                    file_name=os.path.join(IMPORT_DIR, args.base_file + f"_prev-import-{pageToSave}.png")
                    img_to_save.save(filename=file_name)
                with Image(IMPORT_COMPARE_IMAGE.sequence[pageToSave]) as img_to_save:
                    file_name=os.path.join(IMPORT_COMPARE_DIR, args.base_file + f"_import-compare-{pageToSave}.png")
                    img_to_save.save(filename=file_name)
            if IS_FILE_MS_PREV:
                with Image(PREV_EXPORT_IMAGE.sequence[pageToSave]) as img_to_save:
                    file_name=os.path.join(EXPORT_DIR, args.base_file + f"_prev-export-{pageToSave}.png")
                    img_to_save.save(filename=file_name)
                with Image(EXPORT_COMPARE_IMAGE.sequence[pageToSave]) as img_to_save:
                    file_name=os.path.join(EXPORT_COMPARE_DIR, args.base_file + f"_export-compare-{pageToSave}.png")
                    img_to_save.save(filename=file_name)

    # allow the script to run in parallel - wait for lock on report to be released.
    # if lock file exists, wait for one second and try again
    # else
    #     create lock file and put the file name in it
    #     wait for a bit and then read to verify the lock is mine
    while True:
        LOCK_FILE="diff-pdf-" + file_ext[1][1:] + "-statistics.lock"
        if os.path.isfile(LOCK_FILE):
            printdebug(DEBUG, "DEBUG: waiting for file to unlock")
        else:
            with open(LOCK_FILE, 'w') as f:
                f.write(args.base_file)
            time.sleep(0.1) # one tenth of a second
            with open(LOCK_FILE, 'r') as f:
                LOCK = f.read()
                printdebug(DEBUG, "DEBUG LOCK[", LOCK, "]")
                if LOCK == args.base_file:
                    with open('diff-pdf-' + file_ext[1][1:] + '-import-statistics.csv', 'a') as f:
                        for pgnum in range(0, pages):
                            OUT_STRING = [ args.base_file ]
                            OUT_STRING.append(str(pgnum))
                            OUT_STRING.append(str(MS_ORIG_SIZE[pgnum]))
                            OUT_STRING.append(str(MS_ORIG_CONTENT[pgnum]))
                            OUT_STRING.append(str(MS_ORIG_CONTENT[pgnum] / MS_ORIG_SIZE[pgnum]))
                            OUT_STRING.append(str(LO_ORIG_SIZE[pgnum]))
                            OUT_STRING.append(str(LO_ORIG_CONTENT[pgnum]))
                            OUT_STRING.append(str(LO_ORIG_CONTENT[pgnum] / LO_ORIG_SIZE[pgnum]))
                            OUT_STRING.append(str(IMPORT_RED[pgnum]))
                            OUT_STRING.append(str(IMPORT_RED[pgnum] / LO_ORIG_CONTENT[pgnum]))
                            if IS_FILE_LO_PREV:
                                OUT_STRING.append(str(LO_PREV_SIZE[pgnum]))
                                OUT_STRING.append(str(LO_PREV_CONTENT[pgnum]))
                                OUT_STRING.append(str(LO_PREV_CONTENT[pgnum] / LO_PREV_SIZE[pgnum]))
                                OUT_STRING.append(str(PREV_IMPORT_RED[pgnum]))
                                OUT_STRING.append(str(PREV_IMPORT_RED[pgnum] / LO_PREV_CONTENT[pgnum]))
                            f.write(','.join(OUT_STRING) + '\n')

                    with open('diff-pdf-' + file_ext[1][1:] + '-export-statistics.csv', 'a') as f:
                        for pgnum in range(0, pages):
                            OUT_STRING = [ args.base_file ]
                            OUT_STRING.append(str(pgnum))
                            OUT_STRING.append(str(MS_ORIG_SIZE[pgnum]))
                            OUT_STRING.append(str(MS_ORIG_CONTENT[pgnum]))
                            OUT_STRING.append(str(MS_ORIG_CONTENT[pgnum] / MS_ORIG_SIZE[pgnum]))
                            OUT_STRING.append(str(MS_CONV_SIZE[pgnum]))
                            OUT_STRING.append(str(MS_CONV_CONTENT[pgnum]))
                            OUT_STRING.append(str(MS_CONV_CONTENT[pgnum] / MS_CONV_SIZE[pgnum]))
                            OUT_STRING.append(str(EXPORT_RED[pgnum]))
                            OUT_STRING.append(str(EXPORT_RED[pgnum] / MS_CONV_CONTENT[pgnum]))
                            if IS_FILE_MS_PREV:
                                OUT_STRING.append(str(MS_PREV_SIZE[pgnum]))
                                OUT_STRING.append(str(MS_PREV_CONTENT[pgnum]))
                                OUT_STRING.append(str(MS_PREV_CONTENT[pgnum] / MS_PREV_SIZE[pgnum]))
                                OUT_STRING.append(str(PREV_EXPORT_RED[pgnum]))
                                OUT_STRING.append(str(PREV_EXPORT_RED[pgnum] / MS_PREV_CONTENT[pgnum]))
                            f.write(','.join(OUT_STRING) + '\n')

                    with open('diff-pdf-' + file_ext[1][1:] + '-statistics-anomalies.csv', 'a') as f:
                        if IS_FILE_LO_PREV and len(LO_ORIG_PDF.sequence) != LO_PREV_PAGES:
                           f.write(args.base_file + f",import,page count different from {args.history_dir} [{LO_PREV_PAGES}] and converted [{len(LO_ORIG_PDF.sequence)}]" + '\n')
                        if IS_FILE_MS_PREV and len(MS_CONV_PDF.sequence) != MS_PREV_PAGES:
                           f.write(args.base_file + f",export,page count different from {args.history_dir} [{MS_PREV_PAGES}] and converted [{len(MS_CONV_PDF.sequence)}]" + '\n')
                        for pgnum in range(0, len(RED_COLOR)):
                           printdebug(DEBUG, "DEBUG: red[", RED_COLOR[pgnum],"] compared to wand.color.Color('red') on page " + str(pgnum))
                           if RED_COLOR[pgnum] != wand.color.Color('red'):
                               if MS_ORIG_SIZE[pgnum] != MS_ORIG_CONTENT[pgnum]: # false positive: blank page
                                   f.write(args.base_file + f",red color,page {pgnum}," + RED_COLOR[pgnum].normalized_string + '\n')

                    os.remove(LOCK_FILE)
                    return
                else:
                    printdebug(DEBUG, "DEBUG: not my lock after all - try again")
        time.sleep(1) # second


if __name__ == "__main__":
    main()

