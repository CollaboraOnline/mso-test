# mso-test
**A tool that round-trips document files with Collabora Online and tests their integrity in MS Office.**

The Document Foundation collected a large amount of document files from public bug trackers and from
other public sources. They do regular systematic load and save testing against this corpus with LibreOffice.

This tool has the goal to automate testing of Collabora Online's round-trip export filters to ensure that
the resulting documents also load without warnings into Microsoft Office.

The solution will load the documents in MS Office, and on success, test them with Collabora Online
(load/save). Then we try to open these in MS Office again, and see if they open succesfully. If not,
we will log the error for future investigation.


## Development bits
To build the projects simply load the solution in Visual Studio and build the solution. Solution uses .NETFramework 4.8.

In addition, to download files on Windows, Cygwin is required to run shell scripts.
+ Setup/
    + contains files to create a windows installer for the app
+ file-handling/
    + Provides the functionality to reset/copy failed files

## User bits
 This application is a command line app which takes command line arguments to perform any operation. To make usage easy different batch files are provided

 + runExcelTest.bat: runs tests for Excel files
 + runWordTest.bat: runs tests for Word files
 + runPowerpointTest.bat: runs tests for Powerpoint
 + runMSOTest.bat: runs tests for all Excel, Word, and Powerpoint
 + downloadExcelData.sh: Downloads Excel test files
 + downloadWordData.sh: Downloads Word test files
 + downloadPowerpointData.sh: Downloads Powerpoint test files
 + downloadTestData.sh: Downloads Excel, Word, Powerpoint all test files
 + getOriginalExcelFile.bat: Finds and copies original source Excel files which failed round trip test into a separate directory
 + getOriginalWordFile.bat: Finds and copies original source Word files which failed round trip test into a separate directory
 + getOriginalPowerpointFile.bat: Finds and copies original source Powerpoint files which failed round trip test into a separate directory
 + resetFailed.bat: resets all the files which were marked as failed
