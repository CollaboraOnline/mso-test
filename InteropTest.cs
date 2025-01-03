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
using System.Threading;

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
        // Base directory of input/output data (except config)
        public string BaseDir = "";
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
        // Which steps to do: 1 (only initial open in MSO), 2 (initial open and conversion), 3 (all steps)
        public int EndWithStep = 3;
    }

    internal class Logger
    {
        static StreamWriter logFile;
        private static object streamLock = new object();
        public static void Initialize(string fileName) { logFile = new StreamWriter(fileName, false); }

        public static void Close() { if (logFile != null) { logFile.Close(); } }
        public static void Write(string message)
        {
            if (logFile != null)
            {
                lock (streamLock)
                    logFile.WriteLine(message);
            }
            Console.WriteLine(message);
        }
    }
    internal class InteropTest
    {
        static TimeSpan openTimeout = TimeSpan.FromMilliseconds(30000);
        static TimeSpan convertTimeout = TimeSpan.FromMilliseconds(100000);

        Configuration config = new Configuration();
        FileStatistics fileStats = new FileStatistics();

        static word.Application wordApp;
        static excel.Application excelApp;
        static powerPoint.Application powerPointApp;

        ConcurrentQueue<(string, string)> filesToConvert = new ConcurrentQueue<(string, string)>();
        ConcurrentQueue<(string, string)> convertedFilesToTest = new ConcurrentQueue<(string, string)>();
        volatile bool testOriginalFilesCompleted = false;
        volatile bool conversionCompleted = false;

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
                    Logger.Write("Exception on startup:");
                    Logger.Write(ex.ToString());
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
                // Quit() might not return at times for "bad" documents
                //  this way the code always moves on and kills the app... the task might linger on(?)
                try
                {
                    Task quitAppTask = Task.Run(() =>
                    {
                        if (application == ApplicationType.Word)
                            wordApp.Quit();

                        if (application == ApplicationType.Excel)
                            excelApp.Quit();

                        if (application == ApplicationType.PowerPoint)
                            powerPointApp.Quit();
                    });
                    quitAppTask.Wait(5000);
                }
                catch { }
            }
            catch { }
            Object app = null;
            if (application == ApplicationType.Word)
                app = wordApp;
            if (application == ApplicationType.Excel)
                app = excelApp;
            if (application == ApplicationType.PowerPoint)
                app = powerPointApp;
            if (app != null)
                System.Runtime.InteropServices.Marshal.FinalReleaseComObject(app);
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

        public void getLOParams(string formatTo, string targetDir, string fullOrigFileName, out string loName, out string loParams)
        {
            loName = Path.Combine(config.DesktopConverterLocation, "soffice.exe");

            // Prior to LO 25.8, "--convert-to docx" always resulted in using the Word 2007 filter (and after 7.6 always output compatiblityMode 12),
            // So force using the modern filter (compat15 since 7.0 in 2020) to ensure equivalent comparisons.
            if (formatTo == "docx")
                formatTo = @"""docx:Office Open XML Text""";
            // In more recent versions --convert-to implies headless, but before 5.0 that doesn't seem to be the case
            loParams = "--headless --convert-to " + formatTo + " --outdir " + targetDir + " " + fullOrigFileName;
        }
        public bool convertWithLO(string formatTo, string targetDir, string fullOrigFileName)
        {
            Process loProcess = new Process(); ;
            string loName, loParams;
            getLOParams(formatTo, targetDir, fullOrigFileName, out loName, out loParams);
            loProcess.StartInfo.FileName = loName;
            loProcess.StartInfo.Arguments = loParams;
            loProcess.StartInfo.ErrorDialog = false;
            loProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;
            loProcess.Start();
            loProcess.WaitForExit((int)convertTimeout.TotalMilliseconds);

            return killLOProcess(loProcess);

        }

        public static bool killLOProcess(Process loProcess)
        {
            if (loProcess != null && !loProcess.HasExited)
            {
                try
                {
                    loProcess.Kill();
                    Process[] loBinProcesses = Process.GetProcessesByName("soffice.bin");
                    foreach (Process p in loBinProcesses)
                    {
                        p.Kill();
                    }
                    return false;
                }
                catch { return false; }
            }
            return true;
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
            { ApplicationType.Word, "word" },
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

            if (test.config.BaseDir != "")
            {
                if (!Directory.Exists(test.config.BaseDir))
                {
                    Console.Error.WriteLine($"ERROR: BaseDir is configured, but the path {test.config.BaseDir} doesn't exist");
                    Environment.Exit(1);
                }
            }

            Logger.Initialize(Path.Combine(test.config.BaseDir, "log.txt"));

            forceQuitAllApplication(test.config.Application);
            startApplication(test.config.Application);

            Stopwatch watch = new System.Diagnostics.Stopwatch();
            watch.Start();
            test.TestDownloadedFiles(test.config.Application);
            watch.Stop();
            test.PresentResults(test.config.Application);
            Logger.Write("\nFinished in " + watch.ElapsedMilliseconds / 1000 + " seconds");
            Logger.Close();

            quitApplication(test.config.Application);
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
                if (configJson["BaseDir"] != null)
                    config.BaseDir = configJson["BaseDir"].GetValue<string>();
                if (configJson["EndWithStep"] != null && configJson["EndWithStep"].GetValue<int>() <= 3)
                    config.EndWithStep = configJson["EndWithStep"].GetValue<int>();
            }
        }
        public static (bool, string) OpenFile(ApplicationType application, string fileName, bool generatePDF)
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
            DirectoryInfo downloadedDirInfo = new DirectoryInfo(Path.Combine(config.BaseDir, "download"));
            if (!downloadedDirInfo.Exists)
            {
                Console.Error.WriteLine("Download directory does not exist: " + downloadedDirInfo.FullName);
                return;
            }

            Logger.Write("Starting tests for application: " + applicationNames[application]);

            Task originalTest = Task.Run(() => TestOriginalFiles(application, downloadedDirInfo));

            // Only want to build the list of test files that can be opened originally, and used for testing
            if (config.EndWithStep == 1)
            {
                originalTest.Wait();
                return;
            }

            List<Task> convertFiles = new List<Task>();
            if (config.OnlineConverter)
            {
                HttpClient coolClient = new HttpClient()
                {
                    BaseAddress = new Uri(config.OnlineConverterBaseURL),
                    Timeout = convertTimeout
                };
                for (int i = 0; i < config.OnlineConverterTasks; i++)
                {
                    convertFiles.Add(Task.Run(async () => ConvertFiles(application, coolClient)));
                }
            }
            else
            {
                // Use LO as converter
                convertFiles.Add(Task.Run(async () => ConvertFilesUsingLO(application)));
            }

            // Only start 3rd phase after 1st is done, both use MSO
            originalTest.Wait();

            // Only want to perform conversion eg. to build PDFs (skip step 3)
            if (config.EndWithStep == 2)
            {
                Task.WaitAll(convertFiles.ToArray());
                return;
            }

            Task testConvertedFiles = Task.Run(async () => TestConvertedFiles(application));
            Task.WaitAll(convertFiles.ToArray());
            testConvertedFiles.Wait();
        }

        private void TestOriginalFiles(ApplicationType application, DirectoryInfo downloadDirInfo)
        {
            Stopwatch watch = new System.Diagnostics.Stopwatch();

            string filesToSkipName = Path.Combine(downloadDirInfo.FullName, "filestoskip.txt");
            if (File.Exists(filesToSkipName))
                Logger.Write(@"The file download\filestoskip.txt exists, files listed there will be skipped.");
            HashSet<string> filesToSkip = FileStatistics.ReadFileToSet(filesToSkipName);
            string filesToTestName = Path.Combine(downloadDirInfo.FullName, "filestotest.txt");
            if (File.Exists(filesToTestName))
                Logger.Write(@"The file download\filestotest.txt exists, and will be used to limit the files tested.");
            HashSet<string> filesToTest = FileStatistics.ReadFileToSet(filesToTestName);
            Logger.Write("");

            var downloadedDictInfo = downloadDirInfo.GetDirectories().OrderBy(d => d.Name);
            int fileNo = 0;
            foreach (DirectoryInfo dict in downloadedDictInfo)
            {
                if (!InteropTest.fileTypesPerApp[application].Contains(dict.Name))
                    continue;
                string fileType = dict.Name;

                if (nativeMSOFileTypes.Contains(fileType))
                    fileStats.initOpenOriginalFileLists(downloadDirInfo, fileType);

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

                    // MSO lock file names end with the usual extension, but start with ~$
                    if (!allowedExtension.Contains(file.Extension) || file.Name.StartsWith("~$"))
                    {
                        Logger.Write($"{file.Name} - Skipping non test file");
                        continue;
                    }

                    // Open original file, only if it's a native format, and not known if it can be opened in MSO
                    if (nativeMSOExtensions.Contains(file.Extension))
                    {
                        if (fileStats.isFailOpenOriginalFile(fileType, file.Name))
                        {
                            Logger.Write($"{file.Name} - Fail; Previously failed opening original file");
                            fileStats.incrCurrFailOpenOriginalFilesNo(fileType);
                            continue;
                        }

                        // Use the PDF as sufficient proof that the original file can be opened.
                        FileInfo PDFfile = new FileInfo(file.FullName + "_mso.pdf");
                        bool testOriginal = (!config.GeneratePDF && !fileStats.isPassOpenOriginalFile(fileType, file.Name)) || (config.GeneratePDF && !PDFfile.Exists);
                        if (testOriginal)
                        {
                            Logger.Write($"{file.Name} - Opening original file at {DateTime.Now.ToString("s")}");
                            watch.Restart();
                            Task<(bool, string)> OpenOriginalFileTask = Task.Run(() => OpenFile(application, file.FullName, config.GeneratePDF));
                            if (!OpenOriginalFileTask.Wait(openTimeout))
                            {
                                Logger.Write($"Fail opening original file timeout; {file.Name}; {watch.ElapsedMilliseconds} ms; Timed out after {openTimeout.TotalMilliseconds} ms");
                                restartApplication(application);
                                OpenOriginalFileTask.Wait();
                                fileStats.addTimeToOpenOriginalFiles(fileType, watch.ElapsedMilliseconds);
                                fileStats.addToFailOpenOriginalFiles(fileType, file.Name);
                                fileStats.incrCurrFailOpenOriginalFilesNo(fileType);
                                continue;
                            }
                            else if (!OpenOriginalFileTask.Result.Item1)
                            {
                                Logger.Write($"Fail opening original file; {file.Name}; {watch.ElapsedMilliseconds} ms; {OpenOriginalFileTask.Result.Item2}");
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
                            Logger.Write($"{file.Name} - Original file could be opened previously, skipping this step");
                            filesToConvert.Enqueue((fileType, file.Name));
                            fileStats.incrCurrPassOpenOriginalFilesNo(fileType);
                        }
                    }
                    else
                    {
                        Logger.Write($"{file.Name} - Original is not native MSO file, skipping testing whether it can be opened in MSO");
                        filesToConvert.Enqueue((fileType, file.Name));
                        fileStats.incrCurrPassOpenOriginalFilesNo(fileType);
                        fileStats.addToPassOpenOriginalFiles(fileType, file.Name);
                    }
                }
                if (nativeMSOFileTypes.Contains(fileType))
                    fileStats.saveOpenOriginalFileLists(downloadDirInfo, fileType);
            }
            testOriginalFilesCompleted = true;
            Logger.Write("Finished first step: opening original files.");
        }

        private async void ConvertFiles(ApplicationType application, HttpClient coolClient)
        {
            Stopwatch watch = new System.Diagnostics.Stopwatch();
            while (true)
            {
                bool success = filesToConvert.TryDequeue(out var element);
                if (testOriginalFilesCompleted && !success)
                    break;
                if (!success)
                {
                    await Task.Delay(2000);
                    continue;
                }
                (string fileType, string fileName) = element;
                string fullFileName = Path.Combine(config.BaseDir, @"download\" + fileType + @"\" + fileName);
                string convertTo = targetFormatPerApp[application];
                string targetDir = Path.Combine(config.BaseDir, @"converted\" + fileType);
                string convertedFileName = Path.Combine(targetDir, Path.GetFileNameWithoutExtension(fileName) + "." + convertTo);
                Logger.Write($"{fileName} - Converting file at {DateTime.Now.ToString("s")}");

                // Round-trip file to MSO format
                watch.Restart();
                Task<(bool, int, string, string)> ConvertTask = Task.Run(() => ConvertFile(coolClient, application, fullFileName, convertedFileName, fileType, convertTo));
                if (!ConvertTask.Result.Item1)
                {
                    Logger.Write($"Fail converting file {fileName}; {ConvertTask.Result.Item2} tries; {watch.ElapsedMilliseconds} ms; {ConvertTask.Result.Item4}");
                    fileStats.addTimeToConvert(fileType, watch.ElapsedMilliseconds);
                    fileStats.addToFailConvertFiles(fileType, fileName);
                    continue;
                }
                else if (ConvertTask.Result.Item2 > 1)
                {
                    // Passed after a service outage, log that
                    Logger.Write($"Pass converting file {fileName}; {ConvertTask.Result.Item2} tries;");
                }
                fileStats.addTimeToConvert(fileType, watch.ElapsedMilliseconds);
                convertedFilesToTest.Enqueue((fileType, fileName));

                if (!config.GeneratePDF)
                    continue;
                // Make a PDF of the file (not using the round-tripped document, but using the originally provided document)
                //watch.Restart();
                string convertedPDFFileName = Path.Combine(targetDir, Path.GetFileNameWithoutExtension(fileName) + ".pdf");
                FileInfo PDFexport = new FileInfo(convertedPDFFileName);
                if (!PDFexport.Exists)
                {
                    Task<(bool, int, string, string)> PDFTask = Task.Run(() => ConvertFile(coolClient, application, fullFileName, convertedPDFFileName, fileType, "pdf"));
                    if (!PDFTask.Result.Item1)
                    {
                        Logger.Write($"Fail creating PDF export; {fileName}; {ConvertTask.Result.Item2} tries; {watch.ElapsedMilliseconds} ms; {ConvertTask.Result.Item4}");
                        continue;
                    }
                }
                else
                    Logger.Write("DEBUG: PDF already exists for " + PDFexport.FullName);
            }
            conversionCompleted = true;
            Logger.Write("Finished task for second step: converting files.");
        }

        private async void ConvertFilesUsingLO(ApplicationType application)
        {
            Stopwatch watch = new System.Diagnostics.Stopwatch();
            while (true)
            {
                bool success = filesToConvert.TryDequeue(out var element);
                if (testOriginalFilesCompleted && !success)
                    break;
                if (!success)
                {
                    await Task.Delay(2000);
                    continue;
                }
                (string fileType, string fileName) = element;
                string fullFileName = Path.Combine(config.BaseDir, @"download\" + fileType + @"\" + fileName);
                string convertTo = targetFormatPerApp[application];
                string targetDir = Path.Combine(config.BaseDir, @"converted\" + fileType);
                string convertedFileName = Path.Combine(targetDir, @"\" + Path.GetFileNameWithoutExtension(fileName) + "." + convertTo);
                Logger.Write($"{fileName} - Converting file at {DateTime.Now.ToString("s")}");

                // Round-trip file to MSO format in LO
                watch.Restart();
                success = convertWithLO(convertTo, targetDir, fullFileName);
                fileStats.addTimeToConvert(fileType, watch.ElapsedMilliseconds);
                if (!success)
                {
                    string loName, loParams;
                    getLOParams(convertTo, targetDir, fullFileName, out loName, out loParams);
                    Logger.Write(fileName + " - Error during conversion with LO, full command used:\n\t" + loName + " " + loParams);
                    fileStats.addToFailConvertFiles(fileType, fileName);
                }
                else
                    convertedFilesToTest.Enqueue((fileType, fileName));

                if (!config.GeneratePDF)
                    continue;
                // Make a PDF of the file (not using the round-tripped document, but using the originally provided document)
                //watch.Restart();
                string convertedPDFFileName = Path.Combine(targetDir, Path.GetFileNameWithoutExtension(fileName) + ".pdf");
                FileInfo PDFexport = new FileInfo(convertedPDFFileName);
                if (!PDFexport.Exists)
                {
                    success = convertWithLO("pdf", targetDir, fullFileName);
                    if (!success)
                        Logger.Write($"Fail creating PDF export; {fileName}");
                }
                else
                    Logger.Write("DEBUG: PDF already exists for " + PDFexport.FullName);
            }
            conversionCompleted = true;
            Logger.Write("Finished task for second step: converting files.");
        }

        private async void TestConvertedFiles(ApplicationType application)
        {
            Stopwatch watch = new System.Diagnostics.Stopwatch();
            DirectoryInfo convertedDirInfo = new DirectoryInfo(Path.Combine(config.BaseDir, @"converted"));
            while (true)
            {
                bool success = convertedFilesToTest.TryDequeue(out var element);
                if (conversionCompleted && !success)
                    break;
                if (!success)
                {
                    await Task.Delay(2000);
                    continue;
                }
                (string fileType, string origFileName) = element;
                string convertTo = targetFormatPerApp[application];
                string fullConvertedFileName = Path.Combine(convertedDirInfo.FullName, fileType + @"\" + Path.GetFileNameWithoutExtension(origFileName) + "." + convertTo);
                Logger.Write(origFileName + $" - Testing converted file at {DateTime.Now.ToString("s")}");

                // Open converted file
                watch.Restart();
                Task<(bool, string)> OpenConvertedFileTask = Task.Run(() => OpenFile(application, fullConvertedFileName, config.GeneratePDF));
                if (!OpenConvertedFileTask.Wait(openTimeout))
                {
                    Logger.Write($"Fail opening converted file timeout; {origFileName}; {watch.ElapsedMilliseconds} ms; Timed out after {openTimeout.TotalMilliseconds} ms");
                    restartApplication(application);
                    OpenConvertedFileTask.Wait();
                    fileStats.addTimeToOpenConvertedFiles(fileType, watch.ElapsedMilliseconds);
                    fileStats.addToFailOpenConvertedFiles(fileType, origFileName);
                    continue;
                }
                else if (!OpenConvertedFileTask.Result.Item1)
                {
                    Logger.Write($"Fail opening converted file; {origFileName}; {watch.ElapsedMilliseconds} ms; {OpenConvertedFileTask.Result.Item2}");
                    restartApplication(application);
                    fileStats.addTimeToOpenConvertedFiles(fileType, watch.ElapsedMilliseconds);
                    fileStats.addToFailOpenConvertedFiles(fileType, origFileName);
                    continue;
                }
                fileStats.addTimeToOpenConvertedFiles(fileType, watch.ElapsedMilliseconds);

                Logger.Write($"Pass; {origFileName}; {watch.ElapsedMilliseconds} ms (last step)");
            }
            Logger.Write("Finished third step: opening converted files.");
        }

        /*
         * Returns: (success/failure, number of tries, converted file name, error message)
         */
        public static async Task<(bool, int, string, string)> ConvertFile(HttpClient convertService, ApplicationType application, string fullFileName, string convertedFileName, string fileType, string convertTo)
        {
            const int maxConvertTries = 2;
            const int maxTestTries = 5;
            int totalTries = 0;
            string message = "";
            // If first conversion is unsuccessful, test the endpoint, and retry conversion if succesful
            for (int numTries = 0; numTries < maxConvertTries; numTries++)
            {
                using (var request = new HttpRequestMessage(new HttpMethod("POST"), convertService.BaseAddress + "cool/convert-to/" + convertTo))
                {
                    totalTries++;
                    var multipartContent = new MultipartFormDataContent
                    {
                        { new ByteArrayContent(File.ReadAllBytes(fullFileName)), "data", Path.GetFileName(fullFileName) }
                    };
                    request.Content = multipartContent;
                    try
                    {
                        CancellationTokenSource ctks = new CancellationTokenSource();
                        CancellationToken ctk = ctks.Token;
                        var convertResult = convertService.SendAsync(request, ctk);
                        if (await Task.WhenAny(convertResult, Task.Delay(convertTimeout)) == convertResult)
                        {
                            var response = await convertResult;
                            if (!response.IsSuccessStatusCode)
                            {
                                message = "HTTP StatusCode: " + response.StatusCode;
                            }
                            else
                            {
                                Directory.CreateDirectory(Path.GetDirectoryName(convertedFileName));
                                using (FileStream fs = File.Open(convertedFileName, FileMode.Create))
                                {
                                    await response.Content.CopyToAsync(fs);
                                    return (true, totalTries, fs.Name, "");
                                }
                            }
                        }
                        else
                        {
                            ctks.Cancel();
                            message = "Timed out during convert";
                        }
                    }
                    catch (Exception ex)
                    {
                        message = "Exception during convert: " + ex.Message;
                    }
                }
                // Last attempt, if it still failed, no need to test if the server is up
                if (numTries == maxConvertTries - 1)
                {
                    break;
                }
                // Unsuccessful conversion, try capabilities endpoint a few times to see if it comes back before coming to conclusions
                bool serviceSuccess = false;
                int convertTries;
                for (convertTries = 0; convertTries < maxTestTries; convertTries++)
                {
                    totalTries++;
                    using (var request = new HttpRequestMessage(new HttpMethod("GET"), convertService.BaseAddress + "hosting/capabilities"))
                    {
                        try
                        {
                            using (var response = await convertService.SendAsync(request))
                            {
                                if (response.IsSuccessStatusCode)
                                {
                                    serviceSuccess = true;
                                    break;
                                }
                                // Service down, wait for it to come back
                                await Task.Delay(convertTimeout);
                            }
                        }
                        catch {
                            // Service down, wait for it to come back
                            await Task.Delay(convertTimeout);
                        }
                    }
                }
                // If conversion failed, but test try was OK right away, this is a real failure, break
                if (serviceSuccess && convertTries == 0)
                    break;
                // If conversion failed, and all test tries failed, service is down, break
                if (!serviceSuccess)
                    break;
            }
            return (false, totalTries, "", message);
        }

        private static (bool, string) OpenWordDoc(string fileName, bool generatePDF)
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
                        Logger.Write("\nDEBUG: PDF already exists for " + PDFexport.FullName);
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

        private static (bool, string) OpenExcelWorkbook(string fileName, bool generatePDF)
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
                        Logger.Write("\nDEBUG: PDF already exists for " + PDFexport.FullName);
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

        private static (bool, string) OpenPowerPointPresentation(string fileName, bool generatePDF)
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
                        Logger.Write("\nDEBUG: PDF already exists for " + PDFexport.FullName);
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
            csv.AppendLine("Format,Total,Open failed,Total tested,Conversion failed,,Open failed after conversion,,Total succeeded,");
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
                    failConvertFiles + "," + failConvertFilesPercentage + "," + failOpenConvertedFiles + "," + failOpenConvertedFilesPercentage + "," + finalPassOpenFiles + "," + finalPassOpenFilesPercentage);
                
                Logger.Write("\nFormat:                  " + fileType.ToUpper());
                Logger.Write("Total:                   " + totalFiles);
                Logger.Write("Open failed:             " + (nativeMSOFileTypes.Contains(fileType) ? failOpenFiles.ToString() : "-"));
                Logger.Write("Total tested:            " + passOpenFiles);
                Logger.Write("Conversion failure:      " + failConvertFiles + " (" + failConvertFilesPercentage + ")");
                //Logger.Write("Crash in MSO:            ");
                Logger.Write("Open failed after conv.: " + failOpenConvertedFiles + " (" + failOpenConvertedFilesPercentage + ")");
                Logger.Write("Total succeeded:         " + finalPassOpenFiles + " (" + finalPassOpenFilesPercentage + ")");
                Logger.Write("\n");
                Logger.Write("Time spent opening original files   : " + fileStats.getTimeToOpenOriginalFiles(fileType) / 1000 + " seconds");
                Logger.Write("Time spent converting files         : " + fileStats.getTimeToConvert(fileType) / 1000 + " seconds");
                Logger.Write("Time spent opening files after conv.: " + fileStats.getTimeToOpenConvertedFiles(fileType) / 1000 + " seconds");

                fileStats.saveFailedFileLists(new DirectoryInfo(Path.Combine(config.BaseDir, "download")), fileType);
            }

            File.WriteAllText(Path.Combine(config.BaseDir, @"download\result-" + applicationNames[application] + ".csv"), csv.ToString());
        }
    }
}
