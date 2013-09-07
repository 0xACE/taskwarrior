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
use Test::More tests => 8;

# Ensure environment has no influence.
delete $ENV{'TASKDATA'};
delete $ENV{'TASKRC'};

# Create the rc file.
if (open my $fh, '>', 'uda.rc')
{
  print $fh "data.location=.\n",
            "confirmation=off\n",
            "uda.smell.type=string\n",
            "uda.smell.label=Smell\n",
            "uda.smell.values=weak,strong\n",
            "uda.smell.default=weak\n",
            "uda.size.type=numeric\n",
            "uda.size.label=Size\n",
            "report.uda.description=UDA Test\n",
            "report.uda.columns=id,smell,size,description\n",
            "report.uda.sort=id\n",
            "report.uda.labels=ID,Smell,Size,Description\n";
  close $fh;
  ok (-r 'uda.rc', 'Created uda.rc');
}

# Add task with nondefault UDA
my $output = qx{../src/task rc:uda.rc add one smell:strong 2>&1};
like ($output, qr/Created task 1/, 'Add 1 - no errors');

# Add task without a UDA value, checking for usage of the default
$output = qx{../src/task rc:uda.rc add two 2>&1};
like ($output, qr/Created task 2/, 'Add 2 - no errors');

# Add a task with a UDA that has no default, ensure it is entered fine
$output = qx{../src/task rc:uda.rc add three size:10 2>&1};
like ($output, qr/Created task 3/, 'Add 3 - no errors');

$output = qx{../src/task rc:uda.rc uda 2>&1};
like ($output, qr/1\s+strong\s+one/,          'UDA nondefault stored');
like ($output, qr/2\s+weak\s+two/,            'UDA default stored');
like ($output, qr/3\s+weak\s+10\s+three/,     'UDA without default stored');


# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data uda.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'uda.rc', 'Cleanup');

exit 0;

