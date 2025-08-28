#!/usr/bin/env python
# -*- tab-width: 4; indent-tabs-mode: nil; py-indent-offset: 4 -*-
#
# Copyright the mso-test contributors.
#
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

import os
import json
import subprocess
import sys
from pathlib import Path

bibisect_repo_dir = ''
mso_test_dir = ''
test_file_base_dir = ''
range_from = ''
range_to = ''
file_list_full_name = ''
result_file_name = ''

# Perform reverse bibisect, consider opposite of mso-test results
reverse = False

special_git_names = { 'oldest', 'main', 'master' }

extension_app_map = {
    'docx' : 'word',
    'doc'  : 'word',
    'odt'  : 'word',
    'xlsx' : 'excel',
    'xls'  : 'excel',
    'ods'  : 'excel',
    'pptx' : 'powerpoint',
    'ppt'  : 'powerpoint',
    'odp'  : 'powerpoint'
}


def read_config():
    global bibisect_repo_dir, mso_test_dir, test_file_base_dir, range_from, range_to, file_list_full_name, result_file_name, reverse
    with open('config.json', 'r') as config:
        json_config = json.load(config)
    bibisect_repo_dir = json_config['BibisectRepoDir']
    mso_test_dir = json_config['MsoTestDir']
    test_file_base_dir = json_config['TestFileBaseDir']
    range_from = json_config['RangeFrom']
    range_to = json_config['RangeTo']
    file_list_full_name = json_config['FileListFullName']
    result_file_name = json_config['ResultFileName']
    reverse = json_config['Reverse']

def write_set_to_file(full_file_name, set_of_names):
    if not bool(set_of_names):
        return
    with open(full_file_name, 'w') as file:
        for name in sorted(set_of_names):
            file.write(name + '\n')

def read_file_into_set(full_file_name):
    try:
        with open(full_file_name, 'r') as file:
            result_set = {line.strip() for line in file if line.strip()}
    except FileNotFoundError:
        result_set = set()
    return result_set

def has_git_main():
    os.chdir(bibisect_repo_dir)
    if 0 == subprocess.run(["git", "rev-parse", "--verify", "main"], capture_output=True, text=True).returncode:
        return True
    return False

def adjust_range_hash(original_commit_hash):
    os.chdir(bibisect_repo_dir)
    if original_commit_hash not in special_git_names:
        return original_commit_hash
    # main or master
    if original_commit_hash == "master" and has_git_main():
        original_commit_hash = "main"
    elif original_commit_hash == "main" and not has_git_main():
        original_commit_hash = "master"

    result = subprocess.run(["git", "rev-parse", "--verify", original_commit_hash], capture_output=True, text=True, check=True)
    return get_original_hash(result.stdout.strip())

def adjust_range_hashes(range_from, range_to):
    return (adjust_range_hash(range_from), adjust_range_hash(range_to))

def get_bibisect_hash(original_commit_hash):
    os.chdir(bibisect_repo_dir)
    result = subprocess.run(["git", "log", "--all", "--grep=" + original_commit_hash, "--format=%H"], capture_output=True, text=True, check=True)
    result_hash = result.stdout.strip()
    return result_hash

# this can only handle bibisect repos where the subject line contains the hashes in the format 'source sha:<hash>'
def get_original_hash(bibisect_commit_hash):
    os.chdir(bibisect_repo_dir)
    result = subprocess.run(["git", "log", "-1", "--pretty=%s", bibisect_commit_hash], capture_output=True, text=True, check=True)
    return result.stdout.strip()[11:]

def git_checkout_commit(original_commit_hash):
    os.chdir(bibisect_repo_dir)
    if original_commit_hash not in special_git_names:
        commit_hash = get_bibisect_hash(original_commit_hash)
    else:
        commit_hash = original_commit_hash
    subprocess.run(["git", "checkout", "-f", commit_hash], capture_output=True, text=True, check=True)

def git_checkout_previous_commit():
    os.chdir(bibisect_repo_dir)
    subprocess.run(["git", "checkout", "-f", "HEAD~1"], capture_output=True, text=True, check=True)

def git_bisect_start(to_commit, from_commit):
    os.chdir(bibisect_repo_dir)
    from_bibisect_hash = get_bibisect_hash(from_commit)
    to_bibisect_hash = get_bibisect_hash(to_commit)
    subprocess.run(["git", "bisect", "start", to_bibisect_hash, from_bibisect_hash])

def git_bisect_before():
    os.chdir(bibisect_repo_dir)
    subprocess.run(["git", "bisect", "bad"])

def git_bisect_after():
    os.chdir(bibisect_repo_dir)
    subprocess.run(["git", "bisect", "good"])

def is_git_bisect_done():
    if not os.path.exists(git_bisect_log_path):
        raise Exception("ERROR: Trying to check if bisect is done, but not in bisect now")
    with open(git_bisect_log_path, 'r') as bisect_log:
        return any("first bad commit" in line for line in bisect_log)

def get_git_bisect_result_hash():
    if not os.path.exists(git_bisect_log_path):
        raise Exception("ERROR: Trying to find bisect result, but not in bisect now")
    with open(git_bisect_log_path, 'r') as bisect_log_file:
        found_line = next((line for line in bisect_log_file if "first bad commit" in line), None)
        if not found_line:
            raise Exception("ERROR: Couldn't find \"first bad commit\" in bisect result")
        return found_line.split("source sha:")[1].strip()
    

def write_results(result_full_name, reverse, range_from, range_to, start_set, end_set, result_dict, failconvert_result_set):
    print()
    print("Writing results to file...")
    with open(result_full_name, 'w') as file:
        qualifier_good = "good" if not reverse else "bad"
        qualifier_bad = "bad" if not reverse else "good"
        if (not reverse):
            file.write("Autobibisect results for range: " + range_from + " - " + range_to + "\n")
        else:
            file.write("Reverse autobibisect results for range: " + range_from + " - " + range_to + "\n")
        file.write("\n" + str(len(start_set)) + " file(s) are already " + qualifier_bad + " at the start of the range:\n")
        for name in sorted(start_set):
            file.write(name + '\n')
        if not bool(start_set):
            file.write("None\n")
        file.write("\n" + str(len(end_set)) + " file(s) are still " + qualifier_good + " at the end of the range:\n")
        for name in sorted(end_set):
            file.write(name + '\n')
        if not bool(end_set):
            file.write("None\n")
        for commit_hash in result_dict:
            file.write("\n" + str(len(result_dict[commit_hash])) + " file(s) started becoming " + qualifier_bad + " at commit " + str(commit_hash) + "\n")
            for name in sorted(result_dict[commit_hash]):
                file.write(name + '\n')
        if bool(failconvert_result_set):
            file.write("\n" + str(len(failconvert_result_set)) + " file(s) that failed to convert at some point :\n")
            for name in sorted(failconvert_result_set):
                file.write(name + '\n')

# returns tuple of (failconvert, failopenconverted) sets
def execute_mso_test(file_extenstion):
    os.chdir(mso_test_dir)
    full_failconvert_name = os.path.join(test_file_base_dir, file_extension + failconvert_suffix)
    if os.path.isfile(full_failconvert_name):
        os.remove(full_failconvert_name)
    full_failopenconverted_name = os.path.join(test_file_base_dir, file_extension + failopenconverted_suffix)
    if os.path.isfile(full_failopenconverted_name):
        os.remove(full_failopenconverted_name)
    subprocess.run(["./mso-test.exe", extension_app_map[file_extension]], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    failconvert_set = read_file_into_set(full_failconvert_name)
    failopenconverted_set = read_file_into_set(full_failopenconverted_name)
    return (failconvert_set, failopenconverted_set)

# returns tuple of success (bool), hash (string)
def run_bibisect(file_name, reverse, range_from, range_to):
    file_extension = Path(file_name).suffix.lstrip('.')
    fail_convert_name = file_extension + failconvert_suffix
    fail_convert_full_name = os.path.join(test_file_base_dir, fail_convert_name)
    fail_convert_open_name = file_extension + failopenconverted_suffix
    fail_convert_open_full_name = os.path.join(test_file_base_dir, fail_convert_open_name)
    write_set_to_file(filestotest_path, {file_name})
    print()
    print("Starting git bisect between " + range_from + " and " + range_to + " for file " + file_name)
    git_bisect_start(range_to, range_from)
    print("Bisecting using mso-test on 1 test file: " + file_name)
    while not is_git_bisect_done():
        failconvert_set, failopenconverted_set = execute_mso_test(file_extension)
        if bool(failconvert_set):
            print("Failed to convert " + file_name)
            return (False, '')
        # failed and not reverse or succeeded and reverse
        if os.path.exists(fail_convert_open_full_name) != reverse:
            print("Chose git bisect bad")
            git_bisect_before()
        else:
            print("Chose git bisect good")
            git_bisect_after()
    print("Git bisect result: " + get_git_bisect_result_hash())
    return (True, get_git_bisect_result_hash())

read_config()
result_full_name = os.path.join(test_file_base_dir, result_file_name)
git_bisect_log_path = os.path.join(bibisect_repo_dir, ".git", "BISECT_LOG")
filestoskip_path = os.path.join(test_file_base_dir, 'filestoskip.txt')
filestotest_path = os.path.join(test_file_base_dir, 'filestotest.txt')
failconvert_suffix = '-failConvertFiles.txt'
failopenconverted_suffix = '-failOpenConvertedFiles.txt'

range_from, range_to = adjust_range_hashes(range_from, range_to)
range_to_bibisect_hash = get_bibisect_hash(range_to)
range_from_bibisect_hash = get_bibisect_hash(range_from)
if not range_to_bibisect_hash:
    print(range_to + " (<range_to>) commit not found in bibisect repository")
    sys.exit(1)
if not range_from_bibisect_hash:
    print(range_from + "(<range_from>) commit not found in bibisect repository")
    sys.exit(1)

# Only manipulate filestotest / filestoskip
if os.path.isdir(filestoskip_path) or os.path.isdir(filestotest_path):
    print("filestoskip_path (" + filestoskip_path + ") or filestotest_path (" + filestotest_path + ") is not a single file, exiting")
    sys.exit(1)
if os.path.isfile(filestoskip_path):
    os.remove(filestoskip_path)
if os.path.isfile(filestotest_path):
    os.remove(filestotest_path)

remaining_files = read_file_into_set(file_list_full_name)
if not bool(remaining_files):
    print("List of files empty or not found")
    sys.exit(1)

file_extension = Path(next(iter(remaining_files))).suffix.lstrip('.')

# result lists: start, end, list at commit hashes, conversion failures
start_set = set()
end_set = set()
result_dict = dict()
failconvert_result_set = set()

print("Starting autobibisect...")
print()
print("Checking commit at the end of range: " + range_to)
write_set_to_file(filestotest_path, remaining_files)
git_checkout_commit(range_to)
print("Executing mso-test on " + str(len(remaining_files)) + " test file(s)")
failconvert_set, failopenconverted_set = execute_mso_test(file_extension)
print(str(len(failconvert_set)) + " file(s) failed to convert and " + str(len(failopenconverted_set)) + " file(s) failed to open after conversion")
failconvert_result_set |= failconvert_set
remaining_files -= failconvert_set
# end_set = files that are still OK at the end (or still bad at the end in reverse case)
if not reverse:
    end_set = remaining_files - failopenconverted_set
    # normal bibisect: eliminate files still OK at the end (keep failing ones)
    remaining_files &= failopenconverted_set
else:
    end_set = failopenconverted_set
    # reverse bibisect: eliminate files still failing at the end (keep good ones)
    remaining_files -= failopenconverted_set

if len(remaining_files) > 0:
    print()
    print("Checking commit at the beginning of range: " + range_from)
    git_checkout_commit(range_from)
    print("Executing mso-test on " + str(len(remaining_files)) + " test file(s)")
    failconvert_set, failopenconverted_set = execute_mso_test(file_extension)
    print(str(len(failconvert_set)) + " file(s) failed to convert and " + str(len(failopenconverted_set)) + " file(s) failed to open after conversion")
    failconvert_result_set |= failconvert_set
    remaining_files -= failconvert_set
    if not reverse:
        start_set = failopenconverted_set
        # normal bibisect: eliminate files already failing at the beginning (keep good ones)
        remaining_files -= failopenconverted_set
    else:
        start_set = remaining_files - failopenconverted_set
        # reverse bibisect: eliminate files already OK at the beginning (keep failing ones)
        remaining_files &= failopenconverted_set

while len(remaining_files) > 0:
    print(str(len(remaining_files)) + " file(s) remaining")
    elem = remaining_files.pop()
    set_to_add = { elem }
    success, git_hash = run_bibisect(elem, reverse, range_from, range_to)
    # unexpected conversion failure
    if not success:
        print("Conversion failure during bisecting, moving on")
        failconvert_result_set |= set_to_add
        continue
    
    if len(remaining_files) > 0:
        print("Testing remaining " + str(len(remaining_files)) + " file(s) at found and preceding hash to find if multiple files were affected by the commit")
        write_set_to_file(filestotest_path, remaining_files)
        git_checkout_commit(git_hash)
        failconvert_set, failopenconverted_set = execute_mso_test(file_extension)
        git_checkout_previous_commit()
        prev_failconvert_set, prev_failopenconverted_set = execute_mso_test(file_extension)
        failconvert_result_set |= failconvert_set
        remaining_files -= failconvert_set
        failconvert_result_set |= prev_failconvert_set
        remaining_files -= prev_failconvert_set

        if not reverse:
            set_to_add |= (failopenconverted_set - prev_failopenconverted_set)
        else:
            set_to_add |= (prev_failopenconverted_set - failopenconverted_set)

    print(str(len(set_to_add)) + " file(s) affected by this commit altogether")
    result_dict[git_hash] = set_to_add
    print("Eliminated " + str(len(set_to_add)) + " file(s)")
    remaining_files -= set_to_add

write_results(result_full_name, reverse, range_from, range_to, start_set, end_set, result_dict, failconvert_result_set)
print("Done")

if has_git_main():
    git_checkout_commit("main")
else:
    git_checkout_commit("master")
