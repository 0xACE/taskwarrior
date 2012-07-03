#! /usr/bin/perl
################################################################################
## taskwarrior - a command line task list manager.
##
## Copyright 2006-2012, Paul Beckingham, Federico Hernandez.
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

# Create the rc file.
if (open my $fh, '>', 'datesort.rc')
{
  print $fh "data.location=.\n",
            "dateformat=YMD\n",
            "report.small_list.description=Small list\n",
            "report.small_list.columns=due,description\n",
            "report.small_list.labels=Due,Description\n",
            "report.small_list.sort=due+\n",
            "report.small_list.filter=status:pending\n",
            "report.small_list.dateformat=MD\n";

  close $fh;
  ok (-r 'datesort.rc', 'Created datesort.rc');
}

qx{../src/task rc:datesort.rc add two   due:20100201 2>&1};
qx{../src/task rc:datesort.rc add one   due:20100101 2>&1};
qx{../src/task rc:datesort.rc add three due:20100301 2>&1};

my $output = qx{../src/task rc:datesort.rc small_list 2>&1};
like ($output, qr/one.+two.+three/ms, 'Sorting by due+ with format MD works');

$output = qx{../src/task rc:datesort.rc rc.report.small_list.sort=due- small_list 2>&1};
like ($output, qr/three.+two.+one/ms, 'Sorting by due- with format MD works');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data synch.key datesort.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'synch.key'      &&
    ! -r 'datesort.rc', 'Cleanup');

exit 0;

