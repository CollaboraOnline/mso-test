#!/usr/bin/python3

#NOTE: you probably need to increase the cache allowed: /etc/ImageMagick-*/policy.xml
#      <policy domain="resource" name="disk" value="16GiB"/>

# find ../download/docx/ -name "*.docx" -execdir basename {} \; | xargs -L1 -I{} python3 ../diff-pdf-page-statistics.py --no_save_overlay  --base_file="{}"
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
import subprocess
import time
import glob

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
    parser.add_argument("--image_dump", action="store_true") # default is false
    args = parser.parse_args()

    DEBUG = args.debug
    MAX_PAGES = int(args.max_page)

    # Exclude notorious false positives that have no redeeming value in constantly brought to the attention of QA
    if (
        args.base_file == 'forum-mso-de-108371.xlsx' # =rand()
        or args.base_file == 'forum-mso-de-70016.docx' # =rand()
        # ppt
        or args.base_file == 'forum-mso-de-33441.ppt' # date/time/temp-filename field
        or args.base_file == 'fdo47526-1.ppt' # dup
        or args.base_file == 'forum-mso-de-11842.ppt' # dup
        # pptx
        or args.base_file == 'kde249182-3.pptx' # date/time/temp-filename field
        or args.base_file == 'fdo74423-2.pptx' # dup
        or args.base_file == 'forum-mso-en-3693.pptx' # dup
        or args.base_file == 'moz1121068-2.pptx' # dup
        or args.base_file == 'tdf125744-10.pptx' # dup
        or args.base_file == 'tdf125770-1.pptx' # dup
        or args.base_file == 'tdf125771-1.pptx' # dup
        or args.base_file == 'tdf125772-1.pptx' # dup
        # doc
        or args.base_file == 'fdo39327-2.doc' # constantly appears as false positive
        or args.base_file == 'fdo44839-2.doc' # constantly appears as false positive
        or args.base_file == 'fdo55620-1.doc' # constantly appears as false positive
        or args.base_file == 'forum-de3-351.doc' # constantly appears as false positive
        or args.base_file == 'forum-en-17173.doc' # constantly appears as false positive
        or args.base_file == 'forum-mso-en4-218597.doc' # constantly appears as false positive
        or args.base_file == 'ooo39362-1.doc' # constantly appears as false positive
        or args.base_file == 'ooo58118-1.doc' # constantly appears as false positive
        or args.base_file == 'ooo61574-4.doc' # constantly appears as false positive
        or args.base_file == 'ooo91654-1.doc' # constantly appears as false positive
        or args.base_file == 'ooo101660-2.doc' # constantly appears as false positive
        or args.base_file == 'ooo106353-1.doc' # constantly appears as false positive
        or args.base_file == 'fdo41765-1.doc' # constantly appears as false positive: OLE
        or args.base_file == 'forum-en-477.doc' # constantly appears as false positive: OLE
        or args.base_file == 'forum-en-20796.doc' # constantly appears as false positive: OLE
        or args.base_file == 'forum-en-43597.doc' # constantly appears as false positive: OLE
        or args.base_file == 'forum-fr-17596.doc' # constantly appears as false positive: OLE
        or args.base_file == 'forum-fr-4907.doc' # constantly appears as false positive: OLE
        or args.base_file == 'forum-it-4309.doc' # constantly appears as false positive: OLE
        or args.base_file == 'forum-mso-de-1674.doc' # constantly appears as false positive: OLE
        or args.base_file == 'kde239424-1.doc' # constantly appears as false positive: OLE
        or args.base_file == 'kde251338-1.doc' # constantly appears as false positive: OLE
        or args.base_file == 'ooo50472-1.doc' # constantly appears as false positive: OLE
        or args.base_file == 'ooo52181-1.doc' # constantly appears as false positive: OLE
        or args.base_file == 'fdo34319-2.doc' # unusual font
        or args.base_file == 'fdo37926-1.doc' # unusual font
        or args.base_file == 'fdo58277-1.doc' # unusual font
        or args.base_file == 'fdo87036-1.doc' # unusual font
        or args.base_file == 'forum-en-16406.doc' # unusual font
        or args.base_file == 'forum-en-16407.doc' # unusual font
        or args.base_file == 'forum-en-17158.doc' # unusual font
        or args.base_file == 'forum-en-27403.doc' # unusual font
        or args.base_file == 'forum-en-37111.doc' # unusual font
        or args.base_file == 'forum-mso-en-3104.doc' # unusual font
        or args.base_file == 'ooo20105-2.doc' # unusual font
        or args.base_file == 'ooo34207-3.doc' # unusual font
        or args.base_file == 'ooo38158-1.doc' # unusual font
        or args.base_file == 'ooo38158-4.doc' # unusual font
        or args.base_file == 'ooo38158-5.doc' # unusual font
        or args.base_file == 'ooo43866-2.doc' # unusual font
        or args.base_file == 'ooo45085-2.doc' # unusual font
        or args.base_file == 'ooo32650-1.doc' # unusual font
        or args.base_file == 'ooo72915-1.doc' # unusual font
        or args.base_file == 'rhbz163105-1.doc' # unusual font
        or args.base_file == 'tdf89409-1.doc' # unusual font
        or args.base_file == 'tdf93695-1.doc' # unusual font
        or args.base_file == 'tdf95104-1.doc' # unusual font
        or args.base_file == 'tdf104544-1.doc' # unusual font
        or args.base_file == 'fdo39005-1.doc' # formula
        or args.base_file == 'fdo45284-2.doc' # formula
        or args.base_file == 'fdo45284-3.doc' # formula
        or args.base_file == 'lp881390-3.doc' # formula
        or args.base_file == 'ooo12856-1.doc' # formula
        or args.base_file == 'ooo15545-1.doc' # formula
        or args.base_file == 'ooo24340-1.doc' # formula
        or args.base_file == 'ooo26341-1.doc' # formula
        or args.base_file == 'ooo49794-1.doc' # formula
        or args.base_file == 'ooo49798-2.doc' # formula
        or args.base_file == 'ooo63072-1.doc' # formula
        or args.base_file == 'ooo80133-1.doc' # formula
        or args.base_file == 'ooo83641-1.doc' # formula
        or args.base_file == 'ooo85069-2.doc' # formula
        or args.base_file == 'ooo88869-2.doc' # formula
        or args.base_file == 'ooo90263-1.doc' # formula
        or args.base_file == 'ooo95326-1.doc' # formula
        or args.base_file == 'ooo97050-1.doc' # formula
        or args.base_file == 'ooo112342-1.doc' # formula
        or args.base_file == 'tdf93774-1.doc' # formula
        or args.base_file == 'tdf94459-1.doc' # formula
        or args.base_file == 'tdf133081-1.doc' # formula
        or args.base_file == 'abi7300-3.doc' # date/time/temp-filename field
        or args.base_file == 'abi9044-1.doc' # date/time/temp-filename field
        or args.base_file == 'abi9483-1.doc' # date/time/temp-filename field
        or args.base_file == 'fdo34293-1.doc' # date/time/temp-filename field
        or args.base_file == 'fdo36035-1.doc' # date/time/temp-filename field
        or args.base_file == 'fdo42476-1.doc' # date/time/temp-filename field
        or args.base_file == 'fdo45122-2.doc' # date/time/temp-filename field
        or args.base_file == 'fdo46026-4.doc' # date/time/temp-filename field
        or args.base_file == 'fdo46203-2.doc' # date/time/temp-filename field
        or args.base_file == 'fdo49989-1.doc' # date/time/temp-filename field
        or args.base_file == 'fdo62419-3.doc' # date/time/temp-filename field
        or args.base_file == 'fdo66620-1.doc' # date/time/temp-filename field
        or args.base_file == 'fdo76473-2.doc' # date/time/temp-filename field
        or args.base_file == 'fdo80094-1.doc' # date/time/temp-filename field
        or args.base_file == 'fdo85607-1.doc' # date/time/temp-filename field
        or args.base_file == 'forum-de2-3448.doc' # date/time/temp-filename field
        or args.base_file == 'forum-de2-5389.doc' # date/time/temp-filename field
        or args.base_file == 'forum-de3-2825.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-1156.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-2522.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-4213.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-11174.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-11794.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-11795.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-13849.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-17538.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-23373.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-26206.doc' # date/time/temp-filename field
        or args.base_file == 'forum-en-34278.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-5305.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-5996.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-7782.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-7785.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-9115.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-15987.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-15999.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-17180.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-17699.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-17720.doc' # date/time/temp-filename field
        or args.base_file == 'forum-fr-25000.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-786.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-787.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-2342.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-4865.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-4933.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-9155.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-10356.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-10921.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-11203.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-11204.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-11643.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-11643.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-12204.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-12422.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-12516.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-12519.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-12532.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-12670.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-12720.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-13519.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-14766.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-15127.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-15308.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-15313.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-15314.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-16290.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-16305.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-16336.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-16501.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-20305.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-21623.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-22541.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-23715.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-24267.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-24273.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-24022.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-27240.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-29634.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-29637.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-31100.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-33104.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-33110.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-33690.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-34264.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-34550.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-35792.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-35794.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-36734.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-36735.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-38413.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-46375.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-46368.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-48711.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-49489.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-52037.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-52383.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-53089.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-54145.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-62247.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-62373.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-62399.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-65226.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-66536.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-68675.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-69695.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-75302.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-77295.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-80741.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-92542.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-95008.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-113100.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-125088.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-139552.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-412.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-1550.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-1551.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-1575.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-4059.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-4060.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-5031.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-5477.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-5565.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en3-14938.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en3-14961.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-96871.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-96872.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-164788.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-179419.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-190815.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-383921.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-383922.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-408957.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-416020.doc' # date/time/temp-filename field
        or args.base_file == 'kde67216-2.doc' # date/time/temp-filename field
        or args.base_file == 'kde239143-8.doc' # date/time/temp-filename field
        or args.base_file == 'kde239659-1.doc' # date/time/temp-filename field
        or args.base_file == 'kde239691-1.doc' # date/time/temp-filename field
        or args.base_file == 'kde239699-4.doc' # date/time/temp-filename field
        or args.base_file == 'kde250568-1.doc' # date/time/temp-filename field
        or args.base_file == 'kde271984-1.doc' # date/time/temp-filename field
        or args.base_file == 'lp60185-1.doc' # date/time/temp-filename field
        or args.base_file == 'lp1050383-1.doc' # date/time/temp-filename field
        or args.base_file == 'moz577665-2.doc' # date/time/temp-filename field
        or args.base_file == 'moz596764-2.doc' # date/time/temp-filename field
        or args.base_file == 'novell423729-1.doc' # date/time/temp-filename field
        or args.base_file == 'novell653526-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo1140-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo1304-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo2129-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo2593-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo24648-3.doc' # date/time/temp-filename field
        or args.base_file == 'ooo25601-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo26091-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo29646-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo29646-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo31052-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo32554-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo32819-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo36447-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo36594-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo36594-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo36594-6.doc' # date/time/temp-filename field
        or args.base_file == 'ooo36594-7.doc' # date/time/temp-filename field
        or args.base_file == 'ooo36594-9.doc' # date/time/temp-filename field
        or args.base_file == 'ooo38143-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo38959-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo39549-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo43369-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo43718-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo43833-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo44769-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo45051-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo46648-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo48192-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo48822-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo49538-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo50715-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo51449-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo51449-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo56465-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo56654-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo56743-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo58454-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo58454-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo60170-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo61627-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo62496-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo64927-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo64927-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo65485-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo66570-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo66990-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo70163-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo70163-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo70363-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo70363-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo71302-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo72151-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo72176-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo74244-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo75170-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo77118-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo79388-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo80702-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo82047-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo88597-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo92840-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo94820-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo98407-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo105196-2.doc' # date/time/temp-filename field
        or args.base_file == 'ooo110375-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo117044-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo119810-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo119913-1.doc' # date/time/temp-filename field
        or args.base_file == 'ooo120158-1.doc' # date/time/temp-filename field
        or args.base_file == 'tdf98761-2.doc' # date/time/temp-filename field
        or args.base_file == 'tdf127078-2.doc' # date/time/temp-filename field
        or args.base_file == 'fdo35158-5.doc' # dup
        or args.base_file == 'forum-en-2974.doc' # dup
        or args.base_file == 'forum-en-19554.doc' # dup
        or args.base_file == 'forum-en-19562.doc' # dup
        or args.base_file == 'forum-fr-7789.doc' # dup
        or args.base_file == 'forum-mso-de-4868.doc' # dup
        or args.base_file == 'forum-mso-de-5466.doc' # dup
        or args.base_file == 'forum-mso-de-5467.doc' # dup
        or args.base_file == 'forum-mso-de-5540.doc' # dup
        or args.base_file == 'forum-mso-de-12432.doc' # dup
        or args.base_file == 'forum-mso-de-12520.doc' # dup
        or args.base_file == 'forum-mso-de-12740.doc' # dup
        or args.base_file == 'forum-mso-de-15295.doc' # dup
        or args.base_file == 'forum-mso-de-15315.doc' # dup
        or args.base_file == 'forum-mso-de-15355.doc' # dup
        or args.base_file == 'forum-mso-de-15371.doc' # dup
        or args.base_file == 'forum-mso-de-16353.doc' # dup
        or args.base_file == 'forum-mso-de-22546.doc' # dup
        or args.base_file == 'forum-mso-de-51933.doc' # dup
        or args.base_file == 'forum-mso-de-51979.doc' # dup
        or args.base_file == 'forum-mso-de-51990.doc' # dup
        or args.base_file == 'forum-mso-de-53090.doc' # dup
        or args.base_file == 'forum-mso-de-53095.doc' # dup
        or args.base_file == 'forum-mso-de-61448.doc' # dup
        or args.base_file == 'forum-mso-de-66416.doc' # dup
        or args.base_file == 'forum-mso-de-69704.doc' # dup
        or args.base_file == 'forum-mso-de-69714.doc' # dup
        or args.base_file == 'forum-mso-de-77313.doc' # dup
        or args.base_file == 'forum-mso-de-124992.doc' # dup
        or args.base_file == 'forum-mso-de-125055.doc' # dup
        or args.base_file == 'forum-mso-de-130689.doc' # dup
        or args.base_file == 'forum-mso-en-2399.doc' # dup
        or args.base_file == 'forum-mso-en-2399.doc' # dup
        or args.base_file == 'forum-mso-en-4061.doc' # dup
        or args.base_file == 'forum-mso-en-4350.doc' # dup
        or args.base_file == 'forum-mso-en-4353.doc' # dup
        or args.base_file == 'forum-mso-en3-14831.doc' # dup
        or args.base_file == 'forum-mso-en3-14901.doc' # dup
        or args.base_file == 'forum-mso-en4-290223.doc' # dup
        or args.base_file == 'forum-mso-en4-290467.doc' # dup
        or args.base_file == 'forum-mso-en4-290483.doc' # dup
        or args.base_file == 'forum-mso-en4-409511.doc' # dup
        or args.base_file == 'moz1031624-4.doc' # dup
        or args.base_file == 'moz1031624-5.doc' # dup
        or args.base_file == 'ooo54420-1.doc' # dup
        or args.base_file == 'ooo72151-3.doc' # dup
        or args.base_file == 'abi9887-2.doc' # solid blue
        or args.base_file == 'abi10025-3.doc' # solid blue
        or args.base_file == 'gnome166775-1.doc' # solid blue
        or args.base_file == 'lp332943-1.doc' # solid blue
        or args.base_file == 'lp332943-3.doc' # solid blue
        or args.base_file == 'ooo13008-1.doc' # solid blue
        or args.base_file == 'ooo23297-2.doc' # solid blue
        or args.base_file == 'ooo23575-1.doc' # solid blue
        or args.base_file == 'ooo25373-3.doc' # solid blue
        or args.base_file == 'ooo25522-1.doc' # solid blue
        or args.base_file == 'ooo27951-2.doc' # solid blue
        or args.base_file == 'ooo29165-3.doc' # solid blue
        or args.base_file == 'ooo29395-1.doc' # solid blue
        or args.base_file == 'ooo29682-3.doc' # solid blue
        or args.base_file == 'ooo30913-2.doc' # solid blue
        or args.base_file == 'ooo33801-2.doc' # solid blue
        or args.base_file == 'ooo33996-1.doc' # solid blue
        or args.base_file == 'ooo35019-1.doc' # solid blue
        or args.base_file == 'ooo35206-1.doc' # solid blue
        or args.base_file == 'ooo39849-1.doc' # solid blue
        or args.base_file == 'ooo40585-1.doc' # solid blue
        or args.base_file == 'ooo42420-1.doc' # solid blue
        or args.base_file == 'ooo45928-1.doc' # solid blue
        or args.base_file == 'ooo46222-2.doc' # solid blue
        or args.base_file == 'ooo51220-1.doc' # solid blue
        or args.base_file == 'ooo57258-2.doc' # solid blue
        or args.base_file == 'ooo70144-1.doc' # solid blue
        or args.base_file == 'ooo93630-1.doc' # solid blue
        or args.base_file == 'ooo102227-1.doc' # solid blue
        or args.base_file == 'rhbz157925-1.doc' # solid blue
        or args.base_file == 'rhbz158052-1.doc' # solid blue
        or args.base_file == 'rhbz158066-2.doc' # solid blue
        or args.base_file == 'rhbz160141-1.doc' # solid blue
        or args.base_file == 'rhbz161432-1.doc' # solid blue
        or args.base_file == 'rhbz163138-1.doc' # solid blue
        or args.base_file == 'rhbz163670-2.doc' # solid blue
        or args.base_file == 'rhbz164137-1.doc' # solid blue
        or args.base_file == 'rhbz164152-1.doc' # solid blue
        or args.base_file == 'rhbz164691-1.doc' # solid blue
        or args.base_file == 'rhbz173474-1.doc' # solid blue
        or args.base_file == 'rhbz183617-1.doc' # solid blue
        or args.base_file == 'tdf99072-1.doc' # solid blue
        #docx
        or args.base_file == 'forum-mso-en-1268.docx' # =rand()
        or args.base_file == 'ooo34927-2.docx' # excessively font dependent
        or args.base_file == 'rhbz862326-1.docx' # extremely stupid document
        or args.base_file == 'fdo44257-1.docx' # constantly appears as false positive: 10 page OLE fallback images
        or args.base_file == 'fdo77716-1.docx' # constantly appears as false positive: 10 page OLE fallback images
        or args.base_file == 'fdo75557-1.docx' # OLE
        or args.base_file == 'fdo75557-2.docx' # OLE
        or args.base_file == 'fdo75557-3.docx' # OLE
        or args.base_file == 'fdo75557-4.docx' # OLE
        or args.base_file == 'fdo81381-1.docx' # OLE
        or args.base_file == 'fdo87977-1.docx' # OLE
        or args.base_file == 'forum-mso-en3-20916.docx' # OLE
        or args.base_file == 'forum-mso-en4-215795.docx' # OLE
        or args.base_file == 'forum-mso-en4-494109.docx' # OLE
        or args.base_file == 'kde244358-1.docx' # OLE
        or args.base_file == 'tdf66581-4.docx' # OLE
        or args.base_file == 'tdf91093-1.docx' # OLE
        or args.base_file == 'tdf103639-1.docx' # OLE
        or args.base_file == 'tdf104065-1.docx' # OLE
        or args.base_file == 'tdf109102-1.docx' # OLE
        or args.base_file == 'tdf133035-1.docx' # OLE
        or args.base_file == 'tdf133035-2.docx' # OLE
        or args.base_file == 'tdf135653-1.docx' # OLE
        or args.base_file == 'forum-mso-de-83513.docx' # constantly appears as false positive
        or args.base_file == 'forum-mso-en-8744.docx' # constantly appears as false positive
        or args.base_file == 'novell478562-1.docx' # constantly appears as false positive
        or args.base_file == 'novell653526-1.docx' # constantly appears as false positive
        or args.base_file == 'novell821804-1.docx' # constantly appears as false positive
        or args.base_file == 'tdf92597-1.docx' # constantly appears as false positive
        or args.base_file == 'tdf98681-1.docx' # constantly appears as false positive
        or args.base_file == 'tdf98681-3.docx' # constantly appears as false positive
        or args.base_file == 'tdf120551-3.docx' # constantly appears as false positive
        or args.base_file == 'tdf120551-5.docx' # constantly appears as false positive
        or args.base_file == 'abi13160-1.docx' # unusual font
        or args.base_file == 'fdo43028-1.docx' # unusual font
        or args.base_file == 'fdo65494-1.docx' # unusual font
        or args.base_file == 'fdo67087-2.docx' # unusual font
        or args.base_file == 'fdo74110-1.docx' # unusual font
        or args.base_file == 'forum-en-33446.docx' # unusual font
        or args.base_file == 'forum-es-369.docx' # unusual font
        or args.base_file == 'forum-mso-de-75152.docx' # unusual font
        or args.base_file == 'forum-mso-de-79054.docx' # unusual font
        or args.base_file == 'forum-mso-de-85674.docx' # unusual font
        or args.base_file == 'forum-mso-de-87364.docx' # unusual font
        or args.base_file == 'forum-mso-de-102385.docx' # unusual font
        or args.base_file == 'forum-mso-de-106949.docx' # unusual font
        or args.base_file == 'forum-mso-de-113918.docx' # unusual font
        or args.base_file == 'forum-mso-de-122886.docx' # unusual font
        or args.base_file == 'forum-mso-en-635.docx' # unusual font
        or args.base_file == 'forum-mso-en-786.docx' # unusual font
        or args.base_file == 'forum-mso-en-2898.docx' # unusual font
        or args.base_file == 'forum-mso-en-4865.docx' # unusual font
        or args.base_file == 'forum-mso-en-5670.docx' # unusual font
        or args.base_file == 'forum-mso-en-5678.docx' # unusual font
        or args.base_file == 'forum-mso-en-5865.docx' # unusual font
        or args.base_file == 'forum-mso-en-7261.docx' # unusual font
        or args.base_file == 'forum-mso-en-9806.docx' # unusual font
        or args.base_file == 'forum-mso-en-10349.docx' # unusual font
        or args.base_file == 'forum-mso-en-10431.docx' # unusual font
        or args.base_file == 'forum-mso-en-10513.docx' # unusual font
        or args.base_file == 'forum-mso-en-10723.docx' # unusual font
        or args.base_file == 'forum-mso-en-10729.docx' # unusual font
        or args.base_file == 'forum-mso-en-10783.docx' # unusual font
        or args.base_file == 'forum-mso-en-12532.docx' # unusual font
        or args.base_file == 'forum-mso-en-13515.docx' # unusual font
        or args.base_file == 'forum-mso-en-13800.docx' # unusual font
        or args.base_file == 'forum-mso-en-13874.docx' # unusual font
        or args.base_file == 'forum-mso-en-15345.docx' # unusual font
        or args.base_file == 'forum-mso-en-15535.docx' # unusual font
        or args.base_file == 'forum-mso-en-15688.docx' # unusual font
        or args.base_file == 'forum-mso-en-15689.docx' # unusual font
        or args.base_file == 'forum-mso-en-16276.docx' # unusual font
        or args.base_file == 'forum-mso-en-17027.docx' # unusual font
        or args.base_file == 'forum-mso-en-17656.docx' # unusual font
        or args.base_file == 'forum-mso-en-18445.docx' # unusual font
        or args.base_file == 'forum-mso-en-18727.docx' # unusual font
        or args.base_file == 'forum-mso-en-18740.docx' # unusual font
        or args.base_file == 'forum-mso-en3-11288.docx' # unusual font
        or args.base_file == 'forum-mso-en3-12276.docx' # unusual font
        or args.base_file == 'forum-mso-en3-21123.docx' # unusual font
        or args.base_file == 'forum-mso-en3-21703.docx' # unusual font
        or args.base_file == 'forum-mso-en4-265687.docx' # unusual font
        or args.base_file == 'forum-mso-en4-446134.docx' # unusual font
        or args.base_file == 'forum-mso-en4-450894.docx' # unusual font
        or args.base_file == 'forum-mso-en4-517662.docx' # unusual font
        or args.base_file == 'forum-mso-en4-673198.docx' # unusual font
        or args.base_file == 'forum-mso-en4-747862.docx' # unusual font
        or args.base_file == 'forum-mso-en4-754174.docx' # unusual font
        or args.base_file == 'forum-mso-en4-774984.docx' # unusual font
        or args.base_file == 'kde239393-1.docx' # unusual font
        or args.base_file == 'moz815635-2.docx' # unusual font
        or args.base_file == 'moz919816-4.docx' # unusual font
        or args.base_file == 'moz1564148-1.docx' # unusual font
        or args.base_file == 'ooo125270-2.docx' # unusual font
        or args.base_file == 'ooo125270-4.docx' # unusual font
        or args.base_file == 'ooo126928-1.docx' # unusual font
        or args.base_file == 'ooo127806-1.docx' # unusual font
        or args.base_file == 'tdf64991-1.docx' # unusual font
        or args.base_file == 'tdf85755-1.docx' # unusual font
        or args.base_file == 'tdf92642-1.docx' # unusual font
        or args.base_file == 'tdf96172-1.docx' # unusual font
        or args.base_file == 'tdf104162-1.docx' # unusual font
        or args.base_file == 'tdf106572-1.docx' # unusual font
        or args.base_file == 'tdf107928-1.docx' # unusual font
        or args.base_file == 'tdf107928-6.docx' # unusual font
        or args.base_file == 'tdf107928-7.docx' # unusual font
        or args.base_file == 'tdf109059-3.docx' # unusual font
        or args.base_file == 'tdf112172-1.docx' # unusual font
        or args.base_file == 'tdf114363-1.docx' # unusual font
        or args.base_file == 'tdf114363-3.docx' # unusual font
        or args.base_file == 'tdf115944-2.docx' # unusual font
        or args.base_file == 'tdf115944-4.docx' # unusual font
        or args.base_file == 'tdf118521-1.docx' # unusual font
        or args.base_file == 'tdf118521-3.docx' # unusual font
        or args.base_file == 'tdf118521-4.docx' # unusual font
        or args.base_file == 'tdf121045-1.docx' # unusual font
        or args.base_file == 'tdf124091-1.docx' # unusual font
        or args.base_file == 'tdf124536-1.docx' # unusual font
        or args.base_file == 'tdf128373-1.docx' # unusual font
        or args.base_file == 'tdf129458-1.docx' # unusual font
        or args.base_file == 'tdf131225-1.docx' # unusual font
        or args.base_file == 'tdf133873-2.docx' # unusual font
        or args.base_file == 'tdf134235-1.docx' # unusual font
        or args.base_file == 'fdo32636-1.docx' # formulas seem to really undergo layout-in-motion, and the fall-back image often doesn't seem to match modern MSO.
        or args.base_file == 'fdo32636-3.docx' # formula
        or args.base_file == 'fdo44612-1.docx' # formula
        or args.base_file == 'fdo46716-4.docx' # formula
        or args.base_file == 'fdo55743-1.docx' # formula
        or args.base_file == 'fdo58949-1.docx' # formula
        or args.base_file == 'fdo59053-4.docx' # formula
        or args.base_file == 'fdo59181-1.docx' # formula
        or args.base_file == 'fdo61638-1.docx' # formula
        or args.base_file == 'fdo66284-1.docx' # formula
        or args.base_file == 'fdo66627-1.docx' # formula
        or args.base_file == 'fdo68193-2.docx' # formula
        or args.base_file == 'fdo68997-1.docx' # formula
        or args.base_file == 'fdo74431-1.docx' # formula
        or args.base_file == 'fdo78906-1.docx' # formula
        or args.base_file == 'forum-mso-de-43275.docx' # formula
        or args.base_file == 'forum-mso-de-75783.docx' # formula
        or args.base_file == 'forum-mso-de-83512.docx' # formula
        or args.base_file == 'forum-mso-de-83513.docx' # formula
        or args.base_file == 'forum-mso-de-104623.docx' # formula
        or args.base_file == 'forum-mso-en-3685.docx' # formula
        or args.base_file == 'forum-mso-en-7009.docx' # formula
        or args.base_file == 'forum-mso-en-7125.docx' # formula
        or args.base_file == 'forum-mso-en-9109.docx' # formula
        or args.base_file == 'forum-mso-en-11003.docx' # formula
        or args.base_file == 'forum-mso-en-11772.docx' # formula
        or args.base_file == 'forum-mso-en-12492.docx' # formula
        or args.base_file == 'novell518755-1.docx' # formula
        or args.base_file == 'ooo122349-1.docx' # formula
        or args.base_file == 'tdf66525-1.docx' # formula
        or args.base_file == 'tdf66525-2.docx' # formula
        or args.base_file == 'tdf66525-3.docx' # formula
        or args.base_file == 'tdf79695-2.docx' # formula
        or args.base_file == 'tdf86716-4.docx' # formula
        or args.base_file == 'tdf95230-1.docx' # formula
        or args.base_file == 'tdf98886-1.docx' # formula
        or args.base_file == 'tdf100651-1.docx' # formula
        or args.base_file == 'tdf101378-1.docx' # formula
        or args.base_file == 'tdf112038-1.docx' # formula
        or args.base_file == 'tdf115030-2.docx' # formula
        or args.base_file == 'tdf116547-2.docx' # formula
        or args.base_file == 'tdf116547-3.docx' # formula
        or args.base_file == 'tdf117087-1.docx' # formula
        or args.base_file == 'tdf116547-11.docx' # formula
        or args.base_file == 'tdf117087-1.docx' # formula
        or args.base_file == 'tdf120431-1.docx' # formula
        or args.base_file == 'tdf121525-1.docx' # formula
        or args.base_file == 'tdf123688-1.docx' # formula
        or args.base_file == 'tdf128205-1.docx' # formula
        or args.base_file == 'tdf130907-2.docx' # formula
        or args.base_file == 'tdf133030-1.docx' # formula
        or args.base_file == 'tdf133030-2.docx' # formula
        or args.base_file == 'tdf133030-5.docx' # formula
        or args.base_file == 'tdf133081-2.docx' # formula
        or args.base_file == 'tdf134466-1.docx' # formula
        or args.base_file == 'tdf136069-2.docx' # formula
        or args.base_file == 'fdo34298-4.docx' # date/time/temp-filename field
        or args.base_file == 'fdo37466-1.docx' # date/time/temp-filename field
        or args.base_file == 'fdo41679-1.docx' # date/time/temp-filename field
        or args.base_file == 'fdo72043-1.docx' # date/time/temp-filename field
        or args.base_file == 'fdo75158-1.docx' # date/time/temp-filename field
        or args.base_file == 'forum-de-3789.docx' # date/time/temp-filename field
        or args.base_file == 'forum-fr-7791.docx' # date/time/temp-filename field
        or args.base_file == 'forum-fr-16249.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-782.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-47433.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-47852.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-48632.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-48637.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-49007.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-49810.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-52537.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-52538.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-59967.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-60957.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-60969.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-62051.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-63802.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-63804.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-63805.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-65215.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-65225.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-65676.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-65888.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-65903.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-68067.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-69314.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-69669.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-69965.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-69973.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-70532.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-71460.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-71579.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-72350.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-72909.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-74184.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-74194.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-75643.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-76198.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-78978.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-79405.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-79599.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-80865.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-82752.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-85324.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-85460.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-86011.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-86412.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-86716.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-87349.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-87624.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-88527.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-88537.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-88561.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-89921.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-90801.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-92011.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-92516.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-92517.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-92613.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-92780.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-95128.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-96314.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-96366.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-96470.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-100516.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-100841.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-103526.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-104394.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-106821.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-108628.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-108634.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-108709.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-109741.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-109772.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-114992.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-118248.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-118249.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-118435.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-119086.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-119088.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-120046.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-122143.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-122319.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-122329.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-124093.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-124831.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-129026.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-133201.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-136404.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-136586.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-136590.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-136592.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-136593.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-138756.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-138770.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-138774.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-de-139447.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-716.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-917.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-1053.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-1239.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-2047.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-2048.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-2456.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-2674.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-2675.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-3083.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-3220.doc' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-3311.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-3363.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-3807.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-4198.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-4208.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-4361.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-5263.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-5490.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-5570.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-6490.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-7385.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-7388.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-8130.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-8949.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-9131.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-10068.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-10209.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-10487.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-10944.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-11777.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-15108.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-16011.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-16592.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-16781.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-16783.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-17282.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-17493.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-17935.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-17939.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en-18071.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en3-17910.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en3-18456.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en3-20834.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en3-25501.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en3-28911.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-63978.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-64016.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-82761.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-82825.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-83519.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-89814.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-168588.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-182337.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-182360.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-182547.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-218615.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-420618.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-426873.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-449246.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-449409.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-449411.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-453122.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-454830.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-570002.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-570003.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-627249.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-672510.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-676107.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-676124.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-676302.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-717469.docx' # date/time/temp-filename field
        or args.base_file == 'forum-mso-en4-83520.docx' # date/time/temp-filename field
        or args.base_file == 'kde248482-1.docx' # date/time/temp-filename field
        or args.base_file == 'moz904327-1.docx' # date/time/temp-filename field
        or args.base_file == 'novell346590-1.docx' # date/time/temp-filename field
        or args.base_file == 'ooo118533-1.docx' # date/time/temp-filename field
        or args.base_file == 'ooo119595-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf82553-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf98761-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf99236-2.docx' # date/time/temp-filename field
        or args.base_file == 'tdf99970-2.docx' # date/time/temp-filename field
        or args.base_file == 'tdf106486-2.docx' # date/time/temp-filename field
        or args.base_file == 'tdf116486-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf117031-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf117187-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf118156-7.docx' # date/time/temp-filename field
        or args.base_file == 'tdf130041-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf132475-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf134592-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf134592-3.docx' # date/time/temp-filename field
        or args.base_file == 'tdf134901-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf137610-1.docx' # date/time/temp-filename field
        or args.base_file == 'tdf137610-3.docx' # date/time/temp-filename field
        or args.base_file == 'fdo33237-2.docx' # effective duplicate
        or args.base_file == 'fdo66368-3.docx' # effective duplicate
        or args.base_file == 'fdo70007-3.docx' # effective duplicate
        or args.base_file == 'fdo70031-1.docx' # effective duplicate
        or args.base_file == 'fdo70032-1.docx' # effective duplicate
        or args.base_file == 'fdo70033-1.docx' # effective duplicate
        or args.base_file == 'fdo72388-1.docx' # effective duplicate
        or args.base_file == 'fdo73550-3.docx' # effective duplicate
        or args.base_file == 'fdo79326-1.docx' # effective duplicate
        or args.base_file == 'fdo83312-6.docx' # effective duplicate
        or args.base_file == 'forum-fr-16236.docx' # effective duplicate
        or args.base_file == 'forum-fr-16238.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-53497.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-54647.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-56734.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-67371.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-71993.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-84324.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-85093.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-86100.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-89919.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-89935.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-94636.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-100786.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-103598.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-126251.docx' # effective duplicate
        or args.base_file == 'forum-mso-de-139701.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-834.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-1052.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-2681.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-2683.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-3221.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-3785.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-3786.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-5125.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-5126.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-8047.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-8049.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-8233.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-8339.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-13356.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-16076.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-17298.docx' # effective duplicate
        or args.base_file == 'forum-mso-en-17995.docx' # effective duplicate
        or args.base_file == 'moz666767-2.docx' # effective duplicate
        or args.base_file == 'moz1007683-1.docx' # effective duplicate
        or args.base_file == 'moz1067887-1.docx' # effective duplicate
        or args.base_file == 'moz1196491-6.docx' # effective duplicate
        or args.base_file == 'novell348471-1.docx' # effective duplicate
        or args.base_file == 'novell492916-1.docx' # effective duplicate
        or args.base_file == 'novell498196-1.docx' # effective duplicate
        or args.base_file == 'novell502097-1.docx' # effective duplicate
        or args.base_file == 'novell568874-1.docx' # effective duplicate
        or args.base_file == 'novell603069-1.docx' # effective duplicate
        or args.base_file == 'novell622455-1.docx' # effective duplicate
        or args.base_file == 'tdf84678-5.docx' # effective duplicate
        or args.base_file == 'tdf89858-3.docx' # effective duplicate
        or args.base_file == 'tdf107391-4.docx' # effective duplicate
        or args.base_file == 'tdf112202-1.docx' # effective duplicate
        or args.base_file == 'tdf112203-1.docx' # effective duplicate
        or args.base_file == 'tdf112342-4.docx' # effective duplicate
        or args.base_file == 'tdf113788-1.docx' # effective duplicate
        or args.base_file == 'tdf117641-1.docx' # effective duplicate
        or args.base_file == 'tdf118169-2.docx' # effective duplicate
        or args.base_file == 'tdf118169-3.docx' # effective duplicate
        or args.base_file == 'tdf118377-1.docx' # effective duplicate
        or args.base_file == 'tdf119351-1.docx' # effective duplicate
        or args.base_file == 'tdf120163-4.docx' # effective duplicate
        or args.base_file == 'rhbz663619-2.docx' # gradient
        or args.base_file == 'forum-mso-en-4201.docx' # dark background
        or args.base_file == 'forum-mso-en3-12825.docx' # dark background
        or args.base_file == 'forum-mso-en4-524519.docx' # dark background
        or args.base_file == 'tdf98555-1.docx' # dark background
        or args.base_file == 'tdf120760-1.docx' # dark background
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

    if args.image_dump == True:
        IMAGE_DUMP_DIR = os.path.join(base_dir, "converted", "image-dump", file_ext[0])
        if not os.path.isdir(IMAGE_DUMP_DIR):
            os.makedirs(IMAGE_DUMP_DIR)

    # ---------------------------------------------------------------------------- Where image magick starts

    # Need to hook in my cpp code pixelbasher into this script
    # to generate the images for comparison.

    if args.image_dump == True:
        print("Image dump directory: ", IMAGE_DUMP_DIR)

    subprocess.run([
        "magick",
        MS_ORIG,
        "-density", "300",
        "-colorspace", "Gray",
        "-define", "bmp:format=bmp4",
        "-background", "white",
        "-alpha", "remove",
        "-alpha", "on",
        IMAGE_DUMP_DIR + "/authoritative-page.bmp"
    ])

    subprocess.run([
        "magick",
        LO_ORIG,
        "-density", "300",
        "-colorspace", "Gray",
        "-define", "bmp:format=bmp4",
        "-background", "white",
        "-alpha", "remove",
        "-alpha", "on",
        IMAGE_DUMP_DIR + "/import-page.bmp"
    ])

    PIXELBASHER_BIN = "./pixelbasher"
    DIFF_OUTPUT_DIR = os.path.join(IMAGE_DUMP_DIR, "diffs/")
    os.makedirs(DIFF_OUTPUT_DIR, exist_ok=True)

    auth_pages = sorted(glob.glob(os.path.join(IMAGE_DUMP_DIR, "authoritative-*.bmp")))
    import_pages = sorted(glob.glob(os.path.join(IMAGE_DUMP_DIR, "import-*.bmp")))

    subprocess.run(
        [PIXELBASHER_BIN] +
        auth_pages +
        import_pages +
        [DIFF_OUTPUT_DIR, "false"],
        check=True
    )

    subprocess.run(
        [PIXELBASHER_BIN] +
        import_pages +
        auth_pages +
        [DIFF_OUTPUT_DIR + "swapped-", "false"],
        check=True
    )

if __name__ == "__main__":
    main()

