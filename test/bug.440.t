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
use Test::More tests => 6;

# Ensure environment has no influence.
delete $ENV{'TASKDATA'};
delete $ENV{'TASKRC'};

# Create the rc file.
if (open my $fh, '>', '440.rc')
{
  print $fh "data.location=.\n",
            "confirmation=off\n";

  close $fh;
  ok (-r '440.rc', 'Created 440.rc');
}

# Bug #440: Parser recognizes an attempt to simultaneously subst and append, but doesn't do it

# Create a task and attempt simultaneous subst and appends, both permutations

qx{../src/task rc:440.rc add Foo 2>&1};
qx{../src/task rc:440.rc add Foo 2>&1};

qx{../src/task rc:440.rc 1 append /Foo/Bar/ Appendtext 2>&1};
qx{../src/task rc:440.rc 2 append Appendtext /Foo/Bar/ 2>&1};

my $output1 = qx{../src/task rc:440.rc 1 ls 2>&1};
my $output2 = qx{../src/task rc:440.rc 2 ls 2>&1};

unlike ($output1, qr/Foo/, 'simultaneous subst and append - subst');
like ($output1, qr/\w+ Appendtext/, 'simultaneous subst and append - append');

unlike ($output2, qr/Foo/, 'simultaneous append and subst - subst');
like ($output2, qr/\w+ Appendtext/, 'simultaneous append and subst - append');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data 440.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r '440.rc', 'Cleanup');

exit 0;
