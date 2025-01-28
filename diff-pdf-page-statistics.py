#!/usr/bin/python3

#NOTE: you probably need to increase the cache allowed: /etc/ImageMagick-*/policy.xml
#      <policy domain="resource" name="disk" value="16GiB"/>

# find docx/ -name "*.docx" -execdir basename {} \; | xargs -L1 -I{} python3 ../diff-pdf-page-statistics.py --no_save_overlay  --base_file="{}"
#       - first delete any CSV files. Delete the import/export folders in converted
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
#   - shifting MSO layout - captured before the layout has been finalized
#   - automatically updating fields: dates, =rand(), slide date/time ...
#   - different font subsitutions (especially export, where Writer-chosen font is forced on round-trip)
#   - compatibilityMode change: export forces a specific compatibility mode - which may be different from the original: reference META bug tdf#131304

import argparse
import os
import wand # pip install wand && OS_INSTALLER install imagemagick
from wand.image import Image
from wand.display import display
from wand.exceptions import PolicyError
from wand.exceptions import CacheError
import time

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
    parser.add_argument("--max_page", default="10") # limit PDF comparison to the first ten pages
    parser.add_argument("--no_save_overlay", action="store_true") # default is false
    parser.add_argument("--resolution", default="75")
    parser.add_argument("--debug", action="store_true") # default is false
    args = parser.parse_args()

    DEBUG = args.debug
    MAX_PAGES = int(args.max_page)

    # Exclude notorious false positives that have no redeeming value in constantly brought to the attention of QA
    if (
        args.base_file == 'forum-mso-de-108371.xlsx' # =rand()
        or args.base_file == 'forum-mso-de-70016.docx' # =rand()
        or args.base_file == 'forum-mso-en-1268.docx' # =rand()
        or args.base_file == 'ooo34927-2.docx' # excessively font dependent
        or args.base_file == 'fdo44257-1.docx' # constantly appears as false positive: 10 page OLE fallback images
        or args.base_file == 'forum-fr-9115.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-17720.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-108628.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-108634.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-136404.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-136586.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-136590.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-136592.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-136593.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-47433.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-47852.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-49007.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-49810.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-60957.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-60969.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-63802.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-63804.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-65676.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-65888.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-65903.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-68067.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-69965.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-69973.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-70532.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-71460.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-71579.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-72350.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-72909.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-74194.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-75643.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-76198.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-79405.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-80865.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-86412.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-88527.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-88537.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-90801.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-92011.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-92780.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-1239.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-2456.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-2675.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-10944.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en3-17910.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en3-18456.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-182337.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-218615.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-449246.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-449409.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-449411.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-627249.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-672510.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-676124.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-676302.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-717469.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-82761.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-82825.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-83519.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-83520.docx' # date/time/temp-filename field
        or args.base_file == 'tdf116486-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf130041-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf134901-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf137610-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf137610-3.docx' # date/time/temp-filename field
        or args.base_file == 'fdo83312-6.docx' # effective duplicate
        or args.base_file == 'forum-fr-16236.docx' # effective duplicate
        or args.base_file == 'forum-fr-16238.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-54647.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-126251.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-139701.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-3785.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-3786.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-5125.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-5126.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-17298.docx' # effective duplicate
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

    # MSO's PDF of the exported file needs to be manually renamed to match the base_file nane. Helpfully exit if we detect that has not occurred yet.
    if file_ext[1] == ".doc" and not IS_FILE_MS_PREV and os.path.isfile(os.path.join(args.history_dir, file_ext[1][1:], file_ext[0] + ".docx_mso.pdf")):
        print ("Previous MS converted PDF not renamed. rename s/.docx_mso.pdf/.doc_mso.pdf/ doc/*.docx_mso.pdf")
        exit (1)
    if file_ext[1] == ".xls" and not IS_FILE_MS_PREV and os.path.isfile(os.path.join(args.history_dir, file_ext[1][1:], file_ext[0] + ".xlsx_mso.pdf")):
        print ("Previous MS converted PDF not renamed. rename s/.xlsx_mso.pdf/.xls_mso.pdf/ xls/*.xlsx_mso.pdf")
        exit (1)
    if file_ext[1] == ".ppt" and not IS_FILE_MS_PREV and os.path.isfile(os.path.join(args.history_dir, file_ext[1][1:], file_ext[0] + ".pptx_mso.pdf")):
        print ("Previous MS converted PDF not renamed. rename s/.pptx_mso.pdf/.ppt_mso.pdf/ ppt/*.pptx_mso.pdf")
        exit (1)

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
        MS_ORIG_PDF = Image(filename=MS_ORIG, resolution=int(args.resolution))

        # A PDF of how it is displayed in Writer - to be compared to MS_ORIG
        LO_ORIG_PDF = Image(filename=LO_ORIG, resolution=int(args.resolution))

        # A PDF of how MS Word displays Writer's round-tripped file - to be compared to MS_ORIG
        MS_CONV_PDF = Image(filename=MS_CONV, resolution=int(args.resolution))

        # A historical version of how it was displayed in Writer
        LO_PREV_PDF = Image()
        LO_PREV_PAGES = MAX_PAGES
        if IS_FILE_LO_PREV:
            LO_PREV_PDF = Image(filename=LO_PREV, resolution=int(args.resolution))
            LO_PREV_PAGES = len(LO_PREV_PDF.sequence)

        # A historical version of how the round-tripped file was displayed in Word
        MS_PREV_PDF = Image()
        MS_PREV_PAGES = MAX_PAGES
        if IS_FILE_MS_PREV:
            MS_PREV_PDF = Image(filename=MS_PREV, resolution=int(args.resolution))
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

    IMPORT_RED = []     # the number of red pixels on the page
    EXPORT_RED = []
    PREV_IMPORT_RED = []
    PREV_EXPORT_RED = []

    MS_ORIG_RED = MS_ORIG_PDF.clone()
    for pgnum in range(0, pages):
        with MS_ORIG_RED.sequence[pgnum] as page: # need this 'with' clause so that MS_ORIG_RED is actually updated with the following changes
            MS_ORIG_SIZE.append(page.height * page.width)
            page.transform_colorspace('gray')
            page.alpha_channel = 'remove'         # so that 'red' will be painted as 'red' and not some transparent-ized shade of red
            page.opaque_paint('black', 'red', fuzz=MS_ORIG_PDF.quantum_range * 0.90)

    # Composed image: overlay red MS_ORIG with LO_ORIG
    IMPORT_IMAGE = MS_ORIG_RED.clone()
    # Composed image: overlay red MS_ORIG with MS_CONV
    EXPORT_IMAGE = MS_ORIG_RED.clone()

    # Composed image: overlay red MS_ORIG with LO_PREV
    PREV_IMPORT_IMAGE = MS_ORIG_RED.clone()
    # Composed image: overlay red MS_ORIG with MS_PREV
    PREV_EXPORT_IMAGE = MS_ORIG_RED.clone()

    # Composed image: overlay red LO_ORIG and blue LO_PREV with gray MS_ORIG_PDF
    # This is the visual key to the whole tool. The red/blue underlay should be identical except for import fixes or regressions
    IMPORT_COMPARE_IMAGE = LO_ORIG_PDF.clone()
    # Composed image: overlay red MS_CONV and blue MS_PREV with gray MS_ORIG_PDF
    # This is the visual key to the whole tool. The red/blue underlay should be identical except for export fixes or regressions
    EXPORT_COMPARE_IMAGE = MS_CONV_PDF.clone()

    for pgnum in range(0, pages):
        tmp = MS_ORIG_PDF.clone()  # don't make changes to these PDF pages - just get statistics...
        with tmp.sequence[pgnum] as page:
            page.quantize(2)
            MS_ORIG_SIZE.append(page.height * page.width)
            MS_ORIG_CONTENT.append(min(list(page.histogram.values()))) # assuming that the background is more than 50%
            printdebug(DEBUG, "DEBUG LO_ORIG[", pgnum, "] size[", MS_ORIG_SIZE[pgnum], "] content[", MS_ORIG_CONTENT[pgnum], "] percent[", (MS_ORIG_CONTENT[pgnum] / MS_ORIG_SIZE[pgnum]), "] colorspace[", page.colorspace, "] background[", page.background_color, "] ", list(page.histogram.values()), list(page.histogram.keys()))

        tmp = LO_ORIG_PDF.clone()
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
            LO_ORIG_PDF.sequence[pgnum].transform_colorspace('gray')
            LO_ORIG_PDF.sequence[pgnum].transparent_color(LO_ORIG_PDF.background_color, 0, fuzz=LO_ORIG_PDF.quantum_range * 0.10)
            #display(Image(page))  #debug
            #display(Image(LO_ORIG_PDF.sequence[pgnum]))  #debug
            page.composite(LO_ORIG_PDF.sequence[pgnum])  # overlay (red) MS_ORIG with LO_ORIG
            page.merge_layers('flatten')
            #display(Image(page))  #debug
        IMPORT_RED.append(0)
        try:
            IMPORT_RED[pgnum] = IMPORT_IMAGE.sequence[pgnum].histogram[wand.color.Color('red')]
        except:
            printdebug(DEBUG, "IMPORT EXCEPTION: could not get red color from page ", pgnum)#, list(IMPORT_IMAGE.sequence[pgnum].histogram.keys()))

        with EXPORT_IMAGE.sequence[pgnum] as page:
            MS_CONV_PDF.sequence[pgnum].transform_colorspace('gray')
            MS_CONV_PDF.sequence[pgnum].transparent_color(MS_CONV_PDF.background_color, 0, fuzz=MS_CONV_PDF.quantum_range * 0.10)
            page.composite(MS_CONV_PDF.sequence[pgnum]) # overlay (red) MS_ORIG with MS_CONV
            page.merge_layers('flatten')
        EXPORT_RED.append(0)
        try:
            EXPORT_RED[pgnum] = EXPORT_IMAGE.sequence[pgnum].histogram[wand.color.Color('red')]
        except:
            printdebug(DEBUG, "EXPORT EXCEPTION: could not get red color from page ", pgnum)# , list(EXPORT_IMAGE.sequence[pgnum].histogram.keys()))

        PREV_IMPORT_RED.append(0)
        if IS_FILE_LO_PREV:
            with PREV_IMPORT_IMAGE.sequence[pgnum] as page:
                LO_PREV_PDF.sequence[pgnum].transform_colorspace('gray')
                LO_PREV_PDF.sequence[pgnum].transparent_color(LO_PREV_PDF.background_color, 0, fuzz=LO_PREV_PDF.quantum_range * 0.10)
                page.composite(LO_PREV_PDF.sequence[pgnum]) # overlay (red) MS_ORIG with LO_PREV
                page.merge_layers('flatten')
            try:
                PREV_IMPORT_RED[pgnum] = PREV_IMPORT_IMAGE.sequence[pgnum].histogram[wand.color.Color('red')]
            except:
                printdebug(DEBUG, "PREV_IMPORT EXCEPTION: could not get red color from page ", pgnum)#, list(PREV_IMPORT_IMAGE.sequence[pgnum].histogram.keys()))

            with IMPORT_COMPARE_IMAGE.sequence[pgnum] as page:
                page.transform_colorspace('gray')
                page.opaque_paint('black', 'red', fuzz=LO_ORIG_PDF.quantum_range * 0.90)
                LO_PREV_PDF.sequence[pgnum].transparent_color(LO_PREV_PDF.background_color, 0, fuzz=LO_PREV_PDF.quantum_range * 0.10)
                LO_PREV_PDF.sequence[pgnum].opaque_paint('black', 'blue', fuzz=LO_PREV_PDF.quantum_range * 0.90)
                page.composite(LO_PREV_PDF.sequence[pgnum]) # overlay (red) LO_ORIG with (blue) LO_PREV
                MS_ORIG_PDF.sequence[pgnum].transparent_color(MS_ORIG_PDF.background_color, 0, fuzz=MS_ORIG_PDF.quantum_range * 0.10)
                MS_ORIG_PDF.sequence[pgnum].transform_colorspace('gray')
                page.composite(MS_ORIG_PDF.sequence[pgnum]) # overlay both with the authoritative contents in gray
                page.merge_layers('flatten')

        PREV_EXPORT_RED.append(0)
        if IS_FILE_MS_PREV:
            with PREV_EXPORT_IMAGE.sequence[pgnum] as page:
                MS_PREV_PDF.sequence[pgnum].transform_colorspace('gray')
                MS_PREV_PDF.sequence[pgnum].transparent_color(MS_PREV_PDF.background_color, 0, fuzz=MS_PREV_PDF.quantum_range * 0.10)
                page.composite(MS_PREV_PDF.sequence[pgnum]) # overlay (red) MS_ORIG with MS_PREV
                page.merge_layers('flatten')
            try:
                PREV_EXPORT_RED[pgnum] = PREV_EXPORT_IMAGE.sequence[pgnum].histogram[wand.color.Color('red')]
            except:
                printdebug(DEBUG, "PREV_EXPORT EXCEPTION: could not get red color from page ", pgnum)#, list(PREV_EXPORT_IMAGE.sequence[pgnum].histogram.keys()))

            with EXPORT_COMPARE_IMAGE.sequence[pgnum] as page:
                page.transform_colorspace('gray')
                page.opaque_paint('black', 'red', fuzz=MS_CONV_PDF.quantum_range * 0.90)
                MS_PREV_PDF.sequence[pgnum].transparent_color(MS_PREV_PDF.background_color, 0, fuzz=MS_PREV_PDF.quantum_range * 0.10)
                MS_PREV_PDF.sequence[pgnum].opaque_paint('black', 'blue', fuzz=MS_PREV_PDF.quantum_range * 0.90)
                page.composite(MS_PREV_PDF.sequence[pgnum]) # overlay (red) MS_CONV with (blue) MS_PREV
                MS_ORIG_PDF.sequence[pgnum].transparent_color(MS_ORIG_PDF.background_color, 0, fuzz=MS_ORIG_PDF.quantum_range * 0.10)
                MS_ORIG_PDF.sequence[pgnum].transform_colorspace('gray')
                page.composite(MS_ORIG_PDF.sequence[pgnum]) # overlay both with the authoritative contents in gray
                page.merge_layers('flatten')


    for pageToSave in range(0, pages):
        FORCE_SAVE_IMPORT = False
        FORCE_SAVE_EXPORT = False
        if args.no_save_overlay == True:
            # Always provide pages that have more RED now than in the previous version - QA needs to check them out (at least the first one).
            if IS_FILE_LO_PREV and IMPORT_RED[pageToSave] > PREV_IMPORT_RED[pageToSave]:
                FORCE_SAVE_IMPORT = True
            if IS_FILE_MS_PREV and EXPORT_RED[pageToSave] > PREV_EXPORT_RED[pageToSave]:
                FORCE_SAVE_EXPORT = True
        if args.no_save_overlay == False or FORCE_SAVE_IMPORT:
            printdebug(DEBUG, f"DEBUG saving {args.base_file} page {pageToSave+1} IMPORT[{IMPORT_RED[pageToSave]} PREV[{PREV_IMPORT_RED[pageToSave]}]")
            with Image(IMPORT_IMAGE.sequence[pageToSave]) as img_to_save:
                file_name=os.path.join(IMPORT_DIR, args.base_file + f"_import-{pageToSave+1}.png")
                img_to_save.save(filename=file_name)
            if IS_FILE_LO_PREV:
                with Image(PREV_IMPORT_IMAGE.sequence[pageToSave]) as img_to_save:
                    file_name=os.path.join(IMPORT_DIR, args.base_file + f"_prev-import-{pageToSave+1}.png")
                    img_to_save.save(filename=file_name)
                with Image(IMPORT_COMPARE_IMAGE.sequence[pageToSave]) as img_to_save:
                    file_name=os.path.join(IMPORT_COMPARE_DIR, args.base_file + f"_import-compare-{pageToSave+1}.png")
                    img_to_save.save(filename=file_name)

        if args.no_save_overlay == False or FORCE_SAVE_EXPORT:
            printdebug(DEBUG, f"DEBUG saving {args.base_file} page {pageToSave+1} EXPORT[{EXPORT_RED[pageToSave]} PREV[{PREV_EXPORT_RED[pageToSave]}]")
            with Image(EXPORT_IMAGE.sequence[pageToSave]) as img_to_save:
                file_name=os.path.join(EXPORT_DIR, args.base_file + f"_export-{pageToSave+1}.png")
                img_to_save.save(filename=file_name)
            if IS_FILE_MS_PREV:
                with Image(PREV_EXPORT_IMAGE.sequence[pageToSave]) as img_to_save:
                    file_name=os.path.join(EXPORT_DIR, args.base_file + f"_prev-export-{pageToSave+1}.png")
                    img_to_save.save(filename=file_name)
                with Image(EXPORT_COMPARE_IMAGE.sequence[pageToSave]) as img_to_save:
                    file_name=os.path.join(EXPORT_COMPARE_DIR, args.base_file + f"_export-compare-{pageToSave+1}.png")
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
                            OUT_STRING.append(str(pgnum + 1))  # use a human-oriented 1-based number for reporting...
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
                            OUT_STRING.append(str(pgnum + 1))  # use a human-oriented 1-based number for reporting...
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
                            f.write(args.base_file + f",import,page count different from {args.history_dir} [{LO_PREV_PAGES}] and converted [{len(LO_ORIG_PDF.sequence)}]. Should be[{len(MS_ORIG_PDF.sequence)}]" + '\n')
                        if IS_FILE_MS_PREV and len(MS_CONV_PDF.sequence) != MS_PREV_PAGES:
                            f.write(args.base_file + f",export,page count different from {args.history_dir} [{MS_PREV_PAGES}] and converted [{len(MS_CONV_PDF.sequence)}]. Should be[{len(MS_ORIG_PDF.sequence)}]" + '\n')
                        # Although absolute wrongs normally shouldn't be reported (only report a change from previous version) - wrong page count was requested to be an exception.
                        if len(LO_ORIG_PDF.sequence) != len(MS_ORIG_PDF.sequence):
                            f.write(args.base_file + f",import, absolute page count, {len(LO_ORIG_PDF.sequence)}, should be, {len(MS_ORIG_PDF.sequence)}" + '\n')
                        if len(MS_CONV_PDF.sequence) != len(MS_ORIG_PDF.sequence):
                            f.write(args.base_file + f",export, absolute page count, {len(MS_CONV_PDF.sequence)}, should be, {len(MS_ORIG_PDF.sequence)}" + '\n')

                    os.remove(LOCK_FILE)
                    return
                else:
                    printdebug(DEBUG, "DEBUG: not my lock after all - try again")
        time.sleep(1) # second


if __name__ == "__main__":
    main()

