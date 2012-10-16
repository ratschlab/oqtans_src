## Copyright (C) 2007-2011 John W. Eaton
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
## @deftypefn  {Function File} {@var{h} =} text (@var{x}, @var{y}, @var{label})
## @deftypefnx {Function File} {@var{h} =} text (@var{x}, @var{y}, @var{z}, @var{label})
## @deftypefnx {Function File} {@var{h} =} text (@var{x}, @var{y}, @var{label}, @var{p1}, @var{v1}, @dots{})
## @deftypefnx {Function File} {@var{h} =} text (@var{x}, @var{y}, @var{z}, @var{label}, @var{p1}, @var{v1}, @dots{})
## Create a text object with text @var{label} at position @var{x},
## @var{y}, @var{z} on the current axes.  Property-value pairs following
## @var{label} may be used to specify the appearance of the text.
## @end deftypefn

## Author: jwe

function h = text (varargin)

  nargs = nargin;
  offset = 0;

  if (nargs > 2 && isnumeric (varargin{1}) && isnumeric (varargin{2}))
    x = varargin{1};
    y = varargin{2};
    offset = 3;

    if (nargin > 3 && isnumeric (varargin{3}))
      z = varargin{3};
      offset = 4;
    else
      z = zeros (size (x));
      offset = 3;
    endif

    label = varargin{offset};
    if (ischar (label) || iscellstr (label))
      varargin(1:offset) = [];
      if (ischar (label))
        label = cellstr (label);
      endif
      n = numel (label);
      nx = numel (x);
      ny = numel (y);
      nz = numel (z);
    else
      error ("text: expecting LABEL to be a character string or cell array of character strings");
    endif
  else
    x = y = z = 0;
    nx = ny = nz = 1;
    label = {""};
    n = 1;
  endif

  if (rem (numel (varargin), 2) == 0)

    if (nx == ny && nx == nz)
      pos = [x(:), y(:), z(:)];
      ca = gca ();
      tmp = zeros (n, 1);
      if (n == 1)
        label = label{1};
        for i = 1:nx
          tmp(i) = __go_text__ (ca, "string", label,
                                varargin{:},
                                "position", pos(i,:));
        endfor
        __request_drawnow__ ();
      elseif (n == nx)
        for i = 1:nx
          tmp(i) = __go_text__ (ca, "string", label{i},
                                varargin{:},
                                "position", pos(i,:));
        endfor
        __request_drawnow__ ();
      else
        error ("text: dimension mismatch for coordinates and LABEL");
      endif
    else
      error ("text: dimension mismatch for coordinates");
    endif

    if (nargout > 0)
      h = tmp;
    endif

  else
    print_usage ();
  endif

endfunction

%!demo
%! clf
%! ha = {"left", "center", "right"};
%! va = {"bottom", "middle", "top"};
%! x = [0.25 0.5 0.75];
%! y = [0.25 0.5 0.75];
%! for t = 0:30:359;
%!   for nh = 1:numel(ha)
%!     for nv = 1:numel(va)
%!       text (x(nh), y(nv), "Hello World", ...
%!             "rotation", t, ...
%!             "horizontalalignment", ha{nh}, ...
%!             "verticalalignment", va{nv})
%!     endfor
%!   endfor
%! endfor
%! set (gca, "xtick", [0.25, 0.5, 0.75], ...
%!           "xticklabel", ha, ...
%!           "ytick", [0.25, 0.5, 0.75], ...
%!           "yticklabel", va)
%! axis ([0 1 0 1])
%! xlabel ("horizontal alignment")
%! ylabel ("vertical alignment")
%! title ("text alignment and rotation (0:30:360 degrees)")

%!demo
%! clf
%! h = mesh (peaks, "edgecolor", 0.7 * [1 1 1], ...
%!                  "facecolor", "none", ...
%!                  "facealpha", 0);
%! for t = 0:45:359;
%!   text (25, 25, 0, "Vertical Alignment = Bottom", ...
%!                    "rotation", t, ...
%!                    "horizontalalignment", "left", ...
%!                    "verticalalignment", "bottom")
%! endfor
%! caxis ([-100 100])
%! title ("Vertically Aligned at Bottom")
