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
## @deftypefn {Function File} {} cauchy_pdf (@var{x}, @var{location}, @var{scale})
## For each element of @var{x}, compute the probability density function
## (PDF) at @var{x} of the Cauchy distribution with location parameter
## @var{location} and scale parameter @var{scale} > 0.  Default values are
## @var{location} = 0, @var{scale} = 1.
## @end deftypefn

## Author: KH <Kurt.Hornik@wu-wien.ac.at>
## Description: PDF of the Cauchy distribution

function pdf = cauchy_pdf (x, location, scale)

  if (! (nargin == 1 || nargin == 3))
    print_usage ();
  endif

  if (nargin == 1)
    location = 0;
    scale = 1;
  endif

  if (!isscalar (location) || !isscalar (scale))
    [retval, x, location, scale] = common_size (x, location, scale);
    if (retval > 0)
      error ("cauchy_pdf: X, LOCATION and SCALE must be of common size or scalar");
    endif
  endif

  sz = size (x);
  pdf = NaN (sz);

  k = find ((x > -Inf) & (x < Inf) & (location > -Inf) &
            (location < Inf) & (scale > 0) & (scale < Inf));
  if (any (k))
    if (isscalar (location) && isscalar (scale))
      pdf(k) = ((1 ./ (1 + ((x(k) - location) ./ scale) .^ 2))
                / pi ./ scale);
    else
      pdf(k) = ((1 ./ (1 + ((x(k) - location(k)) ./ scale(k)) .^ 2))
                / pi ./ scale(k));
    endif
  endif

endfunction
