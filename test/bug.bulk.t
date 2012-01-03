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
use Test::More tests => 12;

# Create the rc file.
if (open my $fh, '>', 'bulk.rc')
{
  print $fh "data.location=.\n",
            "confirmation=yes\n",
            "bulk=3\n";
  close $fh;
  ok (-r 'bulk.rc', 'Created bulk.rc');
}

# Add some tasks with project, prioriy and due date, some with only due date.
# Bulk add a project and priority to the tasks that were without.
qx{../src/task rc:bulk.rc add t1 pro:p1 pri:H due:monday};
qx{../src/task rc:bulk.rc add t2 pro:p1 pri:M due:tuesday};
qx{../src/task rc:bulk.rc add t3 pro:p1 pri:L due:wednesday};
qx{../src/task rc:bulk.rc add t4              due:thursday};
qx{../src/task rc:bulk.rc add t5              due:friday};
qx{../src/task rc:bulk.rc add t6              due:saturday};

my $output = qx{echo "-- quit"|../src/task rc:bulk.rc 4 5 6  modify pro:p1 pri:M};
like ($output, qr/Modified 0 tasks/, '"quit" prevents any further modifications');

$output = qx{echo "-- All"|../src/task rc:bulk.rc 4 5 6  mod pro:p1 pri:M};
unlike ($output, qr/Task 4 "t4"\n  - No changes were made/, 'Task 4 modified');
unlike ($output, qr/Task 5 "t5"\n  - No changes were made/, 'Task 5 modified');
unlike ($output, qr/Task 6 "t6"\n  - No changes were made/, 'Task 6 modified');

$output = qx{../src/task rc:bulk.rc info 4};
like ($output, qr/Project\s+p1/, 'project applied to 4');
like ($output, qr/Priority\s+M/, 'priority applied to 4');

$output = qx{../src/task rc:bulk.rc info 5};
like ($output, qr/Project\s+p1/, 'project applied to 5');
like ($output, qr/Priority\s+M/, 'priority applied to 5');

$output = qx{../src/task rc:bulk.rc info 6};
like ($output, qr/Project\s+p1/, 'project applied to 6');
like ($output, qr/Priority\s+M/, 'priority applied to 6');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data synch.key bulk.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'synch.key'      &&
    ! -r 'bulk.rc', 'Cleanup');

exit 0;

