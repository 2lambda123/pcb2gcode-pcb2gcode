#!/usr/bin/python

import unittest
import subprocess
import os
import tempfile
import shutil
import difflib
import filecmp
import sys
import argparse
import re
import collections

TestCase = collections.namedtuple("TestCase", ["input_path", "args", "exit_code"])

EXAMPLES_PATH = "testing/gerbv_example"
TEST_CASES = ([TestCase(os.path.join(EXAMPLES_PATH, x), [], 0)
              for x in [
                  "multivibrator",
                  "am-test-voronoi",
                  "slots-milldrill",
                  "multivibrator_xy_offset",
              ]] +
              [TestCase(os.path.join(EXAMPLES_PATH, "multivibrator"), ["--front=non_existant_file"], 1),
               TestCase(os.path.join(EXAMPLES_PATH, "multivibrator"), ["--back=non_existant_file"], 1),
               TestCase(os.path.join(EXAMPLES_PATH, "multivibrator"), ["--outline=non_exsistant_file"], 1),
              ])

class IntegrationTests(unittest.TestCase):

  def pcb2gcode_one_directory(self, input_path, args=[], exit_code=0):
    """Run pcb2gcode once in one directory.

    Current working directory remains unchanged at the end.

    input_path: Where to run pcb2gcode
    Returns the path to the output files created.
    """
    cwd = os.getcwd()
    pcb2gcode = os.path.join(cwd, "pcb2gcode")
    os.chdir(input_path)
    actual_output_path = tempfile.mkdtemp()
    self.assertEqual(
        subprocess.call([pcb2gcode, "--output-dir", actual_output_path] + args),
        exit_code)
    os.chdir(cwd)
    return actual_output_path

  def compare_directories(self, left, right, left_prefix="", right_prefix=""):
    """Compares two directories.

    Returns a string representing the diff between them if there is
    any difference between them.  If there is no difference between
    them, returns an empty string.
    left: Path to left side of diff
    right: Path to right side of diff
    left_prefix: String to prepend to all left-side files
    right_prefix: String to prepend to all right-side files
    """

    # Right side might not exist.
    if not os.path.exists(right):
      all_diffs = []
      for f in os.listdir(left):
        all_diffs += "Found %s but not %s.\n" % (os.path.join(left_prefix, f), os.path.join(right_prefix, f))
        left_file = os.path.join(left, f)
        with open(left_file, 'r') as myfile:
          data=myfile.readlines()
          all_diffs += difflib.unified_diff(data, [], os.path.join(left_prefix, f), "/dev/null")
      return ''.join(all_diffs)

    # Left side might not exist.
    if not os.path.exists(left):
      all_diffs = []
      for f in os.listdir(right):
        all_diffs += "Found %s but not %s.\n" % (os.path.join(right_prefix, f), os.path.join(left_prefix, f))
        right_file = os.path.join(right, f)
        with open(right_file, 'r') as myfile:
          data=myfile.readlines()
          all_diffs += difflib.unified_diff([], data, "/dev/null", os.path.join(right_prefix, f))
      return ''.join(all_diffs)


    diff = filecmp.dircmp(left, right)
    # Now compare all the differing files.
    all_diffs = []
    for f in diff.left_only:
      all_diffs += "Found %s but not %s.\n" % (os.path.join(left_prefix, f), os.path.join(right_prefix, f))
      left_file = os.path.join(left, f)
      with open(left_file, 'r') as myfile:
        data=myfile.readlines()
        all_diffs += difflib.unified_diff(data, [], os.path.join(left_prefix, f), "/dev/null")
    for f in diff.right_only:
      all_diffs += "Found %s but not %s.\n" % (os.path.join(right_prefix, f), os.path.join(left_prefix, f))
      right_file = os.path.join(right, f)
      with open(right_file, 'r') as myfile:
        data=myfile.readlines()
        all_diffs += difflib.unified_diff([], data, "/dev/null", os.path.join(right_prefix, f))
    for f in diff.diff_files:
      left_file = os.path.join(left, f)
      right_file = os.path.join(right, f)
      with open(left_file, 'r') as myfile0, open(right_file, 'r') as myfile1:
        data0=myfile0.readlines()
        data1=myfile1.readlines()
        all_diffs += difflib.unified_diff(data0, data1, os.path.join(left_prefix, f), os.path.join(right_prefix, f))
    return ''.join(all_diffs)

  def run_one_directory(self, input_path, expected_output_path, test_prefix, args=[], exit_code=0):
    """Run pcb2gcode on a directory and return the diff as a string.

    Returns an empty string if there is no mismatch.
    Returns the diff if there is a mismatch.
    input_path: Path to inputs
    expected_output_path: Path to expected outputs
    test_prefix: Strin to prepend to all filenamess
    """
    actual_output_path = self.pcb2gcode_one_directory(input_path, args, exit_code)
    if exit_code:
      return ""
    diff_text = self.compare_directories(expected_output_path, actual_output_path,
                                         os.path.join("expected", test_prefix),
                                         os.path.join("actual", test_prefix))
    shutil.rmtree(actual_output_path)
    return diff_text

  def test_all(self):
    cwd = os.getcwd()
    test_cases = TEST_CASES
    diff_texts = []
    for test_case in test_cases:
      test_prefix = os.path.join(test_case.input_path, "expected")
      input_path = os.path.join(cwd, test_case.input_path)
      expected_output_path = os.path.join(cwd, test_case.input_path, "expected")
      diff_texts.append(self.run_one_directory(input_path, expected_output_path, test_prefix, test_case.args, test_case.exit_code))
    self.assertFalse(any(diff_texts),
                     'Files don\'t match\n' + '\n'.join(diff_texts) +
                     '\n***\nRun one of these:\n' +
                     './integration_tests.py --fix\n' +
                     './integration_tests.py --fix --add\n' +
                     '***\n')

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Integration test of pcb2gcode.')
  parser.add_argument('--fix', action='store_true', default=False,
                      help='Generate expected outputs automatically')
  parser.add_argument('--add', action='store_true', default=False,
                      help='git add new expected outputs automatically')
  args = parser.parse_args()
  if args.fix:
    print("Generating expected outputs...")
    output = None
    try:
      subprocess.check_output([sys.argv[0]], stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError, e:
      output = str(e.output)
    if not output:
      print("No diffs, nothing to do.")
      exit(0)
    p = subprocess.Popen(["patch", "-p1"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    result = p.communicate(input=output)
    files_patched = []
    for l in result[0].split('\n'):
      if l.startswith("patching file "):
        files_patched.append(l[len("patching file "):])
    if args.add:
      subprocess.check_output(["git", "add"] + files_patched)
      print("Done.\nAdded to git:\n" +
            '\n'.join(files_patched))
    else:
      print("Done.\nYou now need to run:\n" +
            '\n'.join('git add ' + x for x in files_patched))
  else:
    unittest.main()
