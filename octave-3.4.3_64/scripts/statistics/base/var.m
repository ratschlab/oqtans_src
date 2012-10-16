## Copyright (C) 1995-2011 Kurt Hornik
##
## This file is part of Octave.
##
## Octave is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or (at
## your option) any later version.
##
## Octave is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Octave; see the file COPYING.  If not, see
## <http://www.gnu.org/licenses/>.

## -*- texinfo -*-
## @deftypefn  {Function File} {} var (@var{x})
## @deftypefnx {Function File} {} var (@var{x}, @var{opt})
## @deftypefnx {Function File} {} var (@var{x}, @var{opt}, @var{dim})
## Compute the variance of the elements of the vector @var{x}.
## @tex
## $$
## {\rm std} (x) = \sigma^2 = {\sum_{i=1}^N (x_i - \bar{x})^2 \over N - 1}
## $$
## where $\bar{x}$ is the mean value of $x$.
## @end tex
## @ifnottex
##
## @example
## @group
## std (x) = 1/(N-1) SUM_i (x(i) - mean(x))^2
## @end group
## @end example
##
## @end ifnottex
## If @var{x} is a matrix, compute the variance for each column
## and return them in a row vector.
##
## The argument @var{opt} determines the type of normalization to use.
## Valid values are
##
## @table @asis
## @item 0:
##   normalize with @math{N-1}, provides the best unbiased estimator of the
## variance [default]
##
## @item 1:
##   normalizes with @math{N}, this provides the second moment around the mean
## @end table
##
## If the optional argument @var{dim} is given, operate along this dimension.
## @seealso{cov, std, skewness, kurtosis, moment}
## @end deftypefn

## Author: KH <Kurt.Hornik@wu-wien.ac.at>
## Description: Compute variance

function retval = var (x, opt = 0, dim)

  if (nargin < 1 || nargin > 3)
    print_usage ();
  endif

  if (!isnumeric (x))
    error ("var: X must be a numeric vector or matrix");
  endif

  if (isempty (opt))
    opt = 0;
  endif
  if (opt != 0 && opt != 1)
    error ("var: normalization OPT must be 0 or 1");
  endif

  if (nargin < 3)
    dim = find (size (x) > 1, 1);
    if (isempty (dim))
      dim = 1;
    endif
  endif

  n = size (x, dim);
  if (n == 1)
    retval = zeros (size (x), class (x));
  elseif (numel (x) > 0)
    retval = sumsq (center (x, dim), dim) / (n - 1 + opt);
  else
    error ("var: X must not be empty");
  endif

endfunction

%!assert (var (13), 0)
%!assert (var ([1,2,3]), 1)
%!assert (var ([1,2,3], 1), 2/3, eps)
%!assert (var ([1,2,3], [], 1), [0,0,0])

%% Test input validation
%!error var ()
%!error var (1,2,3,4)
%!error var (true(1,2))
%!error var (1, -1);
%!error var ([],1)

