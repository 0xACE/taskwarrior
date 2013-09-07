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
use Test::More tests => 18;

# Ensure environment has no influence.
delete $ENV{'TASKDATA'};
delete $ENV{'TASKRC'};

# Create the rc file.
if (open my $fh, '>', 'bug.rc')
{
  print $fh "data.location=.\n";
  print $fh "alias.samplealias=long\n";
  print $fh "abbreviation.minimum=5\n";

  close $fh;
  ok (-r 'bug.rc', 'Created bug.rc');
}

my $source_dir = $0;
$source_dir =~ s{[^/]+$}{..};

# Copy task.sh and make substitutions & additions needed for testing.
if (open my $target, '>', 'task.sh')
{
  if (open my $source, '<', "$source_dir/scripts/bash/task.sh")
  {
    while (<$source>)
    {
      my $temp=$_;
      chomp($_);
      if ($_ eq qw{taskcommand='task'})
      {
        print $target "taskcommand='../src/task rc:bug.rc'";
      }
      else
      {
        print $target $temp;
      }
    }
    close ($source);
    print $target 'COMP_WORDS=("$@")',
                  "\n",
                  'COMP_CWORD=$(($#-1))',
                  "\n",
                  '_task',
                  "\n",
                  'for reply_iter in "${COMPREPLY[@]}"; do',
                  "\n",
                  '  echo $reply_iter',
                  "\n",
                  'done';
    close $target;
    ok (-r 'task.sh', 'Created task.sh');
  }
}

# aliases should be expanded
my $output = qx{bash ./task.sh task sampleali 2>&1};
ok ($? == 0, 'Exit status check');
like ($output, qr/samplealias/, 'Aliases are expanded');

# commands should be expanded
$output = qx{bash ./task.sh task m 2>&1};
ok ($? == 0, 'Exit status check');
like ($output, qr/modify/, 'expansion of \'m\' includes \'modify\'');

# "project:" should be expanded correctly and dependent on abbreviation.minimum
qx{../src/task rc:bug.rc add testing project expansion project:todd 2>&1};

# note the spaces between "projABC", ":", and "to" for correct bash parsing
$output = qx{bash ./task.sh task projeABC : to 2>&1};
ok ($? == 0, 'Exit status check');
unlike ($output, qr/todd/, '\'projeABC:\' does not expand');

$output = qx{bash ./task.sh task proje : to 2>&1};
ok ($? == 0, 'Exit status check');
like ($output, qr/todd/, '\'proje:\' does expand');

$output = qx{bash ./task.sh task proj : to 2>&1};
ok ($? == 0, 'Exit status check');
unlike ($output, qr/todd/, '\'proj:\' does not expand if abbreviation.minimum is 5');

# The following tests were removed because we no longer expand task IDs after
# depends. This expansion was stopped because it was using the _ids command
# which runs _ids and can lead to confusing behavior for the user. See:
# https://groups.google.com/forum/#!topic/taskwarrior-dev/KwHnb9MOOqA

## "priority:" should be expanded correctly and dependent on abbreviation.minimum
#$output = qx{bash ./task.sh task priorABC : 2>&1};
#ok ($? == 0, 'Exit status check');
#unlike ($output, qr/H/, '\'priorABC:\' does not expand');
#
#$output = qx{bash ./task.sh task prior : 2>&1};
#ok ($? == 0, 'Exit status check');
#like ($output, qr/H/, '\'prior:\' does expand');
#
#$output = qx{bash ./task.sh task prio : 2>&1};
#ok ($? == 0, 'Exit status check');
#unlike ($output, qr/H/, '\'prio:\' does not expand if abbreviation.minimum is 5');
#
## "depends:" should be expanded correctly and dependent on abbreviation.minimum
#$output = qx{bash ./task.sh task depenABC : 2>&1};
#ok ($? == 0, 'Exit status check');
#unlike ($output, qr/1/, '\'depenABC:\' does not expand');
#
#$output = qx{bash ./task.sh task depen : 2>&1};
#ok ($? == 0, 'Exit status check');
#like ($output, qr/1/, '\'depen:\' does expand');
#
#$output = qx{bash ./task.sh task depe : 2>&1};
#ok ($? == 0, 'Exit status check');
#unlike ($output, qr/1/, '\'depe:\' does not expand if abbreviation.minimum is 5');

# there should be no gc coming from bash completion
qx{../src/task rc:bug.rc add this task should be number 2 and stay number 2 2>&1};
ok ($? == 0, 'Exit status check');
qx{../src/task rc:bug.rc rc.confirmation:off 1 delete 2>&1};
ok ($? == 0, 'Exit status check');
qx{bash ./task.sh task depends : 2>&1};
ok ($? == 0, 'Exit status check');
$output = qx{../src/task rc:bug.rc rc.confirmation:off 2 modify shouldreplacetext 2>&1};
ok ($? == 0, 'Should exit with 0 because task should exist');
like ($output, qr/shouldreplacetext/, 'no gc was run');

# Cleanup.
unlink qw(pending.data completed.data undo.data backlog.data bug.rc task.sh);
ok (! -r 'pending.data'   &&
    ! -r 'completed.data' &&
    ! -r 'undo.data'      &&
    ! -r 'backlog.data'   &&
    ! -r 'bug.rc'         &&
    ! -r 'task.sh', 'Cleanup');

exit 0;
