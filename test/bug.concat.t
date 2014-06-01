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
use Test::More tests => 3;

# Ensure environment has no influence.
delete $ENV{'TASKDATA'};
delete $ENV{'TASKRC'};

# Create the rc file.
if (open my $fh, '>', 'bug.rc')
{
  print $fh "data.location=.\n",
            "confirmation=no\n";
  close $fh;
}

# When a task is modified like this:
#
#   % task 1 modify This is a new description
#
# The arguments are concatenated thus:
#
#   Thisisanewdescription

qx{../src/task rc:bug.rc add This is the original text 2>&1};
my $output = qx{../src/task rc:bug.rc info 1 2>&1};
like ($output, qr/Description\s+This is the original text/, 'original correct');

qx{../src/task rc:bug.rc 1 modify This is the modified text 2>&1};
$output = qx{../src/task rc:bug.rc info 1 2>&1};
like ($output, qr/Description\s+This is the modified text\n/, 'modified correct');

# When a task is added like this:
#
#   % task add aaa bbb:ccc ddd
#
# The description is concatenated thus:
#
#   aaabbb:ccc ddd

qx{../src/task rc:bug.rc add aaa bbb:ccc ddd 2>&1};
$output = qx{../src/task rc:bug.rc info 2 2>&1};
like ($output, qr/Description\s+aaa bbb:ccc ddd\n/, 'properly concatenated');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data bug.rc);
exit 0;

