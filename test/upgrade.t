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
use Test::More tests => 7;

# Ensure environment has no influence.
delete $ENV{'TASKDATA'};
delete $ENV{'TASKRC'};

# Create the rc file.
if (open my $fh, '>', 'upgrade.rc')
{
  print $fh "data.location=.\n",
            "confirmation=off\n";
  close $fh;
  ok (-r 'upgrade.rc', 'Created upgrade.rc');
}

# Add a plain task, then upgrade to recurring, test for correctness.
qx{../src/task rc:upgrade.rc add one 2>&1};
my $output = qx{../src/task rc:upgrade.rc 1 info 2>&1};
like ($output, qr/Status\s+Pending/,    'Plain task entered');

qx{../src/task rc:upgrade.rc 1 modify due:tomorrow recur:weekly 2>&1};
qx{../src/task rc:upgrade.rc list 2>&1};
$output = qx{../src/task rc:upgrade.rc 1 info 2>&1};
like ($output, qr/Status\s+Recurring/,  'Upgraded parent: good status');
like ($output, qr/Recurrence\s+weekly/, 'Upgraded parent: good recurrence');

$output = qx{../src/task rc:upgrade.rc 2 info 2>&1};
like ($output, qr/Status\s+Pending/,    'Upgraded child: good status');
like ($output, qr/Recurrence\s+weekly/, 'Upgraded child: good recurrence');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data upgrade.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'upgrade.rc', 'Cleanup');

exit 0;

