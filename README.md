# mso-test
**A tool that round-trips document files with Collabora Online and tests their integrity in MS Office.**

The Document Foundation collected a large amount of document files from public bug trackers and from 
other public sources. They do regular systematic load and save testing against this corpus with LibreOffice. 

This tool has the goal to automate testing of Collabora Online's round-trip export filters to ensure that 
the resulting documents also load without warnings into Microsoft Office.

The solution will load the documents in MS Office, and on success, test them with Collabora Online 
(load/save). Then we try to open these in MS Office again, and see if they open succesfully. If not, 
we will log the error for future investigation.
