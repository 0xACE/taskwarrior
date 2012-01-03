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
use Test::More tests => 8;

# Create the rc file.
if (open my $fh, '>', 'projects.rc')
{
  print $fh "data.location=.\n";
  close $fh;
  ok (-r 'projects.rc', 'Created projects.rc');
}

# Create a data set of two tasks, with unique project names, one
# pending, one completed.
qx{../src/task rc:projects.rc add project:p1 one};
qx{../src/task rc:projects.rc add project:p2 two};
qx{../src/task rc:projects.rc 1 done};

my $output = qx{../src/task rc:projects.rc ls};
unlike ($output, qr/p1/, 'p1 done');
like ($output, qr/p2/, 'p2 pending');

$output = qx{../src/task rc:projects.rc projects};
unlike ($output, qr/p1/, 'p1 done');
like ($output, qr/p2/, 'p2 pending');

$output = qx{../src/task rc:projects.rc rc.list.all.projects:yes projects};
like ($output, qr/p1/, 'p1 listed');
like ($output, qr/p2/, 'p2 listed');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data synch.key projects.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'synch.key'      &&
    ! -r 'projects.rc', 'Cleanup');

exit 0;

