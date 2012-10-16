## Copyright (C) 2007-2011 David Bateman
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
## @deftypefn  {Function File} {} trisurf (@var{tri}, @var{x}, @var{y}, @var{z})
## @deftypefnx {Function File} {@var{h} =} trisurf (@dots{})
## Plot a triangular surface in 3D@.  The variable @var{tri} is the triangular
## meshing of the points @code{(@var{x}, @var{y})} which is returned
## from @code{delaunay}.  The variable @var{z} is value at the point
## @code{(@var{x}, @var{y})}.  The output argument @var{h} is the graphic
## handle of the plot.
## @seealso{triplot, trimesh, delaunay3}
## @end deftypefn

function varargout = trisurf (tri, x, y, z, varargin)

  if (nargin < 3)
    print_usage ();
  endif

  if (nargin == 3)
    triplot (tri, x, y);
  elseif (ischar (z))
    triplot (tri, x, y, z, varargin{:});
  else
    if (nargin > 4 && isnumeric (varargin{1}))
      c = varargin{1};
      varargin(1) = [];
    else
      c = z;
    endif
    if (! any (strcmpi (varargin, "FaceColor")))
      nfc = numel (varargin) + 1;
      varargin(nfc+(0:1)) = {"FaceColor", "flat"};
    else
      nfc = find (any (strcmpi (varargin, "FaceColor")), 1);
    endif
    if (! any (strcmpi (varargin, "EdgeColor"))
        && strcmpi (varargin{nfc+1}, "interp"))
      varargin(end+(1:2)) = {"EdgeColor", "none"};
    endif
    newplot ();
    h = patch ("Faces", tri, "Vertices", [x(:), y(:), z(:)],
               "FaceVertexCData", reshape (c, numel (c), 1),
               varargin{:});
    if (nargout > 0)
      varargout = {h};
    endif

    if (! ishold ())
      set (gca(), "view", [-37.5, 30],
           "xgrid", "on", "ygrid", "on", "zgrid", "on");
    endif
  endif
endfunction

%!demo
%! N = 10;
%! rand ('state', 10)
%! x = 3 - 6 * rand (N, N);
%! y = 3 - 6 * rand (N, N);
%! z = peaks (x, y);
%! tri = delaunay (x(:), y(:));
%! trisurf (tri, x(:), y(:), z(:));

%!demo
%! x = rand (100, 1);
%! y = rand (100, 1);
%! z = x.^2 + y.^2; 
%! tri = delaunay (x, y); 
%! trisurf (tri, x, y, z)

%!demo
%! x = rand (100, 1);
%! y = rand (100, 1);
%! z = x.^2 + y.^2; 
%! tri = delaunay (x, y); 
%! trisurf (tri, x, y, z, "facecolor", "interp")

%!demo
%! x = rand (100, 1);
%! y = rand (100, 1);
%! z = x.^2 + y.^2; 
%! tri = delaunay (x, y); 
%! trisurf (tri, x, y, z, "facecolor", "interp", "edgecolor", "k")


