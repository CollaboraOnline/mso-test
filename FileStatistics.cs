using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

namespace mso_test
{
    internal class FileStatistics
    {

        private Dictionary<string, SortedSet<string>> failOpenOriginalFiles = new Dictionary<string, SortedSet<string>>();
        private Dictionary<string, SortedSet<string>> passOpenOriginalFiles = new Dictionary<string, SortedSet<string>>();
        private Dictionary<string, SortedSet<string>> newFailOpenOriginalFiles = new Dictionary<string, SortedSet<string>>();
        private Dictionary<string, SortedSet<string>> newPassOpenOriginalFiles = new Dictionary<string, SortedSet<string>>();

        private Dictionary<string, SortedSet<string>> failConvertFiles = new Dictionary<string, SortedSet<string>>();
        private Dictionary<string, SortedSet<string>> failOpenConvertedFiles = new Dictionary<string, SortedSet<string>>();

        private Dictionary<string, long> timeToConvert = new Dictionary<string, long>();
        private Dictionary<string, long> timeToOpenConvertedFiles = new Dictionary<string, long>();

        public void initOpenOriginalFileLists(DirectoryInfo baseDir, string typeName)
        {
            failOpenOriginalFiles[typeName] = ReadFileToSet(Path.Combine(baseDir.FullName, typeName + "-failOpenOriginalFiles.txt"));
            passOpenOriginalFiles[typeName] = ReadFileToSet(Path.Combine(baseDir.FullName, typeName + "-passOpenOriginalFiles.txt"));
        }

        public void saveOpenOriginalFileLists(DirectoryInfo baseDir, string typeName)
        {
            if (newFailOpenOriginalFiles.ContainsKey(typeName) && newFailOpenOriginalFiles[typeName].Count > 0)
                WriteSetToFile(baseDir, typeName + "-failOpenOriginalFiles.txt", failOpenOriginalFiles[typeName]);
            if (newPassOpenOriginalFiles.ContainsKey(typeName) && newPassOpenOriginalFiles[typeName].Count > 0)
                WriteSetToFile(baseDir, typeName + "-passOpenOriginalFiles.txt", passOpenOriginalFiles[typeName]);
        }

        public void saveFailedFileLists(DirectoryInfo baseDir, string typeName)
        {
            if (failConvertFiles.ContainsKey(typeName))
                WriteSetToFile(baseDir, typeName + "-failConvertFiles.txt", failConvertFiles[typeName]);
            if (failOpenConvertedFiles.ContainsKey(typeName))
                WriteSetToFile(baseDir, typeName + "-failOpenConvertedFiles.txt", failOpenConvertedFiles[typeName]);
        }
        public void addToFailOpenOriginalFiles(string typeName, string fileName)
        {
            if (!failOpenOriginalFiles.ContainsKey(typeName))
                failOpenOriginalFiles.Add(typeName, new SortedSet<string> { fileName });
            else
                failOpenOriginalFiles[typeName].Add(fileName);
            if (!newFailOpenOriginalFiles.ContainsKey(typeName))
                newFailOpenOriginalFiles.Add(typeName, new SortedSet<string> { fileName });
            else
                newFailOpenOriginalFiles[typeName].Add(fileName);
        }
        public void addToPassOpenOriginalFiles(string typeName, string fileName)
        {
            if (!passOpenOriginalFiles.ContainsKey(typeName))
                passOpenOriginalFiles.Add(typeName, new SortedSet<string> { fileName });
            else
                passOpenOriginalFiles[typeName].Add(fileName);
            if (!newPassOpenOriginalFiles.ContainsKey(typeName))
                newPassOpenOriginalFiles.Add(typeName, new SortedSet<string> { fileName });
            else
                newPassOpenOriginalFiles[typeName].Add(fileName);
        }

        public void addToFailConvertFiles(string typeName, string fileName)
        {
            if (!failConvertFiles.ContainsKey(typeName))
                failConvertFiles.Add(typeName, new SortedSet<string> { fileName });
            else
                failConvertFiles[typeName].Add(fileName);
        }

        public void addToFailOpenConvertedFiles(string typeName, string fileName)
        {
            if (!failOpenConvertedFiles.ContainsKey(typeName))
                failOpenConvertedFiles.Add(typeName, new SortedSet<string> { fileName });
            else
                failOpenConvertedFiles[typeName].Add(fileName);
        }
        public bool isFailOpenOriginalFile(string typeName, string fileName)
        {
            if (!failOpenOriginalFiles.ContainsKey(typeName))
                return false;
            return failOpenOriginalFiles[typeName].Contains(fileName);
        }

        public bool isPassOpenOriginalFile(string typeName, string fileName)
        {
            if (!passOpenOriginalFiles.ContainsKey(typeName))
                return false;
            return passOpenOriginalFiles[typeName].Contains(fileName);
        }

        public int getFailOpenOriginalFilesNo(string typeName)
        {
            if (!failOpenOriginalFiles.ContainsKey(typeName))
                return 0;
            return failOpenOriginalFiles[typeName].Count;
        }
        public int getPassOpenOriginalFilesNo(string typeName)
        {
            if (!passOpenOriginalFiles.ContainsKey(typeName))
                return 0;
            return passOpenOriginalFiles[typeName].Count;
        }
        public int getFailConvertFilesNo(string typeName)
        {
            if (!failConvertFiles.ContainsKey(typeName))
                return 0;
            return failConvertFiles[typeName].Count;
        }
        public int getFailOpenConvertedFilesNo(string typeName)
        {
            if (!failOpenConvertedFiles.ContainsKey(typeName))
                return 0;
            return failOpenConvertedFiles[typeName].Count;
        }

        public void addTimeToConvert(string typeName, long timeSpanInMs)
        {
            if (!timeToConvert.ContainsKey(typeName))
                timeToConvert[typeName] = timeSpanInMs;
            else
                timeToConvert[typeName] += timeSpanInMs;
        }

        public void addTimeToOpenConvertedFiles(string typeName, long timeSpanInMs)
        {
            if (!timeToOpenConvertedFiles.ContainsKey(typeName))
                timeToOpenConvertedFiles[typeName] = timeSpanInMs;
            else
                timeToOpenConvertedFiles[typeName] += timeSpanInMs;
        }

        public long getTimeToConvert(string typeName)
        {
            if (!timeToConvert.ContainsKey(typeName))
                return 0;
            return timeToConvert[typeName];
        }

        public long getTimeToOpenConvertedFiles(string typeName)
        {
            if (!timeToOpenConvertedFiles.ContainsKey(@typeName))
                return 0;
            return timeToOpenConvertedFiles[typeName];
        }
        private static SortedSet<string> ReadFileToSet(string fileName)
        {
            SortedSet<string> set = new SortedSet<string>();
            try
            {
                using (StreamReader reader = new StreamReader(fileName))
                {
                    string line;
                    while ((line = reader.ReadLine()) != null)
                    {
                        set.Add(line.Trim());
                    }
                }
            }
            catch (FileNotFoundException)
            {
            }
            return set;
        }

        private static void WriteSetToFile(DirectoryInfo baseDir, string fileName, SortedSet<string> set)
        {
            try
            {
                using (StreamWriter writer = new StreamWriter(Path.Combine(baseDir.FullName, fileName)))
                {
                    foreach (string entry in set)
                    {
                        writer.WriteLine(entry);
                    }
                }
            }
            catch (Exception)
            {
            }
        }
    }
}
