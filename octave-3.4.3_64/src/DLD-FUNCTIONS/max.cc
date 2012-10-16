/*

Copyright (C) 1996-2011 John W. Eaton
Copyright (C) 2009 VZLU Prague

This file is part of Octave.

Octave is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

Octave is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with Octave; see the file COPYING.  If not, see
<http://www.gnu.org/licenses/>.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "lo-ieee.h"
#include "lo-mappers.h"
#include "lo-math.h"
#include "dNDArray.h"
#include "CNDArray.h"
#include "quit.h"

#include "defun-dld.h"
#include "error.h"
#include "gripes.h"
#include "oct-obj.h"

#include "ov-cx-mat.h"
#include "ov-re-sparse.h"
#include "ov-cx-sparse.h"

template <class ArrayType>
static octave_value_list
do_minmax_red_op (const octave_value& arg,
                  int nargout, int dim, bool ismin)
{
  octave_value_list retval;
  ArrayType array = octave_value_extract<ArrayType> (arg);

  if (error_state)
    return retval;

  if (nargout == 2)
    {
      retval.resize (2);
      Array<octave_idx_type> idx;
      if (ismin)
        retval(0) = array.min (idx, dim);
      else
        retval(0) = array.max (idx, dim);

      retval(1) = octave_value (idx, true, true);
    }
  else
    {
      if (ismin)
        retval(0) = array.min (dim);
      else
        retval(0) = array.max (dim);
    }

  return retval;
}

// Specialization for bool arrays.
template <>
octave_value_list
do_minmax_red_op<boolNDArray> (const octave_value& arg,
                               int nargout, int dim, bool ismin)
{
  octave_value_list retval;

  if (nargout <= 1)
    {
      // This case can be handled using any/all.
      boolNDArray array = arg.bool_array_value ();

      if (array.is_empty ())
        retval(0) = array;
      else if (ismin)
        retval(0) = array.all (dim);
      else
        retval(0) = array.any (dim);
    }
  else
    {
      // any/all don't have indexed versions, so do it via a conversion.
      retval = do_minmax_red_op<int8NDArray> (arg, nargout, dim, ismin);
      if (! error_state)
        retval(0) = retval(0).bool_array_value ();
    }

  return retval;
}

template <class ArrayType>
static octave_value
do_minmax_bin_op (const octave_value& argx, const octave_value& argy,
                  bool ismin)
{
  typedef typename ArrayType::element_type ScalarType;

  octave_value retval;

  if (argx.is_scalar_type () == 1)
    {
      ScalarType x = octave_value_extract<ScalarType> (argx);
      ArrayType y = octave_value_extract<ArrayType> (argy);

      if (error_state)
        ;
      else if (ismin)
        retval = min (x, y);
      else
        retval = max (x, y);
    }
  else if (argy.is_scalar_type () == 1)
    {
      ArrayType x = octave_value_extract<ArrayType> (argx);
      ScalarType y = octave_value_extract<ScalarType> (argy);

      if (error_state)
        ;
      else if (ismin)
        retval = min (x, y);
      else
        retval = max (x, y);
    }
  else
    {
      ArrayType x = octave_value_extract<ArrayType> (argx);
      ArrayType y = octave_value_extract<ArrayType> (argy);

      if (error_state)
        ;
      else if (ismin)
        retval = min (x, y);
      else
        retval = max (x, y);
    }

  return retval;
}

static octave_value_list
do_minmax_body (const octave_value_list& args,
                int nargout, bool ismin)
{
  octave_value_list retval;

  const char *func = ismin ? "min" : "max";

  int nargin = args.length ();

  if (nargin == 3 || nargin == 1)
    {
      octave_value arg = args(0);
      int dim = -1;
      if (nargin == 3)
        {
          dim = args(2).int_value (true) - 1;
          if (error_state || dim < 0)
            {
              error ("%s: DIM must be a valid dimension", func);
              return retval;
            }

          if (! args(1).is_empty ())
            warning ("%s: second argument is ignored");
        }

      switch (arg.builtin_type ())
        {
        case btyp_double:
          {
            if (arg.is_range () && (dim == -1 || dim == 1))
              {
                Range range = arg.range_value ();
                if (range.nelem () == 0)
                  {
                    retval(0) = arg;
                    if (nargout > 1)
                      retval(1) = arg;
                  }
                else if (ismin)
                  {
                    retval(0) = range.min ();
                    if (nargout > 1)
                      retval(1) = static_cast<double> (range.inc () < 0 ? range.nelem () : 1);
                  }
                else
                  {
                    retval(0) = range.max ();
                    if (nargout > 1)
                      retval(1) = static_cast<double> (range.inc () >= 0 ? range.nelem () : 1);
                  }
              }
            else if (arg.is_sparse_type ())
              retval = do_minmax_red_op<SparseMatrix> (arg, nargout, dim, ismin);
            else
              retval = do_minmax_red_op<NDArray> (arg, nargout, dim, ismin);
            break;
          }
        case btyp_complex:
          {
            if (arg.is_sparse_type ())
              retval = do_minmax_red_op<SparseComplexMatrix> (arg, nargout, dim, ismin);
            else
              retval = do_minmax_red_op<ComplexNDArray> (arg, nargout, dim, ismin);
            break;
          }
        case btyp_float:
          retval = do_minmax_red_op<FloatNDArray> (arg, nargout, dim, ismin);
          break;
        case btyp_float_complex:
          retval = do_minmax_red_op<FloatComplexNDArray> (arg, nargout, dim, ismin);
          break;
#define MAKE_INT_BRANCH(X) \
        case btyp_ ## X: \
          retval = do_minmax_red_op<X ## NDArray> (arg, nargout, dim, ismin); \
          break;
        MAKE_INT_BRANCH (int8);
        MAKE_INT_BRANCH (int16);
        MAKE_INT_BRANCH (int32);
        MAKE_INT_BRANCH (int64);
        MAKE_INT_BRANCH (uint8);
        MAKE_INT_BRANCH (uint16);
        MAKE_INT_BRANCH (uint32);
        MAKE_INT_BRANCH (uint64);
#undef MAKE_INT_BRANCH
        case btyp_bool:
          retval = do_minmax_red_op<boolNDArray> (arg, nargout, dim, ismin);
          break;
        default:
          gripe_wrong_type_arg (func, arg);
      }
    }
  else if (nargin == 2)
    {
      octave_value argx = args(0), argy = args(1);
      builtin_type_t xtyp = argx.builtin_type (), ytyp = argy.builtin_type ();
      builtin_type_t rtyp = btyp_mixed_numeric (xtyp, ytyp);

      switch (rtyp)
        {
        case btyp_double:
          {
            if ((argx.is_sparse_type ()
                 && (argy.is_sparse_type () || argy.is_scalar_type ()))
                || (argy.is_sparse_type () && argx.is_scalar_type ()))
              retval = do_minmax_bin_op<SparseMatrix> (argx, argy, ismin);
            else
              retval = do_minmax_bin_op<NDArray> (argx, argy, ismin);
            break;
          }
        case btyp_complex:
          {
            if ((argx.is_sparse_type ()
                 && (argy.is_sparse_type () || argy.is_scalar_type ()))
                || (argy.is_sparse_type () && argx.is_scalar_type ()))
              retval = do_minmax_bin_op<SparseComplexMatrix> (argx, argy, ismin);
            else
              retval = do_minmax_bin_op<ComplexNDArray> (argx, argy, ismin);
            break;
          }
        case btyp_float:
          retval = do_minmax_bin_op<FloatNDArray> (argx, argy, ismin);
          break;
        case btyp_float_complex:
          retval = do_minmax_bin_op<FloatComplexNDArray> (argx, argy, ismin);
          break;
#define MAKE_INT_BRANCH(X) \
        case btyp_ ## X: \
          retval = do_minmax_bin_op<X ## NDArray> (argx, argy, ismin); \
          break;
        MAKE_INT_BRANCH (int8);
        MAKE_INT_BRANCH (int16);
        MAKE_INT_BRANCH (int32);
        MAKE_INT_BRANCH (int64);
        MAKE_INT_BRANCH (uint8);
        MAKE_INT_BRANCH (uint16);
        MAKE_INT_BRANCH (uint32);
        MAKE_INT_BRANCH (uint64);
#undef MAKE_INT_BRANCH
        default:
          error ("%s: cannot compute %s (%s, %s)", func, func,
                 argx.type_name ().c_str (), argy.type_name ().c_str ());
        }
    }
  else
    print_usage ();

  return retval;
}

DEFUN_DLD (min, args, nargout,
  "-*- texinfo -*-\n\
@deftypefn  {Loadable Function} {} min (@var{x})\n\
@deftypefnx {Loadable Function} {} min (@var{x}, @var{y})\n\
@deftypefnx {Loadable Function} {} min (@var{x}, @var{y}, @var{dim})\n\
@deftypefnx {Loadable Function} {[@var{w}, @var{iw}] =} min (@var{x})\n\
For a vector argument, return the minimum value.  For a matrix\n\
argument, return the minimum value from each column, as a row\n\
vector, or over the dimension @var{dim} if defined.  For two matrices\n\
(or a matrix and scalar), return the pair-wise minimum.\n\
Thus,\n\
\n\
@example\n\
min (min (@var{x}))\n\
@end example\n\
\n\
@noindent\n\
returns the smallest element of @var{x}, and\n\
\n\
@example\n\
@group\n\
min (2:5, pi)\n\
    @result{}  2.0000  3.0000  3.1416  3.1416\n\
@end group\n\
@end example\n\
\n\
@noindent\n\
compares each element of the range @code{2:5} with @code{pi}, and\n\
returns a row vector of the minimum values.\n\
\n\
For complex arguments, the magnitude of the elements are used for\n\
comparison.\n\
\n\
If called with one input and two output arguments,\n\
@code{min} also returns the first index of the\n\
minimum value(s).  Thus,\n\
\n\
@example\n\
@group\n\
[x, ix] = min ([1, 3, 0, 2, 0])\n\
    @result{}  x = 0\n\
        ix = 3\n\
@end group\n\
@end example\n\
@seealso{max, cummin, cummax}\n\
@end deftypefn")
{
  return do_minmax_body (args, nargout, true);
}

/*

%% test/octave.test/arith/min-1.m
%!assert (min ([1, 4, 2, 3]) == 1);
%!assert (min ([1; -10; 5; -2]) == -10);

%% test/octave.test/arith/min-2.m
%!assert(all (min ([4, i; -2, 2]) == [-2, i]));

%% test/octave.test/arith/min-3.m
%!error <Invalid call to min.*> min ();

%% test/octave.test/arith/min-4.m
%!error <Invalid call to min.*> min (1, 2, 3, 4);

%!test
%! x = reshape (1:8,[2,2,2]);
%! assert (max (x,[],1), reshape ([2, 4, 6, 8], [1,2,2]));
%! assert (max (x,[],2), reshape ([3, 4, 7, 8], [2,1,2]));
%! [y, i ] = max (x, [], 3);
%! assert (y, [5, 7; 6, 8]);
%! assert (ndims(y), 2);
%! assert (i, [2, 2; 2, 2]);
%! assert (ndims(i), 2);

*/

DEFUN_DLD (max, args, nargout,
  "-*- texinfo -*-\n\
@deftypefn  {Loadable Function} {} max (@var{x})\n\
@deftypefnx {Loadable Function} {} max (@var{x}, @var{y})\n\
@deftypefnx {Loadable Function} {} max (@var{x}, @var{y}, @var{dim})\n\
@deftypefnx {Loadable Function} {[@var{w}, @var{iw}] =} max (@var{x})\n\
For a vector argument, return the maximum value.  For a matrix\n\
argument, return the maximum value from each column, as a row\n\
vector, or over the dimension @var{dim} if defined.  For two matrices\n\
(or a matrix and scalar), return the pair-wise maximum.\n\
Thus,\n\
\n\
@example\n\
max (max (@var{x}))\n\
@end example\n\
\n\
@noindent\n\
returns the largest element of the matrix @var{x}, and\n\
\n\
@example\n\
@group\n\
max (2:5, pi)\n\
    @result{}  3.1416  3.1416  4.0000  5.0000\n\
@end group\n\
@end example\n\
\n\
@noindent\n\
compares each element of the range @code{2:5} with @code{pi}, and\n\
returns a row vector of the maximum values.\n\
\n\
For complex arguments, the magnitude of the elements are used for\n\
comparison.\n\
\n\
If called with one input and two output arguments,\n\
@code{max} also returns the first index of the\n\
maximum value(s).  Thus,\n\
\n\
@example\n\
@group\n\
[x, ix] = max ([1, 3, 5, 2, 5])\n\
    @result{}  x = 5\n\
        ix = 3\n\
@end group\n\
@end example\n\
@seealso{min, cummax, cummin}\n\
@end deftypefn")
{
  return do_minmax_body (args, nargout, false);
}

/*

%% test/octave.test/arith/max-1.m
%!assert (max ([1, 4, 2, 3]) == 4);
%!assert (max ([1; -10; 5; -2]) == 5);

%% test/octave.test/arith/max-2.m
%!assert(all (max ([4, i 4.999; -2, 2, 3+4i]) == [4, 2, 3+4i]));

%% test/octave.test/arith/max-3.m
%!error <Invalid call to max.*> max ();

%% test/octave.test/arith/max-4.m
%!error <Invalid call to max.*> max (1, 2, 3, 4);

%!test
%! x = reshape (1:8,[2,2,2]);
%! assert (min (x,[],1), reshape ([1, 3, 5, 7], [1,2,2]));
%! assert (min (x,[],2), reshape ([1, 2, 5, 6], [2,1,2]));
%! [y, i ] = min (x, [], 3);
%! assert (y, [1, 3; 2, 4]);
%! assert (ndims(y), 2);
%! assert (i, [1, 1; 1, 1]);
%! assert (ndims(i), 2);


*/

template <class ArrayType>
static octave_value_list
do_cumminmax_red_op (const octave_value& arg,
                     int nargout, int dim, bool ismin)
{
  octave_value_list retval;
  ArrayType array = octave_value_extract<ArrayType> (arg);

  if (error_state)
    return retval;

  if (nargout == 2)
    {
      retval.resize (2);
      Array<octave_idx_type> idx;
      if (ismin)
        retval(0) = array.cummin (idx, dim);
      else
        retval(0) = array.cummax (idx, dim);

      retval(1) = octave_value (idx, true, true);
    }
  else
    {
      if (ismin)
        retval(0) = array.cummin (dim);
      else
        retval(0) = array.cummax (dim);
    }

  return retval;
}

static octave_value_list
do_cumminmax_body (const octave_value_list& args,
                   int nargout, bool ismin)
{
  octave_value_list retval;

  const char *func = ismin ? "cummin" : "cummax";

  int nargin = args.length ();

  if (nargin == 1 || nargin == 2)
    {
      octave_value arg = args(0);
      int dim = -1;
      if (nargin == 2)
        {
          dim = args(1).int_value (true) - 1;
          if (error_state || dim < 0)
            {
              error ("%s: DIM must be a valid dimension", func);
              return retval;
            }
        }

      switch (arg.builtin_type ())
        {
        case btyp_double:
          retval = do_cumminmax_red_op<NDArray> (arg, nargout, dim, ismin);
          break;
        case btyp_complex:
          retval = do_cumminmax_red_op<ComplexNDArray> (arg, nargout, dim, ismin);
          break;
        case btyp_float:
          retval = do_cumminmax_red_op<FloatNDArray> (arg, nargout, dim, ismin);
          break;
        case btyp_float_complex:
          retval = do_cumminmax_red_op<FloatComplexNDArray> (arg, nargout, dim, ismin);
          break;
#define MAKE_INT_BRANCH(X) \
        case btyp_ ## X: \
          retval = do_cumminmax_red_op<X ## NDArray> (arg, nargout, dim, ismin); \
          break;
        MAKE_INT_BRANCH (int8);
        MAKE_INT_BRANCH (int16);
        MAKE_INT_BRANCH (int32);
        MAKE_INT_BRANCH (int64);
        MAKE_INT_BRANCH (uint8);
        MAKE_INT_BRANCH (uint16);
        MAKE_INT_BRANCH (uint32);
        MAKE_INT_BRANCH (uint64);
#undef MAKE_INT_BRANCH
        case btyp_bool:
          {
            retval = do_cumminmax_red_op<int8NDArray> (arg, nargout, dim, ismin);
            if (retval.length () > 0)
              retval(0) = retval(0).bool_array_value ();
            break;
          }
        default:
          gripe_wrong_type_arg (func, arg);
      }
    }
  else
    print_usage ();

  return retval;
}

DEFUN_DLD (cummin, args, nargout,
  "-*- texinfo -*-\n\
@deftypefn  {Loadable Function} {} cummin (@var{x})\n\
@deftypefnx {Loadable Function} {} cummin (@var{x}, @var{dim})\n\
@deftypefnx {Loadable Function} {[@var{w}, @var{iw}] =} cummin (@var{x})\n\
Return the cumulative minimum values along dimension @var{dim}.  If @var{dim}\n\
is unspecified it defaults to column-wise operation.  For example:\n\
\n\
@example\n\
@group\n\
cummin ([5 4 6 2 3 1])\n\
    @result{}  5  4  4  2  2  1\n\
@end group\n\
@end example\n\
\n\
\n\
The call\n\
\n\
@example\n\
  [w, iw] = cummin (x)\n\
@end example\n\
\n\
@noindent\n\
with @code{x} a vector, is equivalent to the following code:\n\
\n\
@example\n\
@group\n\
w = iw = zeros (size (x));\n\
for i = 1:length (x)\n\
  [w(i), iw(i)] = max (x(1:i));\n\
endfor\n\
@end group\n\
@end example\n\
\n\
@noindent\n\
but computed in a much faster manner.\n\
@seealso{cummax, min, max}\n\
@end deftypefn")
{
  return do_cumminmax_body (args, nargout, true);
}

DEFUN_DLD (cummax, args, nargout,
  "-*- texinfo -*-\n\
@deftypefn  {Loadable Function} {} cummax (@var{x})\n\
@deftypefnx {Loadable Function} {} cummax (@var{x}, @var{dim})\n\
@deftypefnx {Loadable Function} {[@var{w}, @var{iw}] =} cummax (@var{x})\n\
Return the cumulative maximum values along dimension @var{dim}.  If @var{dim}\n\
is unspecified it defaults to column-wise operation.  For example:\n\
\n\
@example\n\
@group\n\
cummax ([1 3 2 6 4 5])\n\
    @result{}  1  3  3  6  6  6\n\
@end group\n\
@end example\n\
\n\
The call\n\
\n\
@example\n\
[w, iw] = cummax (x, dim)\n\
@end example\n\
\n\
@noindent\n\
with @code{x} a vector, is equivalent to the following code:\n\
\n\
@example\n\
@group\n\
w = iw = zeros (size (x));\n\
for i = 1:length (x)\n\
  [w(i), iw(i)] = max (x(1:i));\n\
endfor\n\
@end group\n\
@end example\n\
\n\
@noindent\n\
but computed in a much faster manner.\n\
@seealso{cummin, max, min}\n\
@end deftypefn")
{
  return do_cumminmax_body (args, nargout, false);
}
