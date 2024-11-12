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
using System.Collections.Concurrent;
using System.Text.Json.Nodes;
using System.Runtime.CompilerServices;

namespace mso_test
{
    internal enum ApplicationType
    {
        Word,
        Excel,
        PowerPoint,
        Undefined
    }
    internal class Configuration
    {
        // Application (= formats) to test
        public ApplicationType Application = ApplicationType.Undefined;
        // Perform conversion using online service
        public bool OnlineConverter = true;
        // Launch this amount of online converter tasks in parallel
        public int OnlineConverterTasks = 1;
        // Base URL for online converter
        public string OnlineConverterBaseURL = "https://localhost:9980";
        // Launch LO converter from this location, if OnlineConverter is set to false
        public string DesktopConverterLocation = "";
        // Generate PDFs
        public bool GeneratePDF = false;
        // Test every Nth file (for separating multiple test instances; application must be the same)
        public int FilePeriod = 1;
        // Offset for period, eg. period 5, offset 3 will test every 5th file starting from the 4th (1st + 3)
        public int FileOffset = 0;
    }
    internal class InteropTest
    {
        static TimeSpan openTimeout = TimeSpan.FromMilliseconds(30000);
        static TimeSpan convertTimeout = TimeSpan.FromMilliseconds(60000);

        Configuration config = new Configuration();
        FileStatistics fileStats = new FileStatistics();

        word.Application wordApp;
        excel.Application excelApp;
        powerPoint.Application powerPointApp;

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

        public bool startApplication(ApplicationType application)
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

        public void quitApplication(ApplicationType application)
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

        public void restartApplication(ApplicationType application)
        {
            quitApplication(application);
            startApplication(application);
        }

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

        public static Dictionary<ApplicationType, string> targetFormatPerApp = new Dictionary<ApplicationType, string>()
        {
            { ApplicationType.Word, "docx" },
            { ApplicationType.Excel, "xlsx"},
            { ApplicationType.PowerPoint, "pptx" }
        };

        public static Dictionary<ApplicationType, string> applicationNames = new Dictionary<ApplicationType, string>()
        {
            { ApplicationType.Word, "`word" },
            { ApplicationType.Excel, "excel"},
            { ApplicationType.PowerPoint, "powerpoint" }
        };

        // args can be word/excel/powerpoint
        // specified appropriate docs will be tested with the specified program
        private static void Main(string[] args)
        {
            ServicePointManager.Expect100Continue = false;

            InteropTest test = new InteropTest();

            test.ReadConfig();

            if (args.Length > 0)
            {
                if (args[0] == "word")
                    test.config.Application = ApplicationType.Word;
                else if (args[0] == "excel")
                    test.config.Application = ApplicationType.Excel;
                else if (args[0] == "powerpoint")
                    test.config.Application = ApplicationType.PowerPoint;
            }

            SortedSet<string> allowedApplications = new SortedSet<string>() { "word", "excel", "powerpoint" };
            if (test.config.Application == ApplicationType.Undefined && (args.Length <= 0 || !allowedApplications.Contains(args[0])))
            {
                Console.Error.WriteLine("Usage: mso-test.exe <application>");
                Console.Error.WriteLine("    where <application> is \"word\" \"excel\" or \"powerpoint\"");
                Environment.Exit(1);
            }

            if (!test.config.OnlineConverter)
            {
                string desktopConverter = Path.Combine(test.config.DesktopConverterLocation, "soffice.exe");
                if (!File.Exists(desktopConverter))
                {
                    Console.Error.WriteLine("ERROR: Desktop converter (soffice.exe) not found in location " + test.config.DesktopConverterLocation);
                    Environment.Exit(1);
                }
            }

            forceQuitAllApplication(test.config.Application);
            test.startApplication(test.config.Application);

            Stopwatch watch = new System.Diagnostics.Stopwatch();
            watch.Start();
            test.TestDownloadedFiles(test.config.Application);
            watch.Stop();
            test.PresentResults(test.config.Application);
            Console.WriteLine("\nFinished in " + watch.ElapsedMilliseconds/1000 + " seconds");

            test.quitApplication(test.config.Application);
        }

        public void ReadConfig()
        {
            const string configFile = "config.json";
            if (!File.Exists(configFile))
                return;

            using (StreamReader reader = new StreamReader(configFile, new System.Text.UTF8Encoding()))
            {
                JsonNode configJson = JsonNode.Parse(reader.ReadToEnd());
                if (configJson["Application"] != null)
                {
                    string application = configJson["Application"].GetValue<string>();
                    if (application == "word")
                        config.Application = ApplicationType.Word;
                    else if (application == "excel")
                        config.Application = ApplicationType.Excel;
                    else if (application == "powerpoint")
                        config.Application = ApplicationType.PowerPoint;
                }
                if (configJson["OnlineConverter"] != null)
                    config.OnlineConverter = configJson["OnlineConverter"].GetValue<bool>();
                if (configJson["OnlineConverterBaseURL"] != null)
                    config.OnlineConverterBaseURL = configJson["OnlineConverterBaseURL"].GetValue<string>();
                if (configJson["OnlineConverterTasks"] != null)
                    config.OnlineConverterTasks = configJson["OnlineConverterTasks"].GetValue<int>();
                if (configJson["DesktopConverterLocation"] != null)
                    config.DesktopConverterLocation = configJson["DesktopConverterLocation"].GetValue<string>();
                if (configJson["GeneratePDF"] != null)
                    config.GeneratePDF = configJson["GeneratePDF"].GetValue<bool>();
                if (configJson["FilePeriod"] != null)
                    config.FilePeriod = configJson["FilePeriod"].GetValue<int>();
                if (configJson["FileOffset"] != null)
                    config.FileOffset = configJson["FileOffset"].GetValue<int>();
            }
        }
        public (bool, string) OpenFile(ApplicationType application, string fileName, bool generatePDF)
        {
            if (application == ApplicationType.Word)
            {
                return OpenWordDoc(fileName, generatePDF);
            } 
            else if (application == ApplicationType.Excel)
            {
                return OpenExcelWorkbook(fileName, generatePDF);
            }
            else if (application == ApplicationType.PowerPoint)
            {
                return OpenPowerPointPresentation(fileName, generatePDF);
            }
            else
            {
                return (false, "");
            }
        }
        public void TestDownloadedFiles(ApplicationType application)
        {
            DirectoryInfo downloadedDirInfo = new DirectoryInfo(@"download");
            if (!downloadedDirInfo.Exists)
            {
                Console.Error.WriteLine("Download directory does not exist: " + downloadedDirInfo.FullName);
                return;
            }

            ConcurrentQueue<(string, string)> filesToConvert = new ConcurrentQueue<(string, string)>();
            ConcurrentQueue<(string, string)> convertedFilesToTest = new ConcurrentQueue<(string, string)>();
            bool testOriginalFilesCompleted = false;
            bool conversionCompleted = false;

            Task originalTest = Task.Run(() =>
            {
                Stopwatch watch = new System.Diagnostics.Stopwatch();

                string filesToSkipName = Path.Combine(downloadedDirInfo.FullName, "filestoskip.txt");
                if (File.Exists(filesToSkipName))
                    Console.WriteLine(@"The file download\filestoskip.txt exists, files listed there will be skipped.");
                HashSet<string> filesToSkip = FileStatistics.ReadFileToSet(filesToSkipName);
                string filesToTestName = Path.Combine(downloadedDirInfo.FullName, "filestotest.txt");
                if (File.Exists(filesToTestName))
                    Console.WriteLine(@"The file download\filestotest.txt exists, and will be used to limit the files tested.");
                HashSet<string> filesToTest = FileStatistics.ReadFileToSet(filesToTestName);
                Console.WriteLine();

                var downloadedDictInfo = downloadedDirInfo.GetDirectories().OrderBy(d => d.Name);
                int fileNo = 0;
                foreach (DirectoryInfo dict in downloadedDictInfo)
                {
                    if (!fileTypesPerApp[application].Contains(dict.Name))
                        continue;
                    string fileType = dict.Name;

                    fileStats.initOpenOriginalFileLists(downloadedDirInfo, fileType);

                    var fileInfos = dict.GetFiles().OrderBy(f => f.Name);
                    foreach (FileInfo file in fileInfos)
                    {
                        fileNo++;
                        // Only test every Nth file, as configured
                        if (((fileNo - 1) - config.FileOffset) % config.FilePeriod != 0)
                            continue;

                        if (filesToSkip.Contains(file.Name))
                            continue;
                        if (filesToTest.Count > 0 && !filesToTest.Contains(file.Name))
                            continue;

                        if (!allowedExtension.Contains(file.Extension))
                        {
                            Console.WriteLine($"{file.Name} - Skipping non test file");
                            continue;
                        }

                        // Open original file, only if it's a native format, and not known if it can be opened in MSO
                        if (nativeMSOExtensions.Contains(file.Extension))
                        {
                            if (fileStats.isFailOpenOriginalFile(fileType, file.Name))
                            {
                                Console.WriteLine($"{file.Name} - Fail; Previously failed opening original file");
                                continue;
                            }

                            // Use the PDF as sufficient proof that the original file can be opened.
                            FileInfo PDFfile = new FileInfo(file.FullName + "_mso.pdf");
                            bool testOriginal = (!config.GeneratePDF && !fileStats.isPassOpenOriginalFile(fileType, file.Name)) || (config.GeneratePDF && !PDFfile.Exists);
                            if (testOriginal)
                            {
                                Console.WriteLine($"{file.Name} - Opening original file at {DateTime.Now.ToString("s")}");
                                watch.Restart();
                                Task<(bool, string)> OpenOriginalFileTask = Task.Run(() => OpenFile(application, file.FullName, config.GeneratePDF));
                                if (!OpenOriginalFileTask.Wait(openTimeout))
                                {
                                    Console.WriteLine($"Fail opening original file timeout; {file.Name}; {watch.ElapsedMilliseconds} ms; Timed out after {openTimeout.TotalMilliseconds} ms");
                                    restartApplication(application);
                                    OpenOriginalFileTask.Wait();
                                    fileStats.addTimeToOpenOriginalFiles(fileType, watch.ElapsedMilliseconds);
                                    fileStats.addToFailOpenOriginalFiles(fileType, file.Name);
                                    fileStats.incrCurrFailOpenOriginalFilesNo(fileType);
                                    continue;
                                }
                                else if (!OpenOriginalFileTask.Result.Item1)
                                {
                                    Console.WriteLine($"Fail opening original file; {file.Name}; {watch.ElapsedMilliseconds} ms; {OpenOriginalFileTask.Result.Item2}");
                                    restartApplication(application);
                                    fileStats.addTimeToOpenOriginalFiles(fileType, watch.ElapsedMilliseconds);
                                    fileStats.addToFailOpenOriginalFiles(fileType, file.Name);
                                    fileStats.incrCurrFailOpenOriginalFilesNo(fileType);
                                    continue;
                                }

                                filesToConvert.Enqueue((fileType, file.Name));
                                fileStats.addTimeToOpenOriginalFiles(fileType, watch.ElapsedMilliseconds);
                                fileStats.incrCurrPassOpenOriginalFilesNo(fileType);
                                fileStats.addToPassOpenOriginalFiles(fileType, file.Name);
                            }
                            else
                            {
                                Console.WriteLine($"{file.Name} - Original file could be opened previously, skipping this step");
                                filesToConvert.Enqueue((fileType, file.Name));
                                fileStats.incrCurrPassOpenOriginalFilesNo(fileType);
                            }
                        }
                        else
                        {
                            Console.WriteLine($"{file.Name} - Original is not native MSO file, skipping testing whether it can be opened in MSO");
                            filesToConvert.Enqueue((fileType, file.Name));
                            fileStats.incrCurrPassOpenOriginalFilesNo(fileType);
                            fileStats.addToPassOpenOriginalFiles(fileType, file.Name);
                        }
                    }
                    if (nativeMSOFileTypes.Contains(fileType))
                        fileStats.saveOpenOriginalFileLists(downloadedDirInfo, fileType);
                }
            });

            List<Task> convertFiles = new List<Task>();
            if (config.OnlineConverter)
            {
                for (int i = 0; i < config.OnlineConverterTasks; i++)
                {
                    convertFiles.Add(Task.Run(() =>
                    {
                        HttpClient coolClient = new HttpClient() { BaseAddress = new Uri(config.OnlineConverterBaseURL) };
                        Stopwatch watch = new System.Diagnostics.Stopwatch();
                        while (true)
                        {
                            bool success = filesToConvert.TryDequeue(out var element);
                            if (testOriginalFilesCompleted && !success)
                                break;
                            if (!success)
                            {
                                System.Threading.Thread.Sleep(2000);
                                continue;
                            }
                            (string fileType, string fileName) = element;
                            string fullFileName = @"download\" + fileType + @"\" + fileName;
                            string convertTo = targetFormatPerApp[application];
                            string convertedFileName = @"converted\" + fileType + @"\" + Path.GetFileNameWithoutExtension(fileName) + "." + convertTo;
                            Console.WriteLine($"{fileName} - Converting file");

                            // Round-trip file to MSO format
                            watch.Restart();
                            Task<(bool, string, string)> ConvertTask = Task.Run(() => ConvertFile(coolClient, application, fullFileName, convertedFileName, fileType, convertTo));
                            if (!ConvertTask.Wait(convertTimeout))
                            {
                                Console.WriteLine($"Fail converting file timeout; {fileName}; {watch.ElapsedMilliseconds} ms; Timed out after {convertTimeout.TotalMilliseconds} ms");
                                coolClient.CancelPendingRequests();
                                ConvertTask.Wait();
                                fileStats.addTimeToConvert(fileType, watch.ElapsedMilliseconds);
                                fileStats.addToFailConvertFiles(fileType, fileName);
                                continue;
                            }
                            else if (!ConvertTask.Result.Item1)
                            {
                                Console.WriteLine($"Fail converting file; {fileName}; {watch.ElapsedMilliseconds} ms; {ConvertTask.Result.Item3}");
                                fileStats.addTimeToConvert(fileType, watch.ElapsedMilliseconds);
                                fileStats.addToFailConvertFiles(fileType, fileName);
                                continue;
                            }
                            fileStats.addTimeToConvert(fileType, watch.ElapsedMilliseconds);
                            convertedFilesToTest.Enqueue((fileType, fileName));

                            if (!config.GeneratePDF)
                                continue;
                            // Make a PDF of the file (not using the round-tripped document, but using the originally provided document)
                            //watch.Restart();
                            convertedFileName = @"converted\" + fileType + @"\" + Path.GetFileNameWithoutExtension(fileName) + ".pdf";
                            FileInfo PDFexport = new FileInfo(convertedFileName);
                            if (!PDFexport.Exists)
                            {
                                Task<(bool, string, string)> PDFTask = Task.Run(() => ConvertFile(coolClient, application, fullFileName, convertedFileName, fileType, "pdf"));
                                if (!PDFTask.Wait(convertTimeout))
                                {
                                    Console.WriteLine($"Fail PDF export file timeout; {fileName}; {watch.ElapsedMilliseconds} ms; Timed out after {convertTimeout.TotalMilliseconds} ms");
                                    coolClient.CancelPendingRequests();
                                    PDFTask.Wait();
                                    continue;
                                }
                                else if (!PDFTask.Result.Item1)
                                {
                                    Console.WriteLine($"Fail creating PDF export; {fileName}; {watch.ElapsedMilliseconds} ms; {ConvertTask.Result.Item3}");
                                    continue;
                                }
                            }
                            else
                                Console.WriteLine("\nDEBUG: PDF already exists for " + PDFexport.FullName);
                        }
                    }));
                }
            }
            else
            {
                // Use LO as converter
                convertFiles.Add(Task.Run(() =>
                {
                    Stopwatch watch = new System.Diagnostics.Stopwatch();
                    while (true)
                    {
                        bool success = filesToConvert.TryDequeue(out var element);
                        if (testOriginalFilesCompleted && !success)
                            break;
                        if (!success)
                        {
                            System.Threading.Thread.Sleep(2000);
                            continue;
                        }
                        (string fileType, string fileName) = element;
                        string fullFileName = @"download\" + fileType + @"\" + fileName;
                        string convertTo = targetFormatPerApp[application];
                        string targetDir = @"converted\" + fileType;
                        string convertedFileName = targetDir + @"\" + Path.GetFileNameWithoutExtension(fileName) + "." + convertTo;
                        Console.WriteLine($"{fileName} - Converting file");

                        // Round-trip file to MSO format in LO
                        watch.Restart();
                        string loName = Path.Combine(config.DesktopConverterLocation, "soffice.exe");
                        // In more recent versions --convert-to implies headless, but before 5.0 that doesn't seem to be the case
                        string loParams = "--headless --convert-to " + convertTo + " --outdir " + targetDir + " " + fullFileName;
                        Process loProcess = new Process();
                        loProcess.StartInfo.FileName = loName;
                        loProcess.StartInfo.Arguments = loParams;
                        loProcess.StartInfo.ErrorDialog = false;
                        loProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
                        loProcess.Start();
                        loProcess.WaitForExit((int)convertTimeout.TotalMilliseconds);
                        bool failed = false;
                        if (!loProcess.HasExited)
                        {
                            try
                            {
                                loProcess.Kill();
                                Process[] loBinProcesses = Process.GetProcessesByName("soffice.bin");
                                foreach (Process p in loBinProcesses)
                                {
                                    p.Kill();
                                }
                                failed = true;
                            }
                            catch { failed = true; }
                        }
                        fileStats.addTimeToConvert(fileType, watch.ElapsedMilliseconds);
                        if (failed)
                        {
                            Console.WriteLine(fileName + " - Error during conversion with LO, full command used:\n\t" + loName + " " + loParams);
                            fileStats.addToFailConvertFiles(fileType, fileName);
                        }
                        else
                            convertedFilesToTest.Enqueue((fileType, fileName));
                    }
                }));
            }

            // Only start 3rd phase after 1st is done, both use MSO
            originalTest.Wait();
            testOriginalFilesCompleted = true;
            Task testConvertedFiles = Task.Run(() =>
            {
                Stopwatch watch = new System.Diagnostics.Stopwatch();
                DirectoryInfo convertedDirInfo = new DirectoryInfo(@"converted");
                while (true)
                {
                    bool success = convertedFilesToTest.TryDequeue(out var element);
                    if (conversionCompleted && !success)
                        break;
                    if (!success)
                    {
                        System.Threading.Thread.Sleep(2000);
                        continue;
                    }
                    (string fileType, string origFileName) = element;
                    string convertTo = targetFormatPerApp[application];
                    string fullConvertedFileName = Path.Combine(convertedDirInfo.FullName, fileType + @"\" + Path.GetFileNameWithoutExtension(origFileName) + "." + convertTo);
                    Console.WriteLine(origFileName + " - Testing converted file");

                    // Open converted file
                    watch.Restart();
                    Task<(bool, string)> OpenConvertedFileTask = Task.Run(() => OpenFile(application, fullConvertedFileName, config.GeneratePDF));
                    if (!OpenConvertedFileTask.Wait(openTimeout))
                    {
                        Console.WriteLine($"Fail opening converted file timeout; {origFileName}; {watch.ElapsedMilliseconds} ms; Timed out after {openTimeout.TotalMilliseconds} ms");
                        restartApplication(application);
                        OpenConvertedFileTask.Wait();
                        fileStats.addTimeToOpenConvertedFiles(fileType, watch.ElapsedMilliseconds);
                        fileStats.addToFailOpenConvertedFiles(fileType, origFileName);
                        continue;
                    }
                    else if (!OpenConvertedFileTask.Result.Item1)
                    {
                        Console.WriteLine($"Fail opening converted file; {origFileName}; {watch.ElapsedMilliseconds} ms; {OpenConvertedFileTask.Result.Item2}");
                        restartApplication(application);
                        fileStats.addTimeToOpenConvertedFiles(fileType, watch.ElapsedMilliseconds);
                        fileStats.addToFailOpenConvertedFiles(fileType, origFileName);
                        continue;
                    }
                    fileStats.addTimeToOpenConvertedFiles(fileType, watch.ElapsedMilliseconds);

                    Console.WriteLine($"Pass; {origFileName}; {watch.ElapsedMilliseconds} ms (last step)");
                }
            });
            foreach (Task task in convertFiles)
                task.Wait();
            conversionCompleted = true;
            testConvertedFiles.Wait();
        }

        /*
         * Returns: (success, converted file name, error message)
         */
        public static async Task<(bool, string, string)> ConvertFile(HttpClient convertService, ApplicationType application, string fullFileName, string convertedFileName, string fileType, string convertTo)
        {
            using (var request = new HttpRequestMessage(new HttpMethod("POST"), convertService.BaseAddress + "cool/convert-to/" + convertTo))
            {
                var multipartContent = new MultipartFormDataContent
                {
                    { new ByteArrayContent(File.ReadAllBytes(fullFileName)), "data", Path.GetFileName(fullFileName) }
                };
                request.Content = multipartContent;
                try
                {
                    using (var response = await convertService.SendAsync(request))
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
        public static async Task<(bool, string)> TestConvertService(HttpClient convertService)
        {
            using (var request = new HttpRequestMessage(new HttpMethod("GET"), convertService.BaseAddress + "hosting/capabilities"))
            {
                try
                {
                    using (var response = await convertService.SendAsync(request))
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

        private (bool, string) OpenWordDoc(string fileName, bool generatePDF)
        {
            word.Document doc = null;
            bool testResult = true;
            string errorMessage = "";

            try
            {
                doc = wordApp.Documents.OpenNoRepairDialog(fileName, ReadOnly: true, Visible: false, PasswordDocument: "'");
                if (generatePDF)
                {
                    FileInfo PDFexport = new FileInfo(fileName + @"_mso.pdf");
                    if (!PDFexport.Exists)
                        doc.ExportAsFixedFormat(PDFexport.FullName, word.WdExportFormat.wdExportFormatPDF);
                    else
                        Console.WriteLine("\nDEBUG: PDF already exists for " + PDFexport.FullName);
                }
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

        private (bool, string) OpenExcelWorkbook(string fileName, bool generatePDF)
        {
            excel.Workbook wb = null;
            bool testResult = true;
            string errorMessage = "";

            try
            {
                wb = excelApp.Workbooks.Open(fileName, ReadOnly: true, Password: "'", UpdateLinks: false);
                if (generatePDF)
                {
                    FileInfo PDFexport = new FileInfo(fileName + @"_mso.pdf");
                    if (!PDFexport.Exists)
                        wb.ExportAsFixedFormat(excel.XlFixedFormatType.xlTypePDF, Filename: PDFexport.FullName);
                    else
                        Console.WriteLine("\nDEBUG: PDF already exists for " + PDFexport.FullName);
                }
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

        private (bool, string) OpenPowerPointPresentation(string fileName, bool generatePDF)
        {
            powerPoint.Presentation presentation = null;
            bool testResult = true;
            string errorMessage = "";

            try
            {
                presentation = powerPointApp.Presentations.Open(fileName, ReadOnly: Microsoft.Office.Core.MsoTriState.msoTrue, WithWindow: Microsoft.Office.Core.MsoTriState.msoFalse);
                if (generatePDF)
                {
                    FileInfo PDFexport = new FileInfo(fileName + @"_mso.pdf");
                    if (!PDFexport.Exists)
                        presentation.ExportAsFixedFormat2(PDFexport.FullName, powerPoint.PpFixedFormatType.ppFixedFormatTypePDF);
                    else
                        Console.WriteLine("\nDEBUG: PDF already exists for " + PDFexport.FullName);
                }
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

        public void PresentResults(ApplicationType application)
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
                int failOpenFiles = fileStats.getCurrFailOpenOriginalFilesNo(fileType);
                // For ODF all files are added to PassOpenOriginalFiles, to have an accurate total
                int passOpenFiles = fileStats.getCurrPassOpenOriginalFilesNo(fileType);
                int totalFiles = failOpenFiles + passOpenFiles;
                // If no total files, it means nothing was tested
                if (totalFiles == 0)
                    continue;

                int failConvertFiles = fileStats.getFailConvertFilesNo(fileType);
                int failOpenConvertedFiles = fileStats.getFailOpenConvertedFilesNo(fileType);
                int finalPassOpenFiles = passOpenFiles - failConvertFiles - failOpenConvertedFiles;
                // passOpenFiles are the total actually tested files (=100%)
                string failConvertFilesPercentage = String.Format(culture, "{0:0.0}%", Convert.ToDouble(failConvertFiles) / passOpenFiles * 100);
                string failOpenConvertedFilesPercentage = String.Format(culture, "{0:0.0}%", Convert.ToDouble(failOpenConvertedFiles) / passOpenFiles * 100);
                string finalPassOpenFilesPercentage = String.Format(culture, "{0:0.0}%", Convert.ToDouble(finalPassOpenFiles) / passOpenFiles * 100);

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
                Console.WriteLine("Total succeeded:         " + finalPassOpenFiles + " (" + finalPassOpenFilesPercentage + ")");
                Console.WriteLine();
                Console.WriteLine("Time spent opening original files   : " + fileStats.getTimeToOpenOriginalFiles(fileType) / 1000 + " seconds");
                Console.WriteLine("Time spent converting files         : " + fileStats.getTimeToConvert(fileType) / 1000 + " seconds");
                Console.WriteLine("Time spent opening files after conv.: " + fileStats.getTimeToOpenConvertedFiles(fileType) / 1000 + " seconds");

                fileStats.saveFailedFileLists(new DirectoryInfo(@"download"), fileType);
            }

            File.WriteAllText(@"download\result-" + applicationNames[application] + ".csv", csv.ToString());
        }
    }
}
