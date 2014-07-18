###############################################################################
# taskwarrior - a command line task list manager.
#
# Copyright 2006-2014, Paul Beckingham, Federico Hernandez.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
# http://www.opensource.org/licenses/mit-license.php
#
###############################################################################

# Original version by Renato Alves

import sys
import unittest
import warnings


class TAPTestResult(unittest.result.TestResult):
    def __init__(self, stream, descriptions, verbosity):
        super(TAPTestResult, self).__init__(stream, descriptions, verbosity)
        self.stream = stream
        self.descriptions = descriptions
        self.verbosity = verbosity
        # Buffer stdout and stderr
        self.buffer = True

    def getDescription(self, test):
        doc_first_line = test.shortDescription()
        if self.descriptions and doc_first_line:
            return doc_first_line
        else:
            return str(test)

    def startTestRun(self, total="unk"):
        self.stream.writeln("1..{0}".format(total))

    def stopTest(self, test):
        """Prevent flushing of stdout/stderr buffers until later"""
        pass

    def _restoreStdout(self):
        """Restore sys.stdout and sys.stderr, don't merge buffered output yet
        """
        if self.buffer:
            sys.stdout = self._original_stdout
            sys.stderr = self._original_stderr

    @staticmethod
    def _do_stream(data, stream):
        """Helper function for _mergeStdout"""
        for line in data.splitlines(True):
            # Add a comment sign before each line
            if line.startswith("#"):
                stream.write(line)
            else:
                stream.write("# " + line)

        if not line.endswith('\n'):
            stream.write('\n')

    def _mergeStdout(self):
        """Merge buffered output with main streams
        """

        if self.buffer:
            output = self._stdout_buffer.getvalue()
            error = self._stderr_buffer.getvalue()
            if output:
                self._do_stream(output, sys.stdout)
            if error:
                self._do_stream(error, sys.stderr)

            self._stdout_buffer.seek(0)
            self._stdout_buffer.truncate()
            self._stderr_buffer.seek(0)
            self._stderr_buffer.truncate()

        # Needed to fix the stopTest override
        self._mirrorOutput = False

    def report(self, test, status=None, err=None):
        # Restore stdout/stderr but don't flush just yet
        self._restoreStdout()

        desc = self.getDescription(test)
        try:
            exception, msg, _ = err
        except (TypeError, ValueError):
            exception = ""
            msg = err
        else:
            exception = exception.__name__
            msg = str(msg)

        if status:
            if status == "SKIP":
                self.stream.writeln("skip {0} - {1}".format(self.testsRun,
                                                            desc))
            else:
                self.stream.writeln("not ok {0} - {1}".format(self.testsRun,
                                                              desc))
            self.stream.writeln("# {0}: {1}".format(status, exception))
            padding = " " * (len(status) + 3)
            for line in msg.splitlines():
                # Force displaying new-line characters as literal new lines
                line = line.replace("\\n", "\n# ")
                self.stream.writeln("#{0}{1}".format(padding, line))
        else:
            self.stream.writeln("ok {0} - {1}".format(self.testsRun, desc))

        # Flush all buffers to stdout
        self._mergeStdout()

    def addSuccess(self, test):
        super(TAPTestResult, self).addSuccess(test)
        self.report(test)

    def addError(self, test, err):
        super(TAPTestResult, self).addError(test, err)
        self.report(test, "ERROR", err)

    def addFailure(self, test, err):
        super(TAPTestResult, self).addFailure(test, err)
        self.report(test, "FAIL", err)

    def addSkip(self, test, reason):
        super(TAPTestResult, self).addSkip(test, reason)
        self.report(test, "SKIP", reason)


class TAPTestRunner(unittest.runner.TextTestRunner):
    """A test runner that displays results using the Test Anything Protocol
    syntax.

    Inherits from TextTestRunner the default runner.
    """
    resultclass = TAPTestResult

    def run(self, test):
        result = self._makeResult()
        unittest.signals.registerResult(result)
        result.failfast = self.failfast

        with warnings.catch_warnings():
            if getattr(self, "warnings", None):
                # if self.warnings is set, use it to filter all the warnings
                warnings.simplefilter(self.warnings)
                # if the filter is 'default' or 'always', special-case the
                # warnings from the deprecated unittest methods to show them
                # no more than once per module, because they can be fairly
                # noisy.  The -Wd and -Wa flags can be used to bypass this
                # only when self.warnings is None.
                if self.warnings in ['default', 'always']:
                    warnings.filterwarnings(
                        'module',
                        category=DeprecationWarning,
                        message='Please use assert\w+ instead.')
            startTestRun = getattr(result, 'startTestRun', None)
            if startTestRun is not None:
                startTestRun(test.countTestCases())
            try:
                test(result)
            finally:
                stopTestRun = getattr(result, 'stopTestRun', None)
                if stopTestRun is not None:
                    stopTestRun()

        return result
