/*
 * Copyright the mso-test contributors.
 *
 * SPDX-License-Identifier: MPL-2.0
 */

using System;
using System.Collections.Generic;
using System.Diagnostics.Eventing.Reader;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using excel = Microsoft.Office.Interop.Excel;
using powerPoint = Microsoft.Office.Interop.PowerPoint;
using word = Microsoft.Office.Interop.Word;
using Microsoft.Office.Interop.Excel;
using System.Threading;

namespace mso_test
{
    internal class InteropTest
    {
        public static word.Application wordApp;
        public static excel.Application excelApp;
        public static powerPoint.Application powerPointApp;

        /*
         * Sometimes app.Quit() does not actually quit the application, or other instances of the application
         * can be open, especially if a previous file errored or was interrupted. The open applications can
         * cause issues when testing the next file. This force quits all instances by process name so that
         * the next test file can be tested on a new instance of the application
         */
        public static void forceQuitAllApplication(string application)
        {
            if (application == "word")
            {
                application = "WINWORD";
            }
            else if (application == "excel")
            {
                application = "EXCEL";
            }
            else if (application == "powerpoint")
            {
                application = "POWERPNT";
            }
            Process[] processes = Process.GetProcessesByName(application);
            foreach (Process p in processes)
            {
                try { p.Kill(); } catch { }
            }
        }

        public static void startApplication(string application)
        {
            try
            {
                if (application == "word")
                {
                    wordApp = new word.Application();
                    wordApp.DisplayAlerts = word.WdAlertLevel.wdAlertsNone;
                    wordApp.AutomationSecurity = Microsoft.Office.Core.MsoAutomationSecurity.msoAutomationSecurityForceDisable;
                }

                if (application == "excel")
                {
                    excelApp = new excel.Application();
                    excelApp.DisplayAlerts = false;
                    excelApp.AutomationSecurity = Microsoft.Office.Core.MsoAutomationSecurity.msoAutomationSecurityForceDisable;
                }

                if (application == "powerpoint")
                {
                    powerPointApp = new powerPoint.Application();
                    powerPointApp.DisplayAlerts = powerPoint.PpAlertLevel.ppAlertsNone;
                    powerPointApp.AutomationSecurity = Microsoft.Office.Core.MsoAutomationSecurity.msoAutomationSecurityForceDisable;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Exception on startup:");
                Console.WriteLine(ex.ToString());
                System.Threading.Thread.Sleep(10000);
                forceQuitAllApplication(application);
                System.Threading.Thread.Sleep(10000);
                startApplication(application);
            }
        }

        public static void quitApplication(string application)
        {
            try
            {
                if (application == "word")
                    wordApp.Quit();

                if (application == "excel")
                    excelApp.Quit();

                if (application == "powerpoint")
                    powerPointApp.Quit();
            }
            catch
            {
                if (application == "word")
                    System.Runtime.InteropServices.Marshal.FinalReleaseComObject(wordApp);

                if (application == "excel")
                    System.Runtime.InteropServices.Marshal.FinalReleaseComObject(excelApp);

                if (application == "powerpoint")
                    System.Runtime.InteropServices.Marshal.FinalReleaseComObject(powerPointApp);
            }
            wordApp = null;
            excelApp = null;
            powerPointApp = null;

            forceQuitAllApplication(application);
        }

        public static void restartApplication(string application)
        {
            quitApplication(application);
            startApplication(application);
        }

        public static HttpClient coolClient = new HttpClient() { BaseAddress = new Uri("https://staging.eu.collaboraonline.com/cool/convert-to") };

        public static HashSet<string> allowedExtension = new HashSet<string>()
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

        //args can be word/excel/powerpoint
        //specified appropriate docs will be tested with the specified program
        private static void Main(string[] args)
        {
            ServicePointManager.Expect100Continue = false;

            if (args.Length <= 0)
            {
                Console.Error.WriteLine("Required argument word excel or powerpoint");
                Environment.Exit(1);
            }

            var application = args[0];
            forceQuitAllApplication(application);
            startApplication(application);
            TestDownloadedFiles(application);

            quitApplication(application);
        }

        public static (bool, string) OpenFile(string application, string fileName)
        {
            if (application == "word")
            {
                return OpenWordDoc(fileName);
            }
            else if (application == "excel")
            {
                return OpenExcelWorkbook(fileName);
            }
            else if (application == "powerpoint")
            {
                return OpenPowerPointPresentation(fileName);
            }
            else
            {
                return (false, "");
            }
        }

        public static async void TestFile(string application, FileInfo file, string convertTo)
        {
            Console.WriteLine("\nStarting test for " + file.Name);
            var watch = new System.Diagnostics.Stopwatch();
            watch.Start();
            var openTimeout = 10000;
            var convertTimeout = 30000;

            // Open original file
            Task<(bool, string)> DownloadResultTask = Task.Run(() => OpenFile(application, file.FullName));
            if (!DownloadResultTask.Wait(openTimeout))
            {
                Console.WriteLine("Fail; Opening original file timeout; " + file.Name + "; Timed out after " + openTimeout + "ms");
                restartApplication(application);
                await DownloadResultTask;
                return;
            }
            else if (!DownloadResultTask.Result.Item1)
            {
                Console.WriteLine("Fail; Opening original file; " + file.Name + "; " + DownloadResultTask.Result.Item2);
                restartApplication(application);
                return;
            }

            // Convert file
            Task<string> ConvertTask = Task.Run(() => ConvertFile(application, file.FullName, Path.GetFileNameWithoutExtension(file.Name), convertTo));
            if (!ConvertTask.Wait(convertTimeout))
            {
                Console.WriteLine("Fail; Converting file timeout; " + file.Name + "; Test timed out after " + convertTimeout + "ms");
                // TODO cancel ConvertTask
                await ConvertTask;
                return;
            }
            else if (string.IsNullOrEmpty(ConvertTask.Result))
            {
                // Failure printed in ConvertFile
                return;
            }

            // Open converted file
            Task<(bool, string)> ConvertResultTask = Task.Run(() => OpenFile(application, ConvertTask.Result));
            if (!ConvertResultTask.Wait(openTimeout))
            {
                Console.WriteLine("Fail; Opening converted file timeout; " + file.Name + "; Timed out after " + openTimeout + "ms");
                restartApplication(application);
                await ConvertResultTask;
                return;
            }
            else if (!ConvertResultTask.Result.Item1)
            {
                Console.WriteLine("Fail; Opening converted file; " + file.Name + "; " + ConvertResultTask.Result.Item2);
                restartApplication(application);
                return;
            }

            // Passed
            watch.Stop();
            Console.WriteLine("Success; " + file.Name + $"; {watch.ElapsedMilliseconds}ms");

        }

        public static void TestDirectory(string application, DirectoryInfo dir, string convertTo)
        {
            Console.WriteLine("\n\nStarting test directory " + dir.Name);
            FileInfo[] fileInfos = dir.GetFiles();
            foreach (FileInfo file in fileInfos)
            {
                if (!allowedExtension.Contains(file.Extension)) {
                    Console.WriteLine("\nSkipping file " + file.Name);
                    continue;
                }
                TestFile(application, file, convertTo);
            }
        }

        public static void TestDownloadedFiles(string application)
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
                if (application == "word" && (dict.Name == "doc" || dict.Name == "docx" || dict.Name == "odt"))
                {
                    TestDirectory(application, dict, "docx");
                }
                else if (application == "excel" && (dict.Name == "xls" || dict.Name == "xlsx" || dict.Name == "ods"))
                {
                    TestDirectory(application, dict, "xlsx");
                }
                else if (application == "powerpoint" && (dict.Name == "ppt" || dict.Name == "pptx" || dict.Name == "odp"))
                {
                    TestDirectory(application, dict, "pptx");
                }
            }
        }

        public static async Task<string> ConvertFile(string application, string fullFileName, string fileName, string convertTo)
        {
            using (var request = new HttpRequestMessage(new HttpMethod("POST"), coolClient.BaseAddress + "/" + convertTo))
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
                            Console.WriteLine("Fail; Converting file; " + fileName + "; HTTP StatusCode: " + response.StatusCode);
                            return "";
                        }
                        Directory.CreateDirectory(Path.GetDirectoryName(@"converted\" + convertTo + @"\"));
                        string convertedFilePath = @"converted\" + convertTo + @"\" + fileName + "." + convertTo;
                        using (FileStream fs = File.Open(convertedFilePath, FileMode.Create))
                        {
                            return await response.Content.CopyToAsync(fs).ContinueWith(task => { return fs.Name; });
                        }
                    }
                }
                catch (Exception ex) {
                    Console.WriteLine("Fail; Converting file; " + fileName + "; Exception during convert: " + ex.Message);
                }
            }
            return "";
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
    }
}
