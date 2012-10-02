#! /usr/bin/env perl
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
use Test::More tests => 10;

# Create the rc file.
if (open my $fh, '>', 'bug.rc')
{
  print $fh "data.location=.\n";
  close $fh;
  ok (-r 'bug.rc', 'Created bug.rc');
}

# Bug #818: Filtering by tag counter-intuitively uses partial match

qx{../src/task rc:bug.rc add +hannah +anna Buy some bananas 2>&1};
my $output = qx{../src/task rc:bug.rc list +hannah 2>&1};
like ($output, qr/bananas/, 'Containing tag query');

$output = qx{../src/task rc:bug.rc list +anna 2>&1};
like ($output, qr/bananas/, 'Contained tag query');

qx{../src/task rc:bug.rc add +anna +hannah Buy tickets to Santana 2>&1};
$output = qx{../src/task rc:bug.rc list +anna 2>&1};
like ($output, qr/Santana/, 'Contained tag query');

$output = qx{../src/task rc:bug.rc list +hannah 2>&1};
like ($output, qr/Santana/, 'Containing tag query');

# Buy some bananas        +hannah +anna
# Buy tickets to Santana  +anna +hannah
# AAA                     +hannah
# BBB                     +anna
qx{../src/task rc:bug.rc add +hannah AAA 2>&1};
qx{../src/task rc:bug.rc add +anna BBB 2>&1};
$output = qx{../src/task rc:bug.rc long +anna 2>&1};
like   ($output, qr/bananas/, '+anna --> bananas');
like   ($output, qr/Santana/, '+anna --> Santana');
unlike ($output, qr/AAA/,     '+anna !-> AAA');
like   ($output, qr/BBB/,     '+anna --> BBB');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data bug.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'bug.rc', 'Cleanup');

exit 0;

