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
use Test::More tests => 3;

# Create the rc file.
if (open my $fh, '>', '425.rc')
{
  print $fh "data.location=.\n",
            "confirmation=off\n";

  close $fh;
  ok (-r '425.rc', 'Created 425.rc');
}

# Bug #425: Parser preventing editing of an existing task depending on description

# Create a task and attempt to revise the description to include the word 'in'
# (this breaks in 1.9.3 and earlier)

qx{../src/task rc:425.rc add Foo 2>&1};
qx{../src/task rc:425.rc 1 modify Bar in Bar 2>&1};

my $output = qx{../src/task rc:425.rc 1 ls 2>&1};
like ($output, qr/1\s+Bar in Bar/m, 'parser - interpret \'in\' in description');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data synch.key 425.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'synch.key'      &&
    ! -r '425.rc', 'Cleanup');

exit 0;
