using System;
using System.Collections.Generic;
using System.IO;

namespace mso_test
{
    internal class FileStatistics
    {

        private Dictionary<string, HashSet<string>> failOpenOriginalFiles = new Dictionary<string, HashSet<string>>();
        private Dictionary<string, HashSet<string>> passOpenOriginalFiles = new Dictionary<string, HashSet<string>>();
        private Dictionary<string, HashSet<string>> newFailOpenOriginalFiles = new Dictionary<string, HashSet<string>>();
        private Dictionary<string, HashSet<string>> newPassOpenOriginalFiles = new Dictionary<string, HashSet<string>>();
        public Dictionary<string, int> currFailOpenOriginalFilesNo = new Dictionary<string, int>();
        public Dictionary<string, int> currPassOpenOriginalFilesNo = new Dictionary<string, int>();

        private Dictionary<string, long> timeToOpenOriginalFiles = new Dictionary<string, long>();

        // Conversion might be done by multiple parallel tasks, let's lock the methods that manipulate the collections
        private Dictionary<string, HashSet<string>> failConvertFiles = new Dictionary<string, HashSet<string>>();
        private Dictionary<string, long> timeToConvert = new Dictionary<string, long>();
        private object failLock = new object();
        private object timeLock = new object();

        private Dictionary<string, HashSet<string>> failOpenConvertedFiles = new Dictionary<string, HashSet<string>>();
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

        public int getCurrFailOpenOriginalFilesNo(string typeName)
        {
            if (!currFailOpenOriginalFilesNo.ContainsKey(typeName))
                return 0;
            return currFailOpenOriginalFilesNo[typeName];
        }

        public int getCurrPassOpenOriginalFilesNo(string typeName)
        {
            if (!currPassOpenOriginalFilesNo.ContainsKey(typeName))
                return 0;
            return currPassOpenOriginalFilesNo[typeName];
        }
        public void incrCurrFailOpenOriginalFilesNo(string typeName)
        {
            if (!currFailOpenOriginalFilesNo.ContainsKey(typeName))
                currFailOpenOriginalFilesNo.Add(typeName, 1);
            else
                currFailOpenOriginalFilesNo[typeName]++;
        }

        public void incrCurrPassOpenOriginalFilesNo(string typeName)
        {
            if (!currPassOpenOriginalFilesNo.ContainsKey(typeName))
                currPassOpenOriginalFilesNo.Add(typeName, 1);
            else
                currPassOpenOriginalFilesNo[typeName]++;
        }

        public void addToFailOpenOriginalFiles(string typeName, string fileName)
        {
            if (!failOpenOriginalFiles.ContainsKey(typeName))
                failOpenOriginalFiles.Add(typeName, new HashSet<string> { fileName });
            else
                failOpenOriginalFiles[typeName].Add(fileName);
            if (!newFailOpenOriginalFiles.ContainsKey(typeName))
                newFailOpenOriginalFiles.Add(typeName, new HashSet<string> { fileName });
            else
                newFailOpenOriginalFiles[typeName].Add(fileName);
        }
        public void addToPassOpenOriginalFiles(string typeName, string fileName)
        {
            if (!passOpenOriginalFiles.ContainsKey(typeName))
                passOpenOriginalFiles.Add(typeName, new HashSet<string> { fileName });
            else
                passOpenOriginalFiles[typeName].Add(fileName);
            if (!newPassOpenOriginalFiles.ContainsKey(typeName))
                newPassOpenOriginalFiles.Add(typeName, new HashSet<string> { fileName });
            else
                newPassOpenOriginalFiles[typeName].Add(fileName);
        }

        public void addToFailConvertFiles(string typeName, string fileName)
        {
            lock (failLock)
            {
                if (!failConvertFiles.ContainsKey(typeName))
                    failConvertFiles.Add(typeName, new HashSet<string> { fileName });
                else
                    failConvertFiles[typeName].Add(fileName);
            }
        }

        public void addToFailOpenConvertedFiles(string typeName, string fileName)
        {
            if (!failOpenConvertedFiles.ContainsKey(typeName))
                failOpenConvertedFiles.Add(typeName, new HashSet<string> { fileName });
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

        public void addTimeToOpenOriginalFiles(string typeName, long timeSpanInMs)
        {
            if (!timeToOpenOriginalFiles.ContainsKey(typeName))
                timeToOpenOriginalFiles[typeName] = timeSpanInMs;
            else
                timeToOpenOriginalFiles[typeName] += timeSpanInMs;
        }

        public void addTimeToConvert(string typeName, long timeSpanInMs)
        {
            lock(timeLock)
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

        public long getTimeToOpenOriginalFiles(string typeName)
        {
            if (!timeToOpenOriginalFiles.ContainsKey(typeName))
                return 0;
            return timeToOpenOriginalFiles[typeName];
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
        public static HashSet<string> ReadFileToSet(string fileName)
        {
            HashSet<string> set = new HashSet<string>();
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

        public static void WriteSetToFile(DirectoryInfo baseDir, string fileName, HashSet<string> hashed)
        {
            try
            {
                SortedSet<string> sorted = new SortedSet<string>(hashed);
                using (StreamWriter writer = new StreamWriter(Path.Combine(baseDir.FullName, fileName)))
                {
                    foreach (string entry in sorted)
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
