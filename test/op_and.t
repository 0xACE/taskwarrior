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
use Test::More tests => 26;

# Create the rc file.
if (open my $fh, '>', 'op.rc')
{
  print $fh "data.location=.\n",
            "confirmation=no\n";
  close $fh;
  ok (-r 'op.rc', 'Created op.rc');
}

# Setup: Add a task
qx{../src/task rc:op.rc add one   project:A priority:H 2>&1};
qx{../src/task rc:op.rc add two   project:A            2>&1};
qx{../src/task rc:op.rc add three           priority:H 2>&1};
qx{../src/task rc:op.rc add four                       2>&1};

# Test the 'and' operator.
my $output = qx{../src/task rc:op.rc ls project:A priority:H 2>&1};
like   ($output, qr/one/,   'ls project:A priority:H --> one');
unlike ($output, qr/two/,   'ls project:A priority:H --> !two');
unlike ($output, qr/three/, 'ls project:A priority:H --> !three');
unlike ($output, qr/four/,  'ls project:A priority:H --> !four');

$output = qx{../src/task rc:op.rc ls project:A and priority:H 2>&1};
like   ($output, qr/one/,   'ls project:A priority:H --> one');
unlike ($output, qr/two/,   'ls project:A priority:H --> !two');
unlike ($output, qr/three/, 'ls project:A priority:H --> !three');
unlike ($output, qr/four/,  'ls project:A priority:H --> !four');

$output = qx{../src/task rc:op.rc ls project:A and priority.not:H 2>&1};
unlike ($output, qr/one/,   'ls project:A priority:H --> !one');
like   ($output, qr/two/,   'ls project:A priority:H --> two');
unlike ($output, qr/three/, 'ls project:A priority:H --> !three');
unlike ($output, qr/four/,  'ls project:A priority:H --> !four');

$output = qx{../src/task rc:op.rc ls project=A priority=H 2>&1};
like   ($output, qr/one/,   'ls project:A priority:H --> one');
unlike ($output, qr/two/,   'ls project:A priority:H --> !two');
unlike ($output, qr/three/, 'ls project:A priority:H --> !three');
unlike ($output, qr/four/,  'ls project:A priority:H --> !four');

$output = qx{../src/task rc:op.rc ls project=A and priority=H 2>&1};
like   ($output, qr/one/,   'ls project:A priority:H --> one');
unlike ($output, qr/two/,   'ls project:A priority:H --> !two');
unlike ($output, qr/three/, 'ls project:A priority:H --> !three');
unlike ($output, qr/four/,  'ls project:A priority:H --> !four');

$output = qx{../src/task rc:op.rc ls project=A and priority!=H 2>&1};
unlike ($output, qr/one/,   'ls project:A priority:H --> !one');
like   ($output, qr/two/,   'ls project:A priority:H --> two');
unlike ($output, qr/three/, 'ls project:A priority:H --> !three');
unlike ($output, qr/four/,  'ls project:A priority:H --> !four');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data synch.key op.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'synch.key'      &&
    ! -r 'op.rc', 'Cleanup');

exit 0;

