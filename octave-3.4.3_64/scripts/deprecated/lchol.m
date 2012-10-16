## Copyright (C) 2008-2011 David Bateman
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
## @deftypefn  {Loadable Function} {@var{l} =} lchol (@var{a})
## @deftypefnx {Loadable Function} {[@var{l}, @var{p}] =} lchol (@var{a})
## This function has been deprecated.  Use @code{chol (@dots{},'lower')}
## instead.
## @end deftypefn

## Deprecated in version 3.2

function varargout = lchol (varargin)
  persistent warned = false;
  if (! warned)
    warned = true;
    warning ("Octave:deprecated-function",
             "spfind is obsolete and will be removed from a future version of Octave; please use find instead");
  endif

  varargout = cell (nargout, 1);
  [ varargout{:} ] = chol (varargin{:}, "lower");

endfunction
