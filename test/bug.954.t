#! /usr/bin/env perl
################################################################################
##
## Copyright 2006 - 2014, Paul Beckingham, Federico Hernandez.
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to deal
## in the Software without restriction, including without limitation the rights
## to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
## copies of the Software, and to permit persons to whom the Software is
## furnished to do so, subject to the following conditions:
##
## The above copyright notice and this permission notice shall be included
## in all copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
## OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
## THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
## OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
## SOFTWARE.
##
## http://www.opensource.org/licenses/mit-license.php
##
################################################################################

use strict;
use warnings;
use Test::More tests => 4;

# Ensure environment has no influence.
delete $ENV{'TASKDATA'};
delete $ENV{'TASKRC'};

# Create the rc file.
if (open my $fh, '>', 'bug.rc')
{
  print $fh "data.location=.\n",
            "confirmation=off\n";
  close $fh;
}

# Bug 954 - Deleting task by uuid attempts deleting all tasks
# - adding two tasks "foo" and "bar"
# - deleting task with UUID 874e146d-07a2-2d2c-7808-a76e74b1a332
# - searching for tasks "foo" and "bar"

qx{../src/task rc:bug.rc add foo 2>&1};
qx{../src/task rc:bug.rc add bar 2>&1};
my $output = qx{../src/task rc:bug.rc list 2>&1};
like ($output, qr/foo/ms, 'Task foo added');
like ($output, qr/bar/ms, 'Task bar added');
qx{../src/task rc:bug.rc rc.confirmation=off rc.verbose=nothing rc.bulk=1000 874e146d-07a2-2d2c-7808-a76e74b1a332 delete 2>&1};

$output = qx{../src/task rc:bug.rc list 2>&1};
like ($output, qr/foo/ms, 'Task foo not deleted');
like ($output, qr/bar/ms, 'Task bar not deleted');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data bug.rc);
exit 0;
