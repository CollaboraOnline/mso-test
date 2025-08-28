# Autobibisect tool for mso-test

`mso-test` can be used to find interoperability validity issues in office files after saving them with Collabora Online or desktop LibreOffice. With multiple runs over multiple versions, the difference in results will indicate likely regressions, or issues that got resolved over time.

This Python script allows running the bibisect process using `mso-test` with minimal setup to find the responsible commits.

Bibisect stands for _binary bisect_, and it is a procedure to identify commits causing regressions by running `git bisect` in a repository of LibreOffice core binaries, typically each commit of binaries representing a single commit in the source repository. For more information on bibisecting, read the [QA/Bibisect](https://wiki.documentfoundation.org/QA/Bibisect) LibreOffice wiki page.

In a similar fashion, reverse bibisecting is the same process, only with the opposite goal: finding the commit when an issue got fixed.

### Prerequisites

Since **mso-test** needs Windows, this script can only run on Windows, and is prepared to run within Cygwin.

Set up **mso-test** tool, run it on a set of test files with two different builds of Collabora Online/LibreOffice, and from the results identify a subset that started failing (or got fixed) between those builds.

### Step-by-step

* Copy `config-sample.json` next to the script to `config.json`
* Edit `config.json`
  * `BibisectRepoDir` — Location of `soffice.exe` in the respective bibisect repository, it **must** be in sync with `mso-test`'s `DesktopConverterLocation` item, and also set `OnlineConverter` to `false` there
  * `Reverse` — Indicates whether it is a regular or reverse bibisect, usually leave as `false`
  * `MsoTestDir` — Location of `mso-test.exe`
  * `TestFileBaseDir` — Directory containing the test files for `mso-test` within subdirectories per each format, usually this path ends with `download`
  * `RangeFrom` — Starting source commit of the bibisect range (can also be `oldest`, referring to the first commit in the bibisect repo)
  * `RangeTo` — Ending source commit of the bibisect range (can also be `main`/`master`, referring to the latest commit in the bibisect repo)
  * `FileListFullName` — Full path of the newline-separated list of files to perform the bibisect on
  * `ResultFileName` — Name of the result text file, will be saved into `TestFileBaseDir`
* Execute autobibisect script
* Review result file

### Results

The result file contains:
* List of files already failing after conversion at the start of the range
* List of files still succeeding after conversion at the end of the range
* Series of commits and corresponding list of affected files that started failing after conversion at that commit
* List of files failing to convert at any point

When reverse bibisecting, the same lists are presented with opposite interpretation: file already succeeding at the beginning of range, files still failing at the end of the range, and commits where subsets of files started succeeding.

### Considerations

Files causing conversion failures at any point are disregarded for now due to potentially flaky results, eg. conversion is terminated if it is taking too long. Reressions involving conversion failures need to be investigated manually.

