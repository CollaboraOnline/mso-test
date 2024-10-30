/*
 * Copyright the mso-test contributors.
 *
 * SPDX-License-Identifier: MPL-2.0
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using excel = Microsoft.Office.Interop.Excel;
using powerPoint = Microsoft.Office.Interop.PowerPoint;
using word = Microsoft.Office.Interop.Word;
using System.Linq;
using System.Text;

namespace mso_test
{
    internal class InteropTest
    {
        const string converterBaseAddress = "https://staging-perf.eu.collaboraonline.com/";

        public static FileStatistics fileStats = new FileStatistics();

        public static word.Application wordApp;
        public static excel.Application excelApp;
        public static powerPoint.Application powerPointApp;

        public enum ApplicationType
        {
            Word,
            Excel,
            PowerPoint
        }

        /*
         * Sometimes app.Quit() does not actually quit the application, or other instances of the application
         * can be open, especially if a previous file errored or was interrupted. The open applications can
         * cause issues when testing the next file. This force quits all instances by process name so that
         * the next test file can be tested on a new instance of the application
         */
        public static void forceQuitAllApplication(ApplicationType application)
        {
            string runningProcess = "";
            if (application == ApplicationType.Word)
            {
                runningProcess = "WINWORD";
            }
            else if (application == ApplicationType.Excel)
            {
                runningProcess = "EXCEL";
            }
            else if (application == ApplicationType.PowerPoint)
            {
                runningProcess = "POWERPNT";
            }
            Process[] processes = Process.GetProcessesByName(runningProcess);
            foreach (Process p in processes)
            {
                try { p.Kill(); } catch { }
            }
        }

        public static bool startApplication(ApplicationType application)
        {
            for (int maxTries = 0; maxTries < 5; maxTries++)
            {
                try
                {
                    if (application == ApplicationType.Word)
                    {
                        wordApp = new word.Application();
                        wordApp.DisplayAlerts = word.WdAlertLevel.wdAlertsNone;
                        wordApp.AutomationSecurity = Microsoft.Office.Core.MsoAutomationSecurity.msoAutomationSecurityForceDisable;
                    }

                    if (application == ApplicationType.Excel)
                    {
                        excelApp = new excel.Application();
                        excelApp.DisplayAlerts = false;
                        excelApp.AutomationSecurity = Microsoft.Office.Core.MsoAutomationSecurity.msoAutomationSecurityForceDisable;
                    }

                    if (application == ApplicationType.PowerPoint)
                    {
                        powerPointApp = new powerPoint.Application();
                        powerPointApp.DisplayAlerts = powerPoint.PpAlertLevel.ppAlertsNone;
                        powerPointApp.AutomationSecurity = Microsoft.Office.Core.MsoAutomationSecurity.msoAutomationSecurityForceDisable;
                    }

                    return true;
                }
                catch (Exception ex)
                {
                    Console.WriteLine("Exception on startup:");
                    Console.WriteLine(ex.ToString());
                    System.Threading.Thread.Sleep(10000);
                    forceQuitAllApplication(application);
                    System.Threading.Thread.Sleep(10000);
                }
            }
            return false;
        }

        public static void quitApplication(ApplicationType application)
        {
            try
            {
                if (application == ApplicationType.Word)
                    wordApp.Quit();

                if (application == ApplicationType.Excel)
                    excelApp.Quit();

                if (application == ApplicationType.PowerPoint)
                    powerPointApp.Quit();
            }
            catch
            {
                if (application == ApplicationType.Word)
                    System.Runtime.InteropServices.Marshal.FinalReleaseComObject(wordApp);

                if (application == ApplicationType.Excel)
                    System.Runtime.InteropServices.Marshal.FinalReleaseComObject(excelApp);

                if (application == ApplicationType.PowerPoint)
                    System.Runtime.InteropServices.Marshal.FinalReleaseComObject(powerPointApp);
            }
            wordApp = null;
            excelApp = null;
            powerPointApp = null;

            forceQuitAllApplication(application);
        }

        public static void restartApplication(ApplicationType application)
        {
            quitApplication(application);
            startApplication(application);
        }

        public static HttpClient coolClient = new HttpClient() { BaseAddress = new Uri(converterBaseAddress) };

        public static SortedSet<string> nativeMSOExtensions = new SortedSet<string>()
        {
            ".docx", ".doc", ".xlsx", ".xls", ".pptx", ".ppt"
        };

        public static SortedSet<string> nativeMSOFileTypes = new SortedSet<string>()
        {
            "docx", "doc", "xlsx", "xls", "pptx", "ppt"
        };

        public static SortedSet<string> allowedExtension = new SortedSet<string>()
        {
            ".odt",
            ".docx",
            ".doc",
            ".ods",
            ".xls",
            ".xlsx",
            ".odp",
            ".ppt",
            ".pptx"
        };

        public static Dictionary<ApplicationType, SortedSet<string>> fileTypesPerApp = new Dictionary<ApplicationType, SortedSet<string>>()
        {
            { ApplicationType.Word, new SortedSet<string> { "doc", "docx", "odt" } },
            { ApplicationType.Excel, new SortedSet<string> { "xls", "xlsx", "ods" } },
            { ApplicationType.PowerPoint, new SortedSet<string> { "ppt", "pptx", "odp" } }
        };

        public static bool skipPass = false;
        public static bool skipFailOpenOriginalFiles = false;
        public static bool skipFailConvertFiles = false;
        public static bool skipFailOpenConvertedFiles = false;
        public static bool skipUnknown = false;

        // args can be word/excel/powerpoint
        // specified appropriate docs will be tested with the specified program
        private static void Main(string[] args)
        {
            ServicePointManager.Expect100Continue = false;

            SortedSet<string> allowedApplications = new SortedSet<string>() { "word", "excel", "powerpoint" };
            if (args.Length <= 0 || !allowedApplications.Contains(args[0]))
            {
                Console.Error.WriteLine("Usage: mso-test.exe <application>");
                Console.Error.WriteLine("    where <application> is \"word\" \"excel\" or \"powerpoint\"");
                Environment.Exit(1);
            }

            if (args.Contains("skipPass"))
            {
                skipPass = true;
            }
            if (args.Contains("skipFail") || args.Contains("skipFailOpenOriginalFiles"))
            {
                skipFailOpenOriginalFiles = true;
            }
            if (args.Contains("skipFail") || args.Contains("skipFailConvertFiles"))
            {
                skipFailConvertFiles = true;
            }
            if (args.Contains("skipFail") || args.Contains("skipFailOpenConvertedFiles"))
            {
                skipFailOpenConvertedFiles = true;
            }
            if (args.Contains("skipUnknown"))
            {
                skipUnknown = true;
            }

            ApplicationType application = ApplicationType.Word;
            if (args[0] == "word")
                application = ApplicationType.Word;
            else if (args[0] == "excel")
                application = ApplicationType.Excel;
            else if (args[0] == "powerpoint")
                application = ApplicationType.PowerPoint;

            forceQuitAllApplication(application);
            startApplication(application);

            TestDownloadedFiles(application);

            PresentResults(args[0]);

            quitApplication(application);
        }

        public static (bool, string) OpenFile(ApplicationType application, string fileName)
        {
            if (application == ApplicationType.Word)
            {
                return OpenWordDoc(fileName);
            } 
            else if (application == ApplicationType.Excel)
            {
                return OpenExcelWorkbook(fileName);
            }
            else if (application == ApplicationType.PowerPoint)
            {
                return OpenPowerPointPresentation(fileName);
            }
            else
            {
                return (false, "");
            }
        }

        public static async void TestFile(ApplicationType application, FileInfo file, string fileType, string convertTo)
        {
            Console.WriteLine("\nStarting test for " + file.Name + " at " + DateTime.Now.ToString("s"));
            Stopwatch watch = new System.Diagnostics.Stopwatch();
            // Count total elapsed time for current file (in parts: open, test service, convert, open converted)
            // Store convert and open converted times for total stats
            long elapsedTime = 0;
            watch.Start();
            TimeSpan openTimeout = TimeSpan.FromMilliseconds(10000);
            TimeSpan convertTimeout = TimeSpan.FromMilliseconds(60000);

            // Open original file, only if it's a native format, and not known if it can be opened in MSO
            if (nativeMSOExtensions.Contains(file.Extension))
            {
                if (fileStats.isFailOpenOriginalFile(fileType, file.Name))
                {
                    Console.WriteLine($"Fail; Previously failed opening original file");
                    return;
                }

                // Use the PDF as sufficient proof that the original file can be opened.
                FileInfo PDFfile = new FileInfo(file.FullName + "_mso.pdf");
                if (!PDFfile.Exists)
                {
                    Task<(bool, string)> OpenOriginalFileTask = Task.Run(() => OpenFile(application, file.FullName));
                    if (!OpenOriginalFileTask.Wait(openTimeout))
                    {
                        Console.WriteLine($"Fail opening original file timeout; {file.Name}; {watch.ElapsedMilliseconds} ms; Timed out after {openTimeout.TotalMilliseconds} ms");
                        restartApplication(application);
                        await OpenOriginalFileTask;
                        fileStats.addToFailOpenOriginalFiles(fileType, file.Name);
                        return;
                    }
                    else if (!OpenOriginalFileTask.Result.Item1)
                    {
                        Console.WriteLine($"Fail opening original file; {file.Name}; {watch.ElapsedMilliseconds} ms; {OpenOriginalFileTask.Result.Item2}");
                        restartApplication(application);
                        fileStats.addToFailOpenOriginalFiles(fileType, file.Name);
                        return;
                    }

                    fileStats.addToPassOpenOriginalFiles(fileType, file.Name);
                }
            }
            else
                fileStats.addToPassOpenOriginalFiles(fileType, file.Name);

            // Test convert service
            elapsedTime += watch.ElapsedMilliseconds;
            watch.Restart();
            bool serverStatus = false;
            int count = 0;
            while (!serverStatus)
            {
                if (count >= 10)
                {
                    Console.WriteLine($"Fail converting file timeout; {file.Name}; {watch.ElapsedMilliseconds} ms; Server not available after 10 attempts");
                    return;
                }
                Task<(bool, string)> TestConvertServiceTask = Task.Run(() => TestConvertService());
                if (!TestConvertServiceTask.Wait(convertTimeout))
                {
                    Console.WriteLine($"Server not available timeout. Sleeping for 10s. Timed out after {convertTimeout.TotalMilliseconds} ms");
                    coolClient.CancelPendingRequests();
                    await TestConvertServiceTask;
                    System.Threading.Thread.Sleep(10000);
                }
                else if (!TestConvertServiceTask.Result.Item1)
                {
                    Console.WriteLine($"Server not available. Sleeping for 10s. {TestConvertServiceTask.Result.Item2}");
                    System.Threading.Thread.Sleep(10000);
                }
                else if (TestConvertServiceTask.Result.Item1)
                {
                    serverStatus = true;
                }
                count++;
            }

            // Round-trip file to MSO format
            elapsedTime += watch.ElapsedMilliseconds;
            watch.Restart();
            string convertedFileName = @"converted\" + fileType + @"\" + Path.GetFileNameWithoutExtension(file.Name) + "." + convertTo;
            Task <(bool, string, string)> ConvertTask = Task.Run(() => ConvertFile(application, file.FullName, convertedFileName, fileType, convertTo));
            if (!ConvertTask.Wait(convertTimeout))
            {
                Console.WriteLine($"Fail converting file timeout; {file.Name}; {watch.ElapsedMilliseconds} ms; Timed out after {convertTimeout.TotalMilliseconds} ms");
                coolClient.CancelPendingRequests();
                await ConvertTask;
                fileStats.addTimeToConvert(fileType, watch.ElapsedMilliseconds);
                fileStats.addToFailConvertFiles(fileType, file.Name);
                return;
            }
            else if (!ConvertTask.Result.Item1)
            {
                Console.WriteLine($"Fail converting file; {file.Name}; {watch.ElapsedMilliseconds} ms; {ConvertTask.Result.Item3}");
                fileStats.addTimeToConvert(fileType, watch.ElapsedMilliseconds);
                fileStats.addToFailConvertFiles(fileType, file.Name);
                return;
            }
            fileStats.addTimeToConvert(fileType, watch.ElapsedMilliseconds);

            // Make a PDF of the file (not using the round-tripped document, but using the originally provided document)
            elapsedTime += watch.ElapsedMilliseconds;
            watch.Restart();
            convertedFileName = @"converted\" + fileType + @"\" + Path.GetFileNameWithoutExtension(file.Name) + ".pdf";
            FileInfo PDFexport = new FileInfo(convertedFileName);
            if (!PDFexport.Exists)
            {
                Task<(bool, string, string)> PDFTask = Task.Run(() => ConvertFile(application, file.FullName, convertedFileName, fileType, "pdf"));
                if (!PDFTask.Wait(convertTimeout))
                {
                    Console.WriteLine($"Fail PDF export file timeout; {file.Name}; {watch.ElapsedMilliseconds} ms; Timed out after {convertTimeout.TotalMilliseconds} ms");
                    coolClient.CancelPendingRequests();
                    await PDFTask;
                    return;
                }
                else if (!PDFTask.Result.Item1)
                {
                    Console.WriteLine($"Fail creating PDF export; {file.Name}; {watch.ElapsedMilliseconds} ms; {ConvertTask.Result.Item3}");
                    return;
                }
            }
            else
                Console.WriteLine("\nDEBUG: PDF already exists for " + PDFexport.FullName);

            // Open converted file
            elapsedTime += watch.ElapsedMilliseconds;
            watch.Restart();
            Task<(bool, string)> OpenConvertedFileTask = Task.Run(() => OpenFile(application, ConvertTask.Result.Item2));
            if (!OpenConvertedFileTask.Wait(openTimeout))
            {
                Console.WriteLine($"Fail opening converted file timeout; {file.Name}; {watch.ElapsedMilliseconds} ms; Timed out after {openTimeout.TotalMilliseconds} ms");
                restartApplication(application);
                await OpenConvertedFileTask;
                fileStats.addTimeToOpenConvertedFiles(fileType, watch.ElapsedMilliseconds);
                fileStats.addToFailOpenConvertedFiles(fileType, file.Name);
                return;
            }
            else if (!OpenConvertedFileTask.Result.Item1)
            {
                Console.WriteLine($"Fail opening converted file; {file.Name}; {watch.ElapsedMilliseconds} ms; {OpenConvertedFileTask.Result.Item2}");
                restartApplication(application);
                fileStats.addTimeToOpenConvertedFiles(fileType, watch.ElapsedMilliseconds);
                fileStats.addToFailOpenConvertedFiles(fileType, file.Name);
                return;
            }
            elapsedTime += watch.ElapsedMilliseconds;
            fileStats.addTimeToOpenConvertedFiles(fileType, watch.ElapsedMilliseconds);

            // Passed
            Console.WriteLine($"Pass; {file.Name}; {elapsedTime} ms");
        }

        public static void TestDirectory(ApplicationType application, DirectoryInfo dir, string convertTo)
        {
            Console.WriteLine("\n\nStarting test directory " + dir.Name);
            string fileType = dir.Name;
            Debug.Assert(dir.Parent != null);
            fileStats.initOpenOriginalFileLists(dir.Parent, fileType);

            FileInfo[] fileInfos = dir.GetFiles();
            foreach (FileInfo file in fileInfos)
            {
                if (!allowedExtension.Contains(file.Extension))
                {
                    Console.WriteLine("\nSkipping non test file " + file.Name);
                    continue;
                }
/*                else if (fileStats.isFailOpenOriginalFile(fileType, file.Name))
                {
                    // no point in trying converting a file that originally failed to open
                    Console.WriteLine("\nSkipping file that fails to open " + file.Name);
                    continue;
                }
                else if (fileStats.isPassOpenOriginalFile(fileType, file.Name))
                {
                    if (skipPass)
                    {
                        Console.WriteLine("\nSkipping passing file " + file.Name);
                        continue;
                    }
                }
                else if (failConvertFiles.Contains(file.Name))
                {
                    if (skipFailConvertFiles)
                    {
                        Console.WriteLine("\nSkipping file that fails to convert " + file.Name);
                        continue;
                    }
                }
                else if (failOpenConvertedFiles.Contains(file.Name))
                {
                    if (skipFailOpenConvertedFiles)
                    {
                        Console.WriteLine("\nSkipping file that fails to open after being converted " + file.Name);
                        continue;
                    }
                }*/
                else if (skipUnknown)
                {
                    Console.WriteLine("\nSkipping unknown file " + file.Name);
                    continue;
                }

                TestFile(application, file, fileType, convertTo);

            }

            fileStats.saveOpenOriginalFileLists(dir.Parent, fileType);

        }

        public static void TestDownloadedFiles(ApplicationType application)
        {
            DirectoryInfo downloadedDirInfo = new DirectoryInfo(@"download");
            if (!downloadedDirInfo.Exists)
            {
                Console.Error.WriteLine("Download directory does not exist: " + downloadedDirInfo.FullName);
                return;
            }
            DirectoryInfo[] downloadedDictInfo = downloadedDirInfo.GetDirectories();
            foreach (DirectoryInfo dict in downloadedDictInfo)
            {
                if (application == ApplicationType.Word && fileTypesPerApp[application].Contains(dict.Name))
                {
                    TestDirectory(application, dict, "docx");
                }
                else if (application == ApplicationType.Excel && fileTypesPerApp[application].Contains(dict.Name))
                {
                    TestDirectory(application, dict, "xlsx");
                }
                else if (application == ApplicationType.PowerPoint && fileTypesPerApp[application].Contains(dict.Name))
                {
                    TestDirectory(application, dict, "pptx");
                }
            }
        }

        /*
         * Returns: (success, converted file name, error message)
         */
        public static async Task<(bool, string, string)> ConvertFile(ApplicationType application, string fullFileName, string convertedFileName, string fileType, string convertTo)
        {
            using (var request = new HttpRequestMessage(new HttpMethod("POST"), coolClient.BaseAddress + "cool/convert-to/" + convertTo))
            {
                var multipartContent = new MultipartFormDataContent
                {
                    { new ByteArrayContent(File.ReadAllBytes(fullFileName)), "data", Path.GetFileName(fullFileName) }
                };
                request.Content = multipartContent;
                try
                {
                    using (var response = await coolClient.SendAsync(request))
                    {
                        if (!response.IsSuccessStatusCode)
                        {
                            return (false, "", "HTTP StatusCode: " + response.StatusCode);
                        }
                        Directory.CreateDirectory(Path.GetDirectoryName(convertedFileName));
                        using (FileStream fs = File.Open(convertedFileName, FileMode.Create))
                        {
                            await response.Content.CopyToAsync(fs);
                            return (true, fs.Name, "");
                        }
                    }
                }
                catch (Exception ex)
                {
                    return (false, "", "Exception during convert: " + ex.Message);
                }
            }
        }

        /*
         * Returns: (success, error message)
         */
        public static async Task<(bool, string)> TestConvertService()
        {
            using (var request = new HttpRequestMessage(new HttpMethod("GET"), coolClient.BaseAddress + "hosting/capabilities"))
            {
                try
                {
                    using (var response = await coolClient.SendAsync(request))
                    {
                        if (!response.IsSuccessStatusCode)
                        {
                            return (false, "HTTP StatusCode: " + response.StatusCode);
                        }
                    }
                }
                catch (Exception ex)
                {
                    return (false, "Exception during convert: " + ex.Message);
                }
            }
            return (true, "");
        }

        private static (bool, string) OpenWordDoc(string fileName)
        {
            word.Document doc = null;
            bool testResult = true;
            string errorMessage = "";

            try
            {
                doc = wordApp.Documents.OpenNoRepairDialog(fileName, ReadOnly: true, Visible: false, PasswordDocument: "'");
            }
            catch (COMException e)
            {
                testResult = false;
                errorMessage = e.Message;
            }
            if (doc != null)
            {
                try
                {
                    FileInfo PDFexport = new FileInfo(fileName + @"_mso.pdf");
                    if (!PDFexport.Exists)
                        doc.ExportAsFixedFormat(PDFexport.FullName, word.WdExportFormat.wdExportFormatPDF);
                    else
                        Console.WriteLine("\nDEBUG: PDF already exists for " + PDFexport.FullName);
                    doc.Close(SaveChanges: false);
                }
                catch
                {
                    System.Runtime.InteropServices.Marshal.FinalReleaseComObject(doc);
                    doc = null;
                }
            }

            return (testResult, errorMessage);
        }

        private static (bool, string) OpenExcelWorkbook(string fileName)
        {
            excel.Workbook wb = null;
            bool testResult = true;
            string errorMessage = "";

            try
            {
                wb = excelApp.Workbooks.Open(fileName, ReadOnly: true, Password: "'", UpdateLinks: false);
            }
            catch (COMException e)
            {
                testResult = false;
                errorMessage = e.Message;
            }

            if (wb != null)
            {
                try
                {
                    FileInfo PDFexport = new FileInfo(fileName + @"_mso.pdf");
                    if (!PDFexport.Exists)
                        wb.ExportAsFixedFormat(excel.XlFixedFormatType.xlTypePDF, Filename: PDFexport.FullName);
                    else
                        Console.WriteLine("\nDEBUG: PDF already exists for " + PDFexport.FullName);
                    wb.Close(SaveChanges: false);
                }
                catch
                {
                    System.Runtime.InteropServices.Marshal.FinalReleaseComObject(wb);
                    wb = null;
                }
            }

            return (testResult, errorMessage);
        }

        private static (bool, string) OpenPowerPointPresentation(string fileName)
        {
            powerPoint.Presentation presentation = null;
            bool testResult = true;
            string errorMessage = "";

            try
            {
                presentation = powerPointApp.Presentations.Open(fileName, ReadOnly: Microsoft.Office.Core.MsoTriState.msoTrue, WithWindow: Microsoft.Office.Core.MsoTriState.msoFalse);
            }
            catch (COMException e)
            {
                testResult = false;
                errorMessage = e.Message;
            }

            if (presentation != null)
            {
                try
                {
                    FileInfo PDFexport = new FileInfo(fileName + @"_mso.pdf");
                    if (!PDFexport.Exists)
                        presentation.ExportAsFixedFormat2(PDFexport.FullName, powerPoint.PpFixedFormatType.ppFixedFormatTypePDF);
                    else
                        Console.WriteLine("\nDEBUG: PDF already exists for " + PDFexport.FullName);
                    presentation.Close();
                }
                catch
                {
                    System.Runtime.InteropServices.Marshal.FinalReleaseComObject(presentation);
                    presentation = null;
                }
            }

            return (testResult, errorMessage);
        }

        public static void PresentResults(string applicationName)
        {
            List<string> customSortedFileTypes = new List<string>()
            {
                "odt", "doc", "docx", "ods", "xls", "xlsx", "odp", "ppt", "pptx"
            };
            System.Globalization.CultureInfo culture = new System.Globalization.CultureInfo("en-US");
            StringBuilder csv = new StringBuilder();
            csv.AppendLine("Format,Total,Open failed,Total tested,Conversion failed,,Open failed after conversion");
            foreach (string fileType in customSortedFileTypes)
            {
                int failOpenFiles = fileStats.getFailOpenOriginalFilesNo(fileType);
                // For ODF all files are added to PassOpenOriginalFiles, to have an accurate total
                int passOpenFiles = fileStats.getPassOpenOriginalFilesNo(fileType);
                int totalFiles = failOpenFiles + passOpenFiles;
                // If no total files, it means nothing was tested
                if (totalFiles == 0)
                    continue;

                int failConvertFiles = fileStats.getFailConvertFilesNo(fileType);
                int failOpenConvertedFiles = fileStats.getFailOpenConvertedFilesNo(fileType);
                // passOpenFiles are the total actually tested files (=100%)
                string failConvertFilesPercentage = String.Format(culture, "{0:0.0}%", Convert.ToDouble(failConvertFiles) / passOpenFiles * 100);
                string failOpenConvertedFilesPercentage = String.Format(culture, "{0:0.0}%", Convert.ToDouble(failOpenConvertedFiles) / passOpenFiles * 100);

                csv.AppendLine(
                    fileType.ToUpper() + "," + totalFiles + "," + (nativeMSOFileTypes.Contains(fileType) ? failOpenFiles.ToString() : "") + "," + passOpenFiles + "," +
                    failConvertFiles + "," + failConvertFilesPercentage + "," + failOpenConvertedFiles + "," + failOpenConvertedFilesPercentage);
                
                Console.WriteLine("\nFormat:                  " + fileType.ToUpper());
                Console.WriteLine("Total:                   " + totalFiles);
                Console.WriteLine("Open failed:             " + (nativeMSOFileTypes.Contains(fileType) ? failOpenFiles.ToString() : "-"));
                Console.WriteLine("Total tested:            " + passOpenFiles);
                Console.WriteLine("Conversion failure:      " + failConvertFiles + " (" + failConvertFilesPercentage + ")");
                //Console.WriteLine("Crash in MSO:            ");
                Console.WriteLine("Open failed after conv.: " + failOpenConvertedFiles + " (" + failOpenConvertedFilesPercentage + ")");
                Console.WriteLine("Time spent converting files         : " + fileStats.getTimeToConvert(fileType) / 1000 + " seconds");
                Console.WriteLine("Time spent opening files after conv.: " + fileStats.getTimeToOpenConvertedFiles(fileType) / 1000 + " seconds");

                fileStats.saveFailedFileLists(new DirectoryInfo(@"download"), fileType);
            }

            File.WriteAllText(@"download\result-" + applicationName + ".csv", csv.ToString());
        }
    }
}
