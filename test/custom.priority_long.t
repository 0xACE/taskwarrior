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
use Test::More tests => 6;

# Create the rc file.
if (open my $fh, '>', 'pri.rc')
{
  print $fh "data.location=.\n",
            "report.foo.description=DESC\n",
            "report.foo.columns=id,priority.long\n",
            "report.foo.labels=ID,Pri\n",
            "report.foo.sort=id+\n";
  close $fh;
  ok (-r 'pri.rc', 'Created pri.rc');
}

# Generate the usage screen, and locate the custom report on it.
qx{../src/task rc:pri.rc add one   pri:H 2>&1};
qx{../src/task rc:pri.rc add two   pri:M 2>&1};
qx{../src/task rc:pri.rc add three pri:L 2>&1};

my $output = qx{../src/task rc:pri.rc foo 2>&1};
like ($output,   qr/ID.+Pri/,    'priority.long indicator heading');
like ($output,   qr/1\s+High/,   'priority.long High');
like ($output,   qr/2\s+Medium/, 'priority.long Medium');
like ($output,   qr/3\s+Low/,    'priority.long Low');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data synch.key pri.rc);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'synch.key'      &&
    ! -r 'pri.rc', 'Cleanup');

exit 0;

