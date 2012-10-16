## Copyright (C) 2009-2011 John W. Eaton
##
## This program is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or (at
## your option) any later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; see the file COPYING.  If not, see
## <http://www.gnu.org/licenses/>.

args = argv ();

if (nargin < 2)
  error ("usage: mk_doc_cache OUTPUT-FILE DOCSTRINGS-FILE ...");
endif

output_file = args{1};
docstrings_files = args(2:end);

## Special character used as break between DOCSTRINGS
doc_delim = char (31);

## Read the contents of all the DOCSTRINGS files into TEXT.
## It is more efficient to fork to shell for makeinfo only once on large data

nfiles = numel (docstrings_files);
text = cell (1, nfiles+1);
for i = 1:nfiles
  file = docstrings_files{i};
  fid = fopen (file, "r");
  if (fid < 0)
    error ("unable to open %s for reading", file);
  else
    tmp = fread (fid, Inf, "*char")';
    ## Strip off header lines
    [null, text{i}] = strtok (tmp, doc_delim);
  endif
endfor
text = [text{:}, doc_delim];

## Modify Octave-specific macros before passing to makeinfo
text = regexprep (text, '@seealso *\{([^}]*)\}', "See also: $1.");
text = regexprep (text, '@nospell *\{([^}]*)\}', "$1");
text = regexprep (text, "-\\*- texinfo -\\*-[ \t]*[\r\n]*", "");
text = regexprep (text, '@', "@@");

## Write data to temporary file for input to makeinfo
[fid, name, msg] = mkstemp ("octave_doc_XXXXXX", true);
if (fid < 0)
  error ("%s: %s\n", name, msg);
endif
fwrite (fid, text, "char");
fclose (fid);

cmd = sprintf ("%s --no-headers --no-warn --force --no-validate --fill-column=1024 %s",
               makeinfo_program (), name);

[status, formatted_text] = system (cmd);

## Did we get the help text?
if (status != 0)
  error ("makeinfo failed with exit status %d!", status);
endif

if (isempty (formatted_text))
  error ("makeinfo produced no output!");
endif

## Break apart output and store in cache variable
delim_idx = find (formatted_text == doc_delim);
n = length (delim_idx);

cache = cell (3, n);    # pre-allocate storage for efficiency
k = 1;

for i = 2:n

  block = formatted_text(delim_idx(i-1)+1:delim_idx(i)-1);

  [symbol, doc] = strtok (block, "\r\n");

  doc = regexprep (doc, "^[\r\n]+", '');

  ## Skip internal functions that start with __ as these aren't
  ## indexed by lookfor.
  if (length (symbol) > 2 && regexp (symbol, '^__.+__$'))
    continue;
  endif

  if (isempty (doc))
    continue;
  endif

  tmp = doc;
  found = 0;
  do
    [s, e] = regexp (tmp, "^ -- [^\r\n]*[\r\n]");
    if (! isempty(s))
      found = 1;
      tmp = tmp(e+1:end);
    endif
  until (isempty (s))

  if (! found)
    continue;
  endif

  end_of_first_sentence = regexp (tmp, "(\\.|[\r\n][\r\n])", "once");
  if (isempty (end_of_first_sentence))
    end_of_first_sentence = length (tmp);
  else
    end_of_first_sentence = end_of_first_sentence;
  endif

  first_sentence = tmp(1:end_of_first_sentence);
  first_sentence = regexprep (first_sentence, "([\r\n]| {2,})", " ");
  first_sentence = regexprep (first_sentence, '^ +', "");

  cache{1,k} = symbol;
  cache{2,k} = doc;
  cache{3,k} = first_sentence;
  k++;
endfor

cache(:,k:end) = [];    # delete unused pre-allocated entries

save ("-text", output_file, "cache");
