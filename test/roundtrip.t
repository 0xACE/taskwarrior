#! /usr/bin/env perl
################################################################################
## taskwarrior - a command line task list manager.
##
## Copyright 2006-2014, Paul Beckingham, Federico Hernandez.
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
use Test::More tests => 5;

# Ensure environment has no influence.
delete $ENV{'TASKDATA'};
delete $ENV{'TASKRC'};

# Create the rc file.
if (open my $fh, '>', 'roundtrip.rc')
{
  print $fh "data.location=.\n",
            "verbose=off\n",
            "confirmation=no\n",
            "defaultwidth=100\n",
            "dateformat=m/d/Y\n";
  close $fh;
  ok (-r 'roundtrip.rc', 'Created roundtrip.rc');
}

# Add two tasks.
qx{../src/task rc:roundtrip.rc add priority:H project:A one/1 2>&1};
qx{../src/task rc:roundtrip.rc add +tag1 +tag2 two 2>&1};

# trip 1.
qx{../src/task rc:roundtrip.rc export > ./roundtrip1.json 2>&1};
unlink 'pending.data', 'completed.data', 'undo.data';
qx{../src/task rc:roundtrip.rc rc.debug:1 import ./roundtrip1.json 2>&1};

# trip 2.
qx{../src/task rc:roundtrip.rc export > ./roundtrip2.json 2>&1};
unlink 'pending.data', 'completed.data', 'undo.data';
qx{../src/task rc:roundtrip.rc import ./roundtrip2.json 2>&1};

# Examine.
#
# ID Created    P Project Tags      Description
# -- ---------- - ------- --------- -----------
#  1 1/7/2014   H A                 one/1
#  2 1/7/2014             tag1 tag2 two

my $output = qx{../src/task rc:roundtrip.rc long 2>&1};
like ($output, qr/1\s+\d+\/\d+\/\d+\s+H\s+A\s+one\/1/,    '2 round trips task 1 identical');
like ($output, qr/2\s+\d+\/\d+\/\d+\s+tag1\s+tag2\s+two/, '2 round trips task 2 identical');

# Compare the actual JSON files.
$output = qx{diff ./roundtrip1.json ./roundtrip2.json 2>&1};
like ($output, qr/^$/, 'JSON files roundtrip1.json and roundtrip2.json identical');

# Cleanup.
unlink qw(roundtrip1.json roundtrip2.json pending.data completed.data undo.data backlog.data roundtrip.rc);
ok (! -r 'roundtrip1.json' &&
    ! -r 'roundtrip2.json' &&
    ! -r 'pending.data'    &&
    ! -r 'completed.data'  &&
    ! -r 'undo.data'       &&
    ! -r 'backlog.data'    &&
    ! -r 'roundtrip.rc', 'Cleanup');

exit 0;

