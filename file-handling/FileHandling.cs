/*
 * Copyright the mso-test contributors.
 *
 * SPDX-License-Identifier: MPL-2.0
 */

using System.Collections.Generic;
using System;
using System.IO;
using System.Linq;

namespace file_handling
{
    internal class FileManager
    {
        private static void Main(string[] args)
        {
            if (args[0] == "reset-failed")
            {
                resetFailedDownloadedFiles();
            }
            else
                getOriginalFiles(args[0]);
        }

        public static void getOriginalFiles(string application)
        {
            string directoryPath = @"converted\";

            if (application == "word")
                directoryPath += @"docx\";
            else if (application == "excel")
                directoryPath += @"xlsx\";
            else if (application == "powerpoint")
                directoryPath += @"pptx\";

            if (!Directory.Exists(directoryPath))
                return;
            string[] timeoutFiles = System.IO.Directory.GetFiles(directoryPath, "*.timeout");
            string[] failedFiles = System.IO.Directory.GetFiles(directoryPath, "*.failed");

            string[] files = failedFiles.Union(timeoutFiles).ToArray();

            for (int i = 0; i < files.Length; i++)
            {
                string filename = Path.GetFileName(files[i]);

                files[i] = filename.Substring(0, filename.Length - Path.GetExtension(filename).Length);
                files[i] = files[i].Substring(0, files[i].Length - Path.GetExtension(files[i]).Length);
            }

            DirectoryInfo OriginDirInfo = Directory.CreateDirectory(Path.GetDirectoryName(@"original\" + application + @"\"));

            Dictionary<string, List<FileInfo>> map = getFileinfoMap(application);

            foreach (string file in files)
            {
                if (map.ContainsKey(file))
                {
                    foreach (FileInfo fi in map[file])
                    {
                        System.IO.File.Copy(fi.FullName, OriginDirInfo.FullName + @"\" + fi.Name, true);
                    }
                }
            }
        }

        public static Dictionary<string, List<FileInfo>> createMap(string[] directores)
        {
            Dictionary<string, List<FileInfo>> map = new Dictionary<string, List<FileInfo>>();
            foreach (string dir in directores)
            {
                DirectoryInfo downloadedDirInfo = new DirectoryInfo(dir);
                if (!downloadedDirInfo.Exists)
                    continue;
                FileInfo[] fileinfo = downloadedDirInfo.GetFiles();
                foreach (FileInfo file in fileinfo)
                {
                    string key = Path.GetFileNameWithoutExtension(file.Name);
                    if (map.ContainsKey(key))
                        map[key].Add(file);
                    else
                        map.Add(key, new List<FileInfo> { file });
                }
            }

            return map;
        }

        public static Dictionary<string, List<FileInfo>> getFileinfoMap(string application)
        {
            string[] directories;
            if (application == "word")
                directories = new string[] { @"download\docx\", @"download\doc\", @"download\odt\" };
            else if (application == "excel")
                directories = new string[] { @"download\xlsx\", @"download\xls\", @"download\ods\" };
            else
                directories = new string[] { @"download\pptx\", @"download\ppt\", @"download\odp\" };

            return createMap(directories);
        }

        public static void resetFailedDownloadedFiles()
        {
            void resetFailedFile(string dir)
            {
                string[] files = System.IO.Directory.GetFiles(dir, "*.failed");
                foreach (string file in files)
                {
                    try
                    {
                        Console.WriteLine("Moving file " + file);
                        System.IO.File.Move(file, file.Substring(0, file.Length - ".failed".Length));
                    }
                    catch (System.IO.IOException ex) { Console.WriteLine(ex.Message); }
                }

                string[] convfailfiles = System.IO.Directory.GetFiles(dir, "*.convfail");
                foreach (string file in convfailfiles)
                {
                    try
                    {
                        Console.WriteLine("Moving file " + file);
                        System.IO.File.Move(file, file.Substring(0, file.Length - ".convfail".Length));
                    }
                    catch (System.IO.IOException ex) { Console.WriteLine(ex.Message); }
                }
            }

            DirectoryInfo downloadedDirInfo = new DirectoryInfo(@"download");
            if (!downloadedDirInfo.Exists)
            {
                return;
            }
            DirectoryInfo[] downloadedDictInfo = downloadedDirInfo.GetDirectories();
            foreach (DirectoryInfo dict in downloadedDictInfo)
            {
                resetFailedFile(dict.FullName);
            }
        }
    }
}
