#! /usr/bin/env perl
################################################################################
## taskwarrior - a command line task list manager.
##
## Copyright 2006-2013, Paul Beckingham, Federico Hernandez.
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
use Test::More tests => 5;

# Create the rc file.
if (open my $fh, '>', 'due.rc')
{
  print $fh "data.location=.\n",
            "due=4\n";
  close $fh;
  ok (-r 'due.rc', 'Created due.rc');
}

# Add an overdue task, a due task, and a regular task.  The "overdue" report
# should list only the one task.
qx{../src/task rc:due.rc add due:yesterday one 2>&1};
qx{../src/task rc:due.rc add due:tomorrow two 2>&1};
qx{../src/task rc:due.rc add due:30d three 2>&1};
my $output = qx{../src/task rc:due.rc overdue 2>&1};
like   ($output, qr/one/,   'overdue: task 1 shows up');
unlike ($output, qr/two/,   'overdue: task 2 does not show up');
unlike ($output, qr/three/, 'overdue: task 3 does not show up');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data synch.key due.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'synch.key'      &&
    ! -r 'due.rc', 'Cleanup');

exit 0;

