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
use Test::More tests => 19;

# '15min' is seen as '15', 'min', not '15min' duration.
my $output = qx{../src/calc --debug --noambiguous '12 * 3600 + 34 * 60 + 56'};
like ($output, qr/eval push '12' Number/,   'Number 12');
like ($output, qr/eval push '3600' Number/, 'Number 3600');
like ($output, qr/eval push '34' Number/,   'Number 60');
like ($output, qr/eval push '60' Number/,   'Number 60');
like ($output, qr/eval push '56' Number/,   'Number 56');
like ($output, qr/^45296$/ms,               'Result 45296');
unlike ($output, qr/Error/,                 'No errors');

$output = qx{../src/calc --debug --noambiguous --postfix '12 3600 * 34 60 * 56 + +'};
like ($output, qr/eval push '12' Number/,   'Number 12');
like ($output, qr/eval push '3600' Number/, 'Number 3600');
like ($output, qr/eval push '34' Number/,   'Number 60');
like ($output, qr/eval push '60' Number/,   'Number 60');
like ($output, qr/eval push '56' Number/,   'Number 56');
like ($output, qr/^45296$/ms,               'Result 45296');
unlike ($output, qr/Error/,                 'No errors');

$output = qx{../src/calc --debug --noambiguous '2--3'};
like ($output, qr/eval push '2' Number/ms,  'Number 2');
like ($output, qr/eval operator '-'/ms,     'Operator -');
like ($output, qr/eval push '3' Number/ms,  'Number 3');
like ($output, qr/^5$/ms,                   'Result 5');
unlike ($output, qr/Error/,                 'No errors');

exit 0;

