#ifndef lint
static const char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif

#include <stdlib.h>
#include <string.h>

#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYPATCH 20070509

#define YYEMPTY (-1)
#define yyclearin    (yychar = YYEMPTY)
#define yyerrok      (yyerrflag = 0)
#define YYRECOVERING (yyerrflag != 0)

extern int yyparse(void);

static int yygrowstack(void);
#define YYPREFIX "yy"
#line 17 "parser.y"

#define yylex yylex

char cvsroot_parser_y[] = "$Id: parser.y 13504 2012-08-04 20:24:22Z wsfulton $";

#include "swig.h"
#include "cparse.h"
#include "preprocessor.h"
#include <ctype.h>

/* We do this for portability */
#undef alloca
#define alloca malloc

/* -----------------------------------------------------------------------------
 *                               Externals
 * ----------------------------------------------------------------------------- */

int  yyparse();

/* NEW Variables */

static Node    *top = 0;      /* Top of the generated parse tree */
static int      unnamed = 0;  /* Unnamed datatype counter */
static Hash    *extendhash = 0;     /* Hash table of added methods */
static Hash    *classes = 0;        /* Hash table of classes */
static Symtab  *prev_symtab = 0;
static Node    *current_class = 0;
String  *ModuleName = 0;
static Node    *module_node = 0;
static String  *Classprefix = 0;  
static String  *Namespaceprefix = 0;
static int      inclass = 0;
static int      nested_template = 0; /* template class/function definition within a class */
static char    *last_cpptype = 0;
static int      inherit_list = 0;
static Parm    *template_parameters = 0;
static int      extendmode   = 0;
static int      compact_default_args = 0;
static int      template_reduce = 0;
static int      cparse_externc = 0;

static int      max_class_levels = 0;
static int      class_level = 0;
static Node   **class_decl = NULL;

/* -----------------------------------------------------------------------------
 *                            Assist Functions
 * ----------------------------------------------------------------------------- */


 
/* Called by the parser (yyparse) when an error is found.*/
static void yyerror (const char *e) {
  (void)e;
}

static Node *new_node(const_String_or_char_ptr tag) {
  Node *n = NewHash();
  set_nodeType(n,tag);
  Setfile(n,cparse_file);
  Setline(n,cparse_line);
  return n;
}

/* Copies a node.  Does not copy tree links or symbol table data (except for
   sym:name) */

static Node *copy_node(Node *n) {
  Node *nn;
  Iterator k;
  nn = NewHash();
  Setfile(nn,Getfile(n));
  Setline(nn,Getline(n));
  for (k = First(n); k.key; k = Next(k)) {
    String *ci;
    String *key = k.key;
    char *ckey = Char(key);
    if ((strcmp(ckey,"nextSibling") == 0) ||
	(strcmp(ckey,"previousSibling") == 0) ||
	(strcmp(ckey,"parentNode") == 0) ||
	(strcmp(ckey,"lastChild") == 0)) {
      continue;
    }
    if (Strncmp(key,"csym:",5) == 0) continue;
    /* We do copy sym:name.  For templates */
    if ((strcmp(ckey,"sym:name") == 0) || 
	(strcmp(ckey,"sym:weak") == 0) ||
	(strcmp(ckey,"sym:typename") == 0)) {
      String *ci = Copy(k.item);
      Setattr(nn,key, ci);
      Delete(ci);
      continue;
    }
    if (strcmp(ckey,"sym:symtab") == 0) {
      Setattr(nn,"sym:needs_symtab", "1");
    }
    /* We don't copy any other symbol table attributes */
    if (strncmp(ckey,"sym:",4) == 0) {
      continue;
    }
    /* If children.  We copy them recursively using this function */
    if (strcmp(ckey,"firstChild") == 0) {
      /* Copy children */
      Node *cn = k.item;
      while (cn) {
	Node *copy = copy_node(cn);
	appendChild(nn,copy);
	Delete(copy);
	cn = nextSibling(cn);
      }
      continue;
    }
    /* We don't copy the symbol table.  But we drop an attribute 
       requires_symtab so that functions know it needs to be built */

    if (strcmp(ckey,"symtab") == 0) {
      /* Node defined a symbol table. */
      Setattr(nn,"requires_symtab","1");
      continue;
    }
    /* Can't copy nodes */
    if (strcmp(ckey,"node") == 0) {
      continue;
    }
    if ((strcmp(ckey,"parms") == 0) || (strcmp(ckey,"pattern") == 0) || (strcmp(ckey,"throws") == 0)
	|| (strcmp(ckey,"kwargs") == 0)) {
      ParmList *pl = CopyParmList(k.item);
      Setattr(nn,key,pl);
      Delete(pl);
      continue;
    }
    /* Looks okay.  Just copy the data using Copy */
    ci = Copy(k.item);
    Setattr(nn, key, ci);
    Delete(ci);
  }
  return nn;
}

/* -----------------------------------------------------------------------------
 *                              Variables
 * ----------------------------------------------------------------------------- */

static char  *typemap_lang = 0;    /* Current language setting */

static int cplus_mode  = 0;
static String  *class_rename = 0;

/* C++ modes */

#define  CPLUS_PUBLIC    1
#define  CPLUS_PRIVATE   2
#define  CPLUS_PROTECTED 3

/* include types */
static int   import_mode = 0;

void SWIG_typemap_lang(const char *tm_lang) {
  typemap_lang = Swig_copy_string(tm_lang);
}

void SWIG_cparse_set_compact_default_args(int defargs) {
  compact_default_args = defargs;
}

int SWIG_cparse_template_reduce(int treduce) {
  template_reduce = treduce;
  return treduce;  
}

/* -----------------------------------------------------------------------------
 *                           Assist functions
 * ----------------------------------------------------------------------------- */

static int promote_type(int t) {
  if (t <= T_UCHAR || t == T_CHAR) return T_INT;
  return t;
}

/* Perform type-promotion for binary operators */
static int promote(int t1, int t2) {
  t1 = promote_type(t1);
  t2 = promote_type(t2);
  return t1 > t2 ? t1 : t2;
}

static String *yyrename = 0;

/* Forward renaming operator */

static String *resolve_create_node_scope(String *cname);


Hash *Swig_cparse_features(void) {
  static Hash   *features_hash = 0;
  if (!features_hash) features_hash = NewHash();
  return features_hash;
}

/* Fully qualify any template parameters */
static String *feature_identifier_fix(String *s) {
  String *tp = SwigType_istemplate_templateprefix(s);
  if (tp) {
    String *ts, *ta, *tq;
    ts = SwigType_templatesuffix(s);
    ta = SwigType_templateargs(s);
    tq = Swig_symbol_type_qualify(ta,0);
    Append(tp,tq);
    Append(tp,ts);
    Delete(ts);
    Delete(ta);
    Delete(tq);
    return tp;
  } else {
    return NewString(s);
  }
}

/* Generate the symbol table name for an object */
/* This is a bit of a mess. Need to clean up */
static String *add_oldname = 0;



static String *make_name(Node *n, String *name,SwigType *decl) {
  int destructor = name && (*(Char(name)) == '~');

  if (yyrename) {
    String *s = NewString(yyrename);
    Delete(yyrename);
    yyrename = 0;
    if (destructor  && (*(Char(s)) != '~')) {
      Insert(s,0,"~");
    }
    return s;
  }

  if (!name) return 0;
  return Swig_name_make(n,Namespaceprefix,name,decl,add_oldname);
}

/* Generate an unnamed identifier */
static String *make_unnamed() {
  unnamed++;
  return NewStringf("$unnamed%d$",unnamed);
}

/* Return if the node is a friend declaration */
static int is_friend(Node *n) {
  return Cmp(Getattr(n,"storage"),"friend") == 0;
}

static int is_operator(String *name) {
  return Strncmp(name,"operator ", 9) == 0;
}


/* Add declaration list to symbol table */
static int  add_only_one = 0;

static void add_symbols(Node *n) {
  String *decl;
  String *wrn = 0;

  if (nested_template) {
    if (!(n && Equal(nodeType(n), "template"))) {
      return;
    }
    /* continue if template function, but not template class, declared within a class */
  }

  if (inclass && n) {
    cparse_normalize_void(n);
  }
  while (n) {
    String *symname = 0;
    /* for friends, we need to pop the scope once */
    String *old_prefix = 0;
    Symtab *old_scope = 0;
    int isfriend = inclass && is_friend(n);
    int iscdecl = Cmp(nodeType(n),"cdecl") == 0;
    int only_csymbol = 0;
    if (extendmode) {
      Setattr(n,"isextension","1");
    }
    
    if (inclass) {
      String *name = Getattr(n, "name");
      if (isfriend) {
	/* for friends, we need to add the scopename if needed */
	String *prefix = name ? Swig_scopename_prefix(name) : 0;
	old_prefix = Namespaceprefix;
	old_scope = Swig_symbol_popscope();
	Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	if (!prefix) {
	  if (name && !is_operator(name) && Namespaceprefix) {
	    String *nname = NewStringf("%s::%s", Namespaceprefix, name);
	    Setattr(n,"name",nname);
	    Delete(nname);
	  }
	} else {
	  Symtab *st = Swig_symbol_getscope(prefix);
	  String *ns = st ? Getattr(st,"name") : prefix;
	  String *base  = Swig_scopename_last(name);
	  String *nname = NewStringf("%s::%s", ns, base);
	  Setattr(n,"name",nname);
	  Delete(nname);
	  Delete(base);
	  Delete(prefix);
	}
	Namespaceprefix = 0;
      } else {
	/* for member functions, we need to remove the redundant
	   class scope if provided, as in
	   
	   struct Foo {
	   int Foo::method(int a);
	   };
	   
	*/
	String *prefix = name ? Swig_scopename_prefix(name) : 0;
	if (prefix) {
	  if (Classprefix && (Equal(prefix,Classprefix))) {
	    String *base = Swig_scopename_last(name);
	    Setattr(n,"name",base);
	    Delete(base);
	  }
	  Delete(prefix);
	}

        /*
	if (!Getattr(n,"parentNode") && class_level) set_parentNode(n,class_decl[class_level - 1]);
        */
	Setattr(n,"ismember","1");
      }
    }
    if (!isfriend && inclass) {
      if ((cplus_mode != CPLUS_PUBLIC)) {
	only_csymbol = 1;
	if (cplus_mode == CPLUS_PROTECTED) {
	  Setattr(n,"access", "protected");
	  only_csymbol = !Swig_need_protected(n);
	} else {
	  Setattr(n,"access", "private");
	  /* private are needed only when they are pure virtuals - why? */
	  if ((Cmp(Getattr(n,"storage"),"virtual") == 0) && (Cmp(Getattr(n,"value"),"0") == 0)) {
	    only_csymbol = 0;
	  }
	}
      } else {
	  Setattr(n,"access", "public");
      }
    }
    if (Getattr(n,"sym:name")) {
      n = nextSibling(n);
      continue;
    }
    decl = Getattr(n,"decl");
    if (!SwigType_isfunction(decl)) {
      String *name = Getattr(n,"name");
      String *makename = Getattr(n,"parser:makename");
      if (iscdecl) {	
	String *storage = Getattr(n, "storage");
	if (Cmp(storage,"typedef") == 0) {
	  Setattr(n,"kind","typedef");
	} else {
	  SwigType *type = Getattr(n,"type");
	  String *value = Getattr(n,"value");
	  Setattr(n,"kind","variable");
	  if (value && Len(value)) {
	    Setattr(n,"hasvalue","1");
	  }
	  if (type) {
	    SwigType *ty;
	    SwigType *tmp = 0;
	    if (decl) {
	      ty = tmp = Copy(type);
	      SwigType_push(ty,decl);
	    } else {
	      ty = type;
	    }
	    if (!SwigType_ismutable(ty)) {
	      SetFlag(n,"hasconsttype");
	      SetFlag(n,"feature:immutable");
	    }
	    if (tmp) Delete(tmp);
	  }
	  if (!type) {
	    Printf(stderr,"notype name %s\n", name);
	  }
	}
      }
      Swig_features_get(Swig_cparse_features(), Namespaceprefix, name, 0, n);
      if (makename) {
	symname = make_name(n, makename,0);
        Delattr(n,"parser:makename"); /* temporary information, don't leave it hanging around */
      } else {
        makename = name;
	symname = make_name(n, makename,0);
      }
      
      if (!symname) {
	symname = Copy(Getattr(n,"unnamed"));
      }
      if (symname) {
	wrn = Swig_name_warning(n, Namespaceprefix, symname,0);
      }
    } else {
      String *name = Getattr(n,"name");
      SwigType *fdecl = Copy(decl);
      SwigType *fun = SwigType_pop_function(fdecl);
      if (iscdecl) {	
	Setattr(n,"kind","function");
      }
      
      Swig_features_get(Swig_cparse_features(),Namespaceprefix,name,fun,n);

      symname = make_name(n, name,fun);
      wrn = Swig_name_warning(n, Namespaceprefix,symname,fun);
      
      Delete(fdecl);
      Delete(fun);
      
    }
    if (!symname) {
      n = nextSibling(n);
      continue;
    }
    if (only_csymbol || GetFlag(n,"feature:ignore")) {
      /* Only add to C symbol table and continue */
      Swig_symbol_add(0, n);
    } else if (strncmp(Char(symname),"$ignore",7) == 0) {
      char *c = Char(symname)+7;
      SetFlag(n,"feature:ignore");
      if (strlen(c)) {
	SWIG_WARN_NODE_BEGIN(n);
	Swig_warning(0,Getfile(n), Getline(n), "%s\n",c+1);
	SWIG_WARN_NODE_END(n);
      }
      Swig_symbol_add(0, n);
    } else {
      Node *c;
      if ((wrn) && (Len(wrn))) {
	String *metaname = symname;
	if (!Getmeta(metaname,"already_warned")) {
	  SWIG_WARN_NODE_BEGIN(n);
	  Swig_warning(0,Getfile(n),Getline(n), "%s\n", wrn);
	  SWIG_WARN_NODE_END(n);
	  Setmeta(metaname,"already_warned","1");
	}
      }
      c = Swig_symbol_add(symname,n);

      if (c != n) {
        /* symbol conflict attempting to add in the new symbol */
        if (Getattr(n,"sym:weak")) {
          Setattr(n,"sym:name",symname);
        } else {
          String *e = NewStringEmpty();
          String *en = NewStringEmpty();
          String *ec = NewStringEmpty();
          int redefined = Swig_need_redefined_warn(n,c,inclass);
          if (redefined) {
            Printf(en,"Identifier '%s' redefined (ignored)",symname);
            Printf(ec,"previous definition of '%s'",symname);
          } else {
            Printf(en,"Redundant redeclaration of '%s'",symname);
            Printf(ec,"previous declaration of '%s'",symname);
          }
          if (Cmp(symname,Getattr(n,"name"))) {
            Printf(en," (Renamed from '%s')", SwigType_namestr(Getattr(n,"name")));
          }
          Printf(en,",");
          if (Cmp(symname,Getattr(c,"name"))) {
            Printf(ec," (Renamed from '%s')", SwigType_namestr(Getattr(c,"name")));
          }
          Printf(ec,".");
	  SWIG_WARN_NODE_BEGIN(n);
          if (redefined) {
            Swig_warning(WARN_PARSE_REDEFINED,Getfile(n),Getline(n),"%s\n",en);
            Swig_warning(WARN_PARSE_REDEFINED,Getfile(c),Getline(c),"%s\n",ec);
          } else if (!is_friend(n) && !is_friend(c)) {
            Swig_warning(WARN_PARSE_REDUNDANT,Getfile(n),Getline(n),"%s\n",en);
            Swig_warning(WARN_PARSE_REDUNDANT,Getfile(c),Getline(c),"%s\n",ec);
          }
	  SWIG_WARN_NODE_END(n);
          Printf(e,"%s:%d:%s\n%s:%d:%s\n",Getfile(n),Getline(n),en,
                 Getfile(c),Getline(c),ec);
          Setattr(n,"error",e);
	  Delete(e);
          Delete(en);
          Delete(ec);
        }
      }
    }
    /* restore the class scope if needed */
    if (isfriend) {
      Swig_symbol_setscope(old_scope);
      if (old_prefix) {
	Delete(Namespaceprefix);
	Namespaceprefix = old_prefix;
      }
    }
    Delete(symname);

    if (add_only_one) return;
    n = nextSibling(n);
  }
}


/* add symbols a parse tree node copy */

static void add_symbols_copy(Node *n) {
  String *name;
  int    emode = 0;
  while (n) {
    char *cnodeType = Char(nodeType(n));

    if (strcmp(cnodeType,"access") == 0) {
      String *kind = Getattr(n,"kind");
      if (Strcmp(kind,"public") == 0) {
	cplus_mode = CPLUS_PUBLIC;
      } else if (Strcmp(kind,"private") == 0) {
	cplus_mode = CPLUS_PRIVATE;
      } else if (Strcmp(kind,"protected") == 0) {
	cplus_mode = CPLUS_PROTECTED;
      }
      n = nextSibling(n);
      continue;
    }

    add_oldname = Getattr(n,"sym:name");
    if ((add_oldname) || (Getattr(n,"sym:needs_symtab"))) {
      int old_inclass = -1;
      Node *old_current_class = 0;
      if (add_oldname) {
	DohIncref(add_oldname);
	/*  Disable this, it prevents %rename to work with templates */
	/* If already renamed, we used that name  */
	/*
	if (Strcmp(add_oldname, Getattr(n,"name")) != 0) {
	  Delete(yyrename);
	  yyrename = Copy(add_oldname);
	}
	*/
      }
      Delattr(n,"sym:needs_symtab");
      Delattr(n,"sym:name");

      add_only_one = 1;
      add_symbols(n);

      if (Getattr(n,"partialargs")) {
	Swig_symbol_cadd(Getattr(n,"partialargs"),n);
      }
      add_only_one = 0;
      name = Getattr(n,"name");
      if (Getattr(n,"requires_symtab")) {
	Swig_symbol_newscope();
	Swig_symbol_setscopename(name);
	Delete(Namespaceprefix);
	Namespaceprefix = Swig_symbol_qualifiedscopename(0);
      }
      if (strcmp(cnodeType,"class") == 0) {
	old_inclass = inclass;
	inclass = 1;
	old_current_class = current_class;
	current_class = n;
	if (Strcmp(Getattr(n,"kind"),"class") == 0) {
	  cplus_mode = CPLUS_PRIVATE;
	} else {
	  cplus_mode = CPLUS_PUBLIC;
	}
      }
      if (strcmp(cnodeType,"extend") == 0) {
	emode = cplus_mode;
	cplus_mode = CPLUS_PUBLIC;
      }
      add_symbols_copy(firstChild(n));
      if (strcmp(cnodeType,"extend") == 0) {
	cplus_mode = emode;
      }
      if (Getattr(n,"requires_symtab")) {
	Setattr(n,"symtab", Swig_symbol_popscope());
	Delattr(n,"requires_symtab");
	Delete(Namespaceprefix);
	Namespaceprefix = Swig_symbol_qualifiedscopename(0);
      }
      if (add_oldname) {
	Delete(add_oldname);
	add_oldname = 0;
      }
      if (strcmp(cnodeType,"class") == 0) {
	inclass = old_inclass;
	current_class = old_current_class;
      }
    } else {
      if (strcmp(cnodeType,"extend") == 0) {
	emode = cplus_mode;
	cplus_mode = CPLUS_PUBLIC;
      }
      add_symbols_copy(firstChild(n));
      if (strcmp(cnodeType,"extend") == 0) {
	cplus_mode = emode;
      }
    }
    n = nextSibling(n);
  }
}

/* Extension merge.  This function is used to handle the %extend directive
   when it appears before a class definition.   To handle this, the %extend
   actually needs to take precedence.  Therefore, we will selectively nuke symbols
   from the current symbol table, replacing them with the added methods */

static void merge_extensions(Node *cls, Node *am) {
  Node *n;
  Node *csym;

  n = firstChild(am);
  while (n) {
    String *symname;
    if (Strcmp(nodeType(n),"constructor") == 0) {
      symname = Getattr(n,"sym:name");
      if (symname) {
	if (Strcmp(symname,Getattr(n,"name")) == 0) {
	  /* If the name and the sym:name of a constructor are the same,
             then it hasn't been renamed.  However---the name of the class
             itself might have been renamed so we need to do a consistency
             check here */
	  if (Getattr(cls,"sym:name")) {
	    Setattr(n,"sym:name", Getattr(cls,"sym:name"));
	  }
	}
      } 
    }

    symname = Getattr(n,"sym:name");
    DohIncref(symname);
    if ((symname) && (!Getattr(n,"error"))) {
      /* Remove node from its symbol table */
      Swig_symbol_remove(n);
      csym = Swig_symbol_add(symname,n);
      if (csym != n) {
	/* Conflict with previous definition.  Nuke previous definition */
	String *e = NewStringEmpty();
	String *en = NewStringEmpty();
	String *ec = NewStringEmpty();
	Printf(ec,"Identifier '%s' redefined by %%extend (ignored),",symname);
	Printf(en,"%%extend definition of '%s'.",symname);
	SWIG_WARN_NODE_BEGIN(n);
	Swig_warning(WARN_PARSE_REDEFINED,Getfile(csym),Getline(csym),"%s\n",ec);
	Swig_warning(WARN_PARSE_REDEFINED,Getfile(n),Getline(n),"%s\n",en);
	SWIG_WARN_NODE_END(n);
	Printf(e,"%s:%d:%s\n%s:%d:%s\n",Getfile(csym),Getline(csym),ec, 
	       Getfile(n),Getline(n),en);
	Setattr(csym,"error",e);
	Delete(e);
	Delete(en);
	Delete(ec);
	Swig_symbol_remove(csym);              /* Remove class definition */
	Swig_symbol_add(symname,n);            /* Insert extend definition */
      }
    }
    n = nextSibling(n);
  }
}

static void append_previous_extension(Node *cls, Node *am) {
  Node *n, *ne;
  Node *pe = 0;
  Node *ae = 0;

  if (!am) return;
  
  n = firstChild(am);
  while (n) {
    ne = nextSibling(n);
    set_nextSibling(n,0);
    /* typemaps and fragments need to be prepended */
    if (((Cmp(nodeType(n),"typemap") == 0) || (Cmp(nodeType(n),"fragment") == 0)))  {
      if (!pe) pe = new_node("extend");
      appendChild(pe, n);
    } else {
      if (!ae) ae = new_node("extend");
      appendChild(ae, n);
    }    
    n = ne;
  }
  if (pe) prependChild(cls,pe);
  if (ae) appendChild(cls,ae);
}
 

/* Check for unused %extend.  Special case, don't report unused
   extensions for templates */
 
static void check_extensions() {
  Iterator ki;

  if (!extendhash) return;
  for (ki = First(extendhash); ki.key; ki = Next(ki)) {
    if (!Strchr(ki.key,'<')) {
      SWIG_WARN_NODE_BEGIN(ki.item);
      Swig_warning(WARN_PARSE_EXTEND_UNDEF,Getfile(ki.item), Getline(ki.item), "%%extend defined for an undeclared class %s.\n", ki.key);
      SWIG_WARN_NODE_END(ki.item);
    }
  }
}

/* Check a set of declarations to see if any are pure-abstract */

static List *pure_abstract(Node *n) {
  List *abs = 0;
  while (n) {
    if (Cmp(nodeType(n),"cdecl") == 0) {
      String *decl = Getattr(n,"decl");
      if (SwigType_isfunction(decl)) {
	String *init = Getattr(n,"value");
	if (Cmp(init,"0") == 0) {
	  if (!abs) {
	    abs = NewList();
	  }
	  Append(abs,n);
	  Setattr(n,"abstract","1");
	}
      }
    } else if (Cmp(nodeType(n),"destructor") == 0) {
      if (Cmp(Getattr(n,"value"),"0") == 0) {
	if (!abs) {
	  abs = NewList();
	}
	Append(abs,n);
	Setattr(n,"abstract","1");
      }
    }
    n = nextSibling(n);
  }
  return abs;
}

/* Make a classname */

static String *make_class_name(String *name) {
  String *nname = 0;
  String *prefix;
  if (Namespaceprefix) {
    nname= NewStringf("%s::%s", Namespaceprefix, name);
  } else {
    nname = NewString(name);
  }
  prefix = SwigType_istemplate_templateprefix(nname);
  if (prefix) {
    String *args, *qargs;
    args   = SwigType_templateargs(nname);
    qargs  = Swig_symbol_type_qualify(args,0);
    Append(prefix,qargs);
    Delete(nname);
    Delete(args);
    Delete(qargs);
    nname = prefix;
  }
  return nname;
}

static List *make_inherit_list(String *clsname, List *names) {
  int i, ilen;
  String *derived;
  List *bases = NewList();

  if (Namespaceprefix) derived = NewStringf("%s::%s", Namespaceprefix,clsname);
  else derived = NewString(clsname);

  ilen = Len(names);
  for (i = 0; i < ilen; i++) {
    Node *s;
    String *base;
    String *n = Getitem(names,i);
    /* Try to figure out where this symbol is */
    s = Swig_symbol_clookup(n,0);
    if (s) {
      while (s && (Strcmp(nodeType(s),"class") != 0)) {
	/* Not a class.  Could be a typedef though. */
	String *storage = Getattr(s,"storage");
	if (storage && (Strcmp(storage,"typedef") == 0)) {
	  String *nn = Getattr(s,"type");
	  s = Swig_symbol_clookup(nn,Getattr(s,"sym:symtab"));
	} else {
	  break;
	}
      }
      if (s && ((Strcmp(nodeType(s),"class") == 0) || (Strcmp(nodeType(s),"template") == 0))) {
	String *q = Swig_symbol_qualified(s);
	Append(bases,s);
	if (q) {
	  base = NewStringf("%s::%s", q, Getattr(s,"name"));
	  Delete(q);
	} else {
	  base = NewString(Getattr(s,"name"));
	}
      } else {
	base = NewString(n);
      }
    } else {
      base = NewString(n);
    }
    if (base) {
      Swig_name_inherit(base,derived);
      Delete(base);
    }
  }
  return bases;
}

/* If the class name is qualified.  We need to create or lookup namespace entries */

static Symtab *set_scope_to_global() {
  Symtab *symtab = Swig_symbol_global_scope();
  Swig_symbol_setscope(symtab);
  return symtab;
}
 
/* Remove the block braces, { and }, if the 'noblock' attribute is set.
 * Node *kw can be either a Hash or Parmlist. */
static String *remove_block(Node *kw, const String *inputcode) {
  String *modified_code = 0;
  while (kw) {
   String *name = Getattr(kw,"name");
   if (name && (Cmp(name,"noblock") == 0)) {
     char *cstr = Char(inputcode);
     size_t len = Len(inputcode);
     if (len && cstr[0] == '{') {
       --len; ++cstr; 
       if (len && cstr[len - 1] == '}') { --len; }
       /* we now remove the extra spaces */
       while (len && isspace((int)cstr[0])) { --len; ++cstr; }
       while (len && isspace((int)cstr[len - 1])) { --len; }
       modified_code = NewStringWithSize(cstr, len);
       break;
     }
   }
   kw = nextSibling(kw);
  }
  return modified_code;
}


static Node *nscope = 0;
static Node *nscope_inner = 0;

/* Remove the scope prefix from cname and return the base name without the prefix.
 * The scopes required for the symbol name are resolved and/or created, if required.
 * For example AA::BB::CC as input returns CC and creates the namespace AA then inner 
 * namespace BB in the current scope. If cname is found to already exist as a weak symbol
 * (forward reference) then the scope might be changed to match, such as when a symbol match 
 * is made via a using reference. */
static String *resolve_create_node_scope(String *cname) {
  Symtab *gscope = 0;
  Node *cname_node = 0;
  int skip_lookup = 0;
  nscope = 0;
  nscope_inner = 0;  

  if (Strncmp(cname,"::",2) == 0)
    skip_lookup = 1;

  cname_node = skip_lookup ? 0 : Swig_symbol_clookup_no_inherit(cname, 0);

  if (cname_node) {
    /* The symbol has been defined already or is in another scope.
       If it is a weak symbol, it needs replacing and if it was brought into the current scope
       via a using declaration, the scope needs adjusting appropriately for the new symbol.
       Similarly for defined templates. */
    Symtab *symtab = Getattr(cname_node, "sym:symtab");
    Node *sym_weak = Getattr(cname_node, "sym:weak");
    if ((symtab && sym_weak) || Equal(nodeType(cname_node), "template")) {
      /* Check if the scope is the current scope */
      String *current_scopename = Swig_symbol_qualifiedscopename(0);
      String *found_scopename = Swig_symbol_qualifiedscopename(symtab);
      int len;
      if (!current_scopename)
	current_scopename = NewString("");
      if (!found_scopename)
	found_scopename = NewString("");
      len = Len(current_scopename);
      if ((len > 0) && (Strncmp(current_scopename, found_scopename, len) == 0)) {
	if (Len(found_scopename) > len + 2) {
	  /* A matching weak symbol was found in non-global scope, some scope adjustment may be required */
	  String *new_cname = NewString(Char(found_scopename) + len + 2); /* skip over "::" prefix */
	  String *base = Swig_scopename_last(cname);
	  Printf(new_cname, "::%s", base);
	  cname = new_cname;
	  Delete(base);
	} else {
	  /* A matching weak symbol was found in the same non-global local scope, no scope adjustment required */
	  assert(len == Len(found_scopename));
	}
      } else {
	String *base = Swig_scopename_last(cname);
	if (Len(found_scopename) > 0) {
	  /* A matching weak symbol was found in a different scope to the local scope - probably via a using declaration */
	  cname = NewStringf("%s::%s", found_scopename, base);
	} else {
	  /* Either:
	      1) A matching weak symbol was found in a different scope to the local scope - this is actually a
	      symbol with the same name in a different scope which we don't want, so no adjustment required.
	      2) A matching weak symbol was found in the global scope - no adjustment required.
	  */
	  cname = Copy(base);
	}
	Delete(base);
      }
      Delete(current_scopename);
      Delete(found_scopename);
    }
  }

  if (Swig_scopename_check(cname)) {
    Node   *ns;
    String *prefix = Swig_scopename_prefix(cname);
    String *base = Swig_scopename_last(cname);
    if (prefix && (Strncmp(prefix,"::",2) == 0)) {
/* I don't think we can use :: global scope to declare classes and hence neither %template. - consider reporting error instead - wsfulton. */
      /* Use the global scope */
      String *nprefix = NewString(Char(prefix)+2);
      Delete(prefix);
      prefix= nprefix;
      gscope = set_scope_to_global();
    }
    if (Len(prefix) == 0) {
      /* Use the global scope, but we need to add a 'global' namespace.  */
      if (!gscope) gscope = set_scope_to_global();
      /* note that this namespace is not the "unnamed" one,
	 and we don't use Setattr(nscope,"name", ""),
	 because the unnamed namespace is private */
      nscope = new_node("namespace");
      Setattr(nscope,"symtab", gscope);;
      nscope_inner = nscope;
      return base;
    }
    /* Try to locate the scope */
    ns = Swig_symbol_clookup(prefix,0);
    if (!ns) {
      Swig_error(cparse_file,cparse_line,"Undefined scope '%s'\n", prefix);
    } else {
      Symtab *nstab = Getattr(ns,"symtab");
      if (!nstab) {
	Swig_error(cparse_file,cparse_line, "'%s' is not defined as a valid scope.\n", prefix);
	ns = 0;
      } else {
	/* Check if the node scope is the current scope */
	String *tname = Swig_symbol_qualifiedscopename(0);
	String *nname = Swig_symbol_qualifiedscopename(nstab);
	if (tname && (Strcmp(tname,nname) == 0)) {
	  ns = 0;
	  cname = base;
	}
	Delete(tname);
	Delete(nname);
      }
      if (ns) {
	/* we will try to create a new node using the namespaces we
	   can find in the scope name */
	List *scopes;
	String *sname;
	Iterator si;
	String *name = NewString(prefix);
	scopes = NewList();
	while (name) {
	  String *base = Swig_scopename_last(name);
	  String *tprefix = Swig_scopename_prefix(name);
	  Insert(scopes,0,base);
	  Delete(base);
	  Delete(name);
	  name = tprefix;
	}
	for (si = First(scopes); si.item; si = Next(si)) {
	  Node *ns1,*ns2;
	  sname = si.item;
	  ns1 = Swig_symbol_clookup(sname,0);
	  assert(ns1);
	  if (Strcmp(nodeType(ns1),"namespace") == 0) {
	    if (Getattr(ns1,"alias")) {
	      ns1 = Getattr(ns1,"namespace");
	    }
	  } else {
	    /* now this last part is a class */
	    si = Next(si);
	    ns1 = Swig_symbol_clookup(sname,0);
	    /*  or a nested class tree, which is unrolled here */
	    for (; si.item; si = Next(si)) {
	      if (si.item) {
		Printf(sname,"::%s",si.item);
	      }
	    }
	    /* we get the 'inner' class */
	    nscope_inner = Swig_symbol_clookup(sname,0);
	    /* set the scope to the inner class */
	    Swig_symbol_setscope(Getattr(nscope_inner,"symtab"));
	    /* save the last namespace prefix */
	    Delete(Namespaceprefix);
	    Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	    /* and return the node name, including the inner class prefix */
	    break;
	  }
	  /* here we just populate the namespace tree as usual */
	  ns2 = new_node("namespace");
	  Setattr(ns2,"name",sname);
	  Setattr(ns2,"symtab", Getattr(ns1,"symtab"));
	  add_symbols(ns2);
	  Swig_symbol_setscope(Getattr(ns1,"symtab"));
	  Delete(Namespaceprefix);
	  Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	  if (nscope_inner) {
	    if (Getattr(nscope_inner,"symtab") != Getattr(ns2,"symtab")) {
	      appendChild(nscope_inner,ns2);
	      Delete(ns2);
	    }
	  }
	  nscope_inner = ns2;
	  if (!nscope) nscope = ns2;
	}
	cname = base;
	Delete(scopes);
      }
    }
    Delete(prefix);
  }

  return cname;
}
 


/* Structures for handling code fragments built for nested classes */

typedef struct Nested {
  String   *code;        /* Associated code fragment */
  int      line;         /* line number where it starts */
  const char *name;      /* Name associated with this nested class */
  const char *kind;      /* Kind of class */
  int      unnamed;      /* unnamed class */
  SwigType *type;        /* Datatype associated with the name */
  struct Nested   *next; /* Next code fragment in list */
} Nested;

/* Some internal variables for saving nested class information */

static Nested      *nested_list = 0;

/* Add a function to the nested list */

static void add_nested(Nested *n) {
  if (!nested_list) {
    nested_list = n;
  } else {
    Nested *n1 = nested_list;
    while (n1->next)
      n1 = n1->next;
    n1->next = n;
  }
}

/* -----------------------------------------------------------------------------
 * nested_new_struct()
 *
 * Nested struct handling for C code only creates a global struct from the nested struct.
 *
 * Nested structure. This is a sick "hack". If we encounter
 * a nested structure, we're going to grab the text of its definition and
 * feed it back into the scanner.  In the meantime, we need to grab
 * variable declaration information and generate the associated wrapper
 * code later.  Yikes!
 *
 * This really only works in a limited sense.   Since we use the
 * code attached to the nested class to generate both C code
 * it can't have any SWIG directives in it.  It also needs to be parsable
 * by SWIG or this whole thing is going to puke.
 * ----------------------------------------------------------------------------- */

static void nested_new_struct(const char *kind, String *struct_code, Node *cpp_opt_declarators) {
  String *name;
  String *decl;

  /* Create a new global struct declaration which is just a copy of the nested struct */
  Nested *nested = (Nested *) malloc(sizeof(Nested));
  Nested *n = nested;

  name = Getattr(cpp_opt_declarators, "name");
  decl = Getattr(cpp_opt_declarators, "decl");

  n->code = NewStringEmpty();
  Printv(n->code, "typedef ", kind, " ", struct_code, " $classname_", name, ";\n", NIL);
  n->name = Swig_copy_string(Char(name));
  n->line = cparse_start_line;
  n->type = NewStringEmpty();
  n->kind = kind;
  n->unnamed = 0;
  SwigType_push(n->type, decl);
  n->next = 0;

  /* Repeat for any multiple instances of the nested struct */
  {
    Node *p = cpp_opt_declarators;
    p = nextSibling(p);
    while (p) {
      Nested *nn = (Nested *) malloc(sizeof(Nested));

      name = Getattr(p, "name");
      decl = Getattr(p, "decl");

      nn->code = NewStringEmpty();
      Printv(nn->code, "typedef ", kind, " ", struct_code, " $classname_", name, ";\n", NIL);
      nn->name = Swig_copy_string(Char(name));
      nn->line = cparse_start_line;
      nn->type = NewStringEmpty();
      nn->kind = kind;
      nn->unnamed = 0;
      SwigType_push(nn->type, decl);
      nn->next = 0;
      n->next = nn;
      n = nn;
      p = nextSibling(p);
    }
  }

  add_nested(nested);
}

/* -----------------------------------------------------------------------------
 * nested_forward_declaration()
 * 
 * Nested struct handling for C++ code only.
 *
 * Treat the nested class/struct/union as a forward declaration until a proper 
 * nested class solution is implemented.
 * ----------------------------------------------------------------------------- */

static Node *nested_forward_declaration(const char *storage, const char *kind, String *sname, const char *name, Node *cpp_opt_declarators) {
  Node *nn = 0;
  int warned = 0;

  if (sname) {
    /* Add forward declaration of the nested type */
    Node *n = new_node("classforward");
    Setfile(n, cparse_file);
    Setline(n, cparse_line);
    Setattr(n, "kind", kind);
    Setattr(n, "name", sname);
    Setattr(n, "storage", storage);
    Setattr(n, "sym:weak", "1");
    add_symbols(n);
    nn = n;
  }

  /* Add any variable instances. Also add in any further typedefs of the nested type.
     Note that anonymous typedefs (eg typedef struct {...} a, b;) are treated as class forward declarations */
  if (cpp_opt_declarators) {
    int storage_typedef = (storage && (strcmp(storage, "typedef") == 0));
    int variable_of_anonymous_type = !sname && !storage_typedef;
    if (!variable_of_anonymous_type) {
      int anonymous_typedef = !sname && (storage && (strcmp(storage, "typedef") == 0));
      Node *n = cpp_opt_declarators;
      SwigType *type = NewString(name);
      while (n) {
	Setattr(n, "type", type);
	Setattr(n, "storage", storage);
	if (anonymous_typedef) {
	  Setattr(n, "nodeType", "classforward");
	  Setattr(n, "sym:weak", "1");
	}
	n = nextSibling(n);
      }
      Delete(type);
      add_symbols(cpp_opt_declarators);

      if (nn) {
	set_nextSibling(nn, cpp_opt_declarators);
      } else {
	nn = cpp_opt_declarators;
      }
    }
  }

  if (nn && Equal(nodeType(nn), "classforward")) {
    Node *n = nn;
    if (GetFlag(n, "feature:nestedworkaround")) {
      Swig_symbol_remove(n);
      nn = 0;
      warned = 1;
    } else {
      SWIG_WARN_NODE_BEGIN(n);
      Swig_warning(WARN_PARSE_NAMED_NESTED_CLASS, cparse_file, cparse_line,"Nested %s not currently supported (%s ignored)\n", kind, sname ? sname : name);
      SWIG_WARN_NODE_END(n);
      warned = 1;
    }
  }

  if (!warned)
    Swig_warning(WARN_PARSE_UNNAMED_NESTED_CLASS, cparse_file, cparse_line, "Nested %s not currently supported (ignored).\n", kind);

  return nn;
}

/* Strips C-style and C++-style comments from string in-place. */
static void strip_comments(char *string) {
  int state = 0; /* 
                  * 0 - not in comment
                  * 1 - in c-style comment
                  * 2 - in c++-style comment
                  * 3 - in string
                  * 4 - after reading / not in comments
                  * 5 - after reading * in c-style comments
                  * 6 - after reading \ in strings
                  */
  char * c = string;
  while (*c) {
    switch (state) {
    case 0:
      if (*c == '\"')
        state = 3;
      else if (*c == '/')
        state = 4;
      break;
    case 1:
      if (*c == '*')
        state = 5;
      *c = ' ';
      break;
    case 2:
      if (*c == '\n')
        state = 0;
      else
        *c = ' ';
      break;
    case 3:
      if (*c == '\"')
        state = 0;
      else if (*c == '\\')
        state = 6;
      break;
    case 4:
      if (*c == '/') {
        *(c-1) = ' ';
        *c = ' ';
        state = 2;
      } else if (*c == '*') {
        *(c-1) = ' ';
        *c = ' ';
        state = 1;
      } else
        state = 0;
      break;
    case 5:
      if (*c == '/')
        state = 0;
      else 
        state = 1;
      *c = ' ';
      break;
    case 6:
      state = 3;
      break;
    }
    ++c;
  }
}

/* Dump all of the nested class declarations to the inline processor
 * However.  We need to do a few name replacements and other munging
 * first.  This function must be called before closing a class! */

static Node *dump_nested(const char *parent) {
  Nested *n,*n1;
  Node *ret = 0;
  Node *last = 0;
  n = nested_list;
  if (!parent) {
    nested_list = 0;
    return 0;
  }
  while (n) {
    Node *retx;
    SwigType *nt;
    /* Token replace the name of the parent class */
    Replace(n->code, "$classname", parent, DOH_REPLACE_ANY);

    /* Fix up the name of the datatype (for building typedefs and other stuff) */
    Append(n->type,parent);
    Append(n->type,"_");
    Append(n->type,n->name);

    /* Add the appropriate declaration to the C++ processor */
    retx = new_node("cdecl");
    Setattr(retx,"name",n->name);
    nt = Copy(n->type);
    Setattr(retx,"type",nt);
    Delete(nt);
    Setattr(retx,"nested",parent);
    if (n->unnamed) {
      Setattr(retx,"unnamed","1");
    }
    
    add_symbols(retx);
    if (ret) {
      set_nextSibling(last, retx);
      Delete(retx);
    } else {
      ret = retx;
    }
    last = retx;

    /* Strip comments - further code may break in presence of comments. */
    strip_comments(Char(n->code));

    /* Make all SWIG created typedef structs/unions/classes unnamed else 
       redefinition errors occur - nasty hack alert.*/

    {
      const char* types_array[3] = {"struct", "union", "class"};
      int i;
      for (i=0; i<3; i++) {
	char* code_ptr = Char(n->code);
	while (code_ptr) {
	  /* Replace struct name (as in 'struct name {...}' ) with whitespace
	     name will be between struct and opening brace */
	
	  code_ptr = strstr(code_ptr, types_array[i]);
	  if (code_ptr) {
	    char *open_bracket_pos;
	    code_ptr += strlen(types_array[i]);
	    open_bracket_pos = strchr(code_ptr, '{');
	    if (open_bracket_pos) { 
	      /* Make sure we don't have something like struct A a; */
	      char* semi_colon_pos = strchr(code_ptr, ';');
	      if (!(semi_colon_pos && (semi_colon_pos < open_bracket_pos)))
		while (code_ptr < open_bracket_pos)
		  *code_ptr++ = ' ';
	    }
	  }
	}
      }
    }
    
    {
      /* Remove SWIG directive %constant which may be left in the SWIG created typedefs */
      char* code_ptr = Char(n->code);
      while (code_ptr) {
	code_ptr = strstr(code_ptr, "%constant");
	if (code_ptr) {
	  char* directive_end_pos = strchr(code_ptr, ';');
	  if (directive_end_pos) { 
            while (code_ptr <= directive_end_pos)
              *code_ptr++ = ' ';
	  }
	}
      }
    }
    {
      Node *newnode = new_node("insert");
      String *code = NewStringEmpty();
      Wrapper_pretty_print(n->code, code);
      Setattr(newnode,"code", code);
      Delete(code);
      set_nextSibling(last, newnode);
      Delete(newnode);      
      last = newnode;
    }
      
    /* Dump the code to the scanner */
    start_inline(Char(Getattr(last, "code")),n->line);

    n1 = n->next;
    Delete(n->code);
    free(n);
    n = n1;
  }
  nested_list = 0;
  return ret;
}

Node *Swig_cparse(File *f) {
  scanner_file(f);
  top = 0;
  yyparse();
  return top;
}

static void single_new_feature(const char *featurename, String *val, Hash *featureattribs, char *declaratorid, SwigType *type, ParmList *declaratorparms, String *qualifier) {
  String *fname;
  String *name;
  String *fixname;
  SwigType *t = Copy(type);

  /* Printf(stdout, "single_new_feature: [%s] [%s] [%s] [%s] [%s] [%s]\n", featurename, val, declaratorid, t, ParmList_str_defaultargs(declaratorparms), qualifier); */

  fname = NewStringf("feature:%s",featurename);
  if (declaratorid) {
    fixname = feature_identifier_fix(declaratorid);
  } else {
    fixname = NewStringEmpty();
  }
  if (Namespaceprefix) {
    name = NewStringf("%s::%s",Namespaceprefix, fixname);
  } else {
    name = fixname;
  }

  if (declaratorparms) Setmeta(val,"parms",declaratorparms);
  if (!Len(t)) t = 0;
  if (t) {
    if (qualifier) SwigType_push(t,qualifier);
    if (SwigType_isfunction(t)) {
      SwigType *decl = SwigType_pop_function(t);
      if (SwigType_ispointer(t)) {
	String *nname = NewStringf("*%s",name);
	Swig_feature_set(Swig_cparse_features(), nname, decl, fname, val, featureattribs);
	Delete(nname);
      } else {
	Swig_feature_set(Swig_cparse_features(), name, decl, fname, val, featureattribs);
      }
      Delete(decl);
    } else if (SwigType_ispointer(t)) {
      String *nname = NewStringf("*%s",name);
      Swig_feature_set(Swig_cparse_features(),nname,0,fname,val, featureattribs);
      Delete(nname);
    }
  } else {
    /* Global feature, that is, feature not associated with any particular symbol */
    Swig_feature_set(Swig_cparse_features(),name,0,fname,val, featureattribs);
  }
  Delete(fname);
  Delete(name);
}

/* Add a new feature to the Hash. Additional features are added if the feature has a parameter list (declaratorparms)
 * and one or more of the parameters have a default argument. An extra feature is added for each defaulted parameter,
 * simulating the equivalent overloaded method. */
static void new_feature(const char *featurename, String *val, Hash *featureattribs, char *declaratorid, SwigType *type, ParmList *declaratorparms, String *qualifier) {

  ParmList *declparms = declaratorparms;

  /* remove the { and } braces if the noblock attribute is set */
  String *newval = remove_block(featureattribs, val);
  val = newval ? newval : val;

  /* Add the feature */
  single_new_feature(featurename, val, featureattribs, declaratorid, type, declaratorparms, qualifier);

  /* Add extra features if there are default parameters in the parameter list */
  if (type) {
    while (declparms) {
      if (ParmList_has_defaultargs(declparms)) {

        /* Create a parameter list for the new feature by copying all
           but the last (defaulted) parameter */
        ParmList* newparms = CopyParmListMax(declparms, ParmList_len(declparms)-1);

        /* Create new declaration - with the last parameter removed */
        SwigType *newtype = Copy(type);
        Delete(SwigType_pop_function(newtype)); /* remove the old parameter list from newtype */
        SwigType_add_function(newtype,newparms);

        single_new_feature(featurename, Copy(val), featureattribs, declaratorid, newtype, newparms, qualifier);
        declparms = newparms;
      } else {
        declparms = 0;
      }
    }
  }
}

/* check if a function declaration is a plain C object */
static int is_cfunction(Node *n) {
  if (!cparse_cplusplus || cparse_externc) return 1;
  if (Cmp(Getattr(n,"storage"),"externc") == 0) {
    return 1;
  }
  return 0;
}

/* If the Node is a function with parameters, check to see if any of the parameters
 * have default arguments. If so create a new function for each defaulted argument. 
 * The additional functions form a linked list of nodes with the head being the original Node n. */
static void default_arguments(Node *n) {
  Node *function = n;

  if (function) {
    ParmList *varargs = Getattr(function,"feature:varargs");
    if (varargs) {
      /* Handles the %varargs directive by looking for "feature:varargs" and 
       * substituting ... with an alternative set of arguments.  */
      Parm     *p = Getattr(function,"parms");
      Parm     *pp = 0;
      while (p) {
	SwigType *t = Getattr(p,"type");
	if (Strcmp(t,"v(...)") == 0) {
	  if (pp) {
	    ParmList *cv = Copy(varargs);
	    set_nextSibling(pp,cv);
	    Delete(cv);
	  } else {
	    ParmList *cv =  Copy(varargs);
	    Setattr(function,"parms", cv);
	    Delete(cv);
	  }
	  break;
	}
	pp = p;
	p = nextSibling(p);
      }
    }

    /* Do not add in functions if kwargs is being used or if user wants old default argument wrapping
       (one wrapped method per function irrespective of number of default arguments) */
    if (compact_default_args 
	|| is_cfunction(function) 
	|| GetFlag(function,"feature:compactdefaultargs") 
	|| GetFlag(function,"feature:kwargs")) {
      ParmList *p = Getattr(function,"parms");
      if (p) 
        Setattr(p,"compactdefargs", "1"); /* mark parameters for special handling */
      function = 0; /* don't add in extra methods */
    }
  }

  while (function) {
    ParmList *parms = Getattr(function,"parms");
    if (ParmList_has_defaultargs(parms)) {

      /* Create a parameter list for the new function by copying all
         but the last (defaulted) parameter */
      ParmList* newparms = CopyParmListMax(parms,ParmList_len(parms)-1);

      /* Create new function and add to symbol table */
      {
	SwigType *ntype = Copy(nodeType(function));
	char *cntype = Char(ntype);
        Node *new_function = new_node(ntype);
        SwigType *decl = Copy(Getattr(function,"decl"));
        int constqualifier = SwigType_isconst(decl);
	String *ccode = Copy(Getattr(function,"code"));
	String *cstorage = Copy(Getattr(function,"storage"));
	String *cvalue = Copy(Getattr(function,"value"));
	SwigType *ctype = Copy(Getattr(function,"type"));
	String *cthrow = Copy(Getattr(function,"throw"));

        Delete(SwigType_pop_function(decl)); /* remove the old parameter list from decl */
        SwigType_add_function(decl,newparms);
        if (constqualifier)
          SwigType_add_qualifier(decl,"const");

        Setattr(new_function,"name", Getattr(function,"name"));
        Setattr(new_function,"code", ccode);
        Setattr(new_function,"decl", decl);
        Setattr(new_function,"parms", newparms);
        Setattr(new_function,"storage", cstorage);
        Setattr(new_function,"value", cvalue);
        Setattr(new_function,"type", ctype);
        Setattr(new_function,"throw", cthrow);

	Delete(ccode);
	Delete(cstorage);
	Delete(cvalue);
	Delete(ctype);
	Delete(cthrow);
	Delete(decl);

        {
          Node *throws = Getattr(function,"throws");
	  ParmList *pl = CopyParmList(throws);
          if (throws) Setattr(new_function,"throws",pl);
	  Delete(pl);
        }

        /* copy specific attributes for global (or in a namespace) template functions - these are not templated class methods */
        if (strcmp(cntype,"template") == 0) {
          Node *templatetype = Getattr(function,"templatetype");
          Node *symtypename = Getattr(function,"sym:typename");
          Parm *templateparms = Getattr(function,"templateparms");
          if (templatetype) {
	    Node *tmp = Copy(templatetype);
	    Setattr(new_function,"templatetype",tmp);
	    Delete(tmp);
	  }
          if (symtypename) {
	    Node *tmp = Copy(symtypename);
	    Setattr(new_function,"sym:typename",tmp);
	    Delete(tmp);
	  }
          if (templateparms) {
	    Parm *tmp = CopyParmList(templateparms);
	    Setattr(new_function,"templateparms",tmp);
	    Delete(tmp);
	  }
        } else if (strcmp(cntype,"constructor") == 0) {
          /* only copied for constructors as this is not a user defined feature - it is hard coded in the parser */
          if (GetFlag(function,"feature:new")) SetFlag(new_function,"feature:new");
        }

        add_symbols(new_function);
        /* mark added functions as ones with overloaded parameters and point to the parsed method */
        Setattr(new_function,"defaultargs", n);

        /* Point to the new function, extending the linked list */
        set_nextSibling(function, new_function);
	Delete(new_function);
        function = new_function;
	
	Delete(ntype);
      }
    } else {
      function = 0;
    }
  }
}

/* -----------------------------------------------------------------------------
 * tag_nodes()
 *
 * Used by the parser to mark subtypes with extra information.
 * ----------------------------------------------------------------------------- */

static void tag_nodes(Node *n, const_String_or_char_ptr attrname, DOH *value) {
  while (n) {
    Setattr(n, attrname, value);
    tag_nodes(firstChild(n), attrname, value);
    n = nextSibling(n);
  }
}

#line 1652 "parser.y"
typedef union {
  char  *id;
  List  *bases;
  struct Define {
    String *val;
    String *rawval;
    int     type;
    String *qualifier;
    String *bitfield;
    Parm   *throws;
    String *throwf;
  } dtype;
  struct {
    char *type;
    String *filename;
    int   line;
  } loc;
  struct {
    char      *id;
    SwigType  *type;
    String    *defarg;
    ParmList  *parms;
    short      have_parms;
    ParmList  *throws;
    String    *throwf;
  } decl;
  Parm         *tparms;
  struct {
    String     *method;
    Hash       *kwargs;
  } tmap;
  struct {
    String     *type;
    String     *us;
  } ptype;
  SwigType     *type;
  String       *str;
  Parm         *p;
  ParmList     *pl;
  int           intvalue;
  Node         *node;
} YYSTYPE;
#line 1700 "CParse/parser.c"
#define ID 257
#define HBLOCK 258
#define POUND 259
#define STRING 260
#define INCLUDE 261
#define IMPORT 262
#define INSERT 263
#define CHARCONST 264
#define NUM_INT 265
#define NUM_FLOAT 266
#define NUM_UNSIGNED 267
#define NUM_LONG 268
#define NUM_ULONG 269
#define NUM_LONGLONG 270
#define NUM_ULONGLONG 271
#define NUM_BOOL 272
#define TYPEDEF 273
#define TYPE_INT 274
#define TYPE_UNSIGNED 275
#define TYPE_SHORT 276
#define TYPE_LONG 277
#define TYPE_FLOAT 278
#define TYPE_DOUBLE 279
#define TYPE_CHAR 280
#define TYPE_WCHAR 281
#define TYPE_VOID 282
#define TYPE_SIGNED 283
#define TYPE_BOOL 284
#define TYPE_COMPLEX 285
#define TYPE_TYPEDEF 286
#define TYPE_RAW 287
#define TYPE_NON_ISO_INT8 288
#define TYPE_NON_ISO_INT16 289
#define TYPE_NON_ISO_INT32 290
#define TYPE_NON_ISO_INT64 291
#define LPAREN 292
#define RPAREN 293
#define COMMA 294
#define SEMI 295
#define EXTERN 296
#define INIT 297
#define LBRACE 298
#define RBRACE 299
#define PERIOD 300
#define CONST_QUAL 301
#define VOLATILE 302
#define REGISTER 303
#define STRUCT 304
#define UNION 305
#define EQUAL 306
#define SIZEOF 307
#define MODULE 308
#define LBRACKET 309
#define RBRACKET 310
#define BEGINFILE 311
#define ENDOFFILE 312
#define ILLEGAL 313
#define CONSTANT 314
#define NAME 315
#define RENAME 316
#define NAMEWARN 317
#define EXTEND 318
#define PRAGMA 319
#define FEATURE 320
#define VARARGS 321
#define ENUM 322
#define CLASS 323
#define TYPENAME 324
#define PRIVATE 325
#define PUBLIC 326
#define PROTECTED 327
#define COLON 328
#define STATIC 329
#define VIRTUAL 330
#define FRIEND 331
#define THROW 332
#define CATCH 333
#define EXPLICIT 334
#define USING 335
#define NAMESPACE 336
#define NATIVE 337
#define INLINE 338
#define TYPEMAP 339
#define EXCEPT 340
#define ECHO 341
#define APPLY 342
#define CLEAR 343
#define SWIGTEMPLATE 344
#define FRAGMENT 345
#define WARN 346
#define LESSTHAN 347
#define GREATERTHAN 348
#define DELETE_KW 349
#define LESSTHANOREQUALTO 350
#define GREATERTHANOREQUALTO 351
#define EQUALTO 352
#define NOTEQUALTO 353
#define QUESTIONMARK 354
#define TYPES 355
#define PARMS 356
#define NONID 357
#define DSTAR 358
#define DCNOT 359
#define TEMPLATE 360
#define OPERATOR 361
#define COPERATOR 362
#define PARSETYPE 363
#define PARSEPARM 364
#define PARSEPARMS 365
#define CAST 366
#define LOR 367
#define LAND 368
#define OR 369
#define XOR 370
#define AND 371
#define LSHIFT 372
#define RSHIFT 373
#define PLUS 374
#define MINUS 375
#define STAR 376
#define SLASH 377
#define MODULO 378
#define UMINUS 379
#define NOT 380
#define LNOT 381
#define DCOLON 382
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,    0,    0,    0,    0,    0,    1,    1,    2,
    2,    2,    2,    2,    2,    2,    3,    3,    3,    3,
    3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
    3,    3,    3,    3,    3,    3,    3,  126,    4,    5,
    6,    7,    7,    7,    8,    8,    9,    9,    9,    9,
  123,  122,  122,   10,   10,   10,  127,   11,   95,   95,
   12,   12,   13,   13,   13,   13,   14,   15,   15,   16,
   16,   17,   17,   94,   94,   93,   93,   18,   18,   18,
  119,  119,   19,   19,   19,   19,   19,   19,   19,   19,
  113,  113,  113,  124,  124,   20,   58,   58,   21,   21,
   21,  106,   67,   68,   68,   66,   66,   66,   22,   23,
   24,   25,   25,   25,  128,   25,   26,   27,   27,   27,
   52,   52,   52,   52,   29,   28,   28,   30,   33,   33,
   33,   33,   33,   33,  129,   34,  130,   34,   46,   46,
   35,  131,   36,   36,   44,   44,   44,   44,   44,   44,
  116,   59,   59,   69,   69,   60,   60,   47,   47,  132,
   48,  133,   48,   48,   37,  134,   37,   37,   37,  135,
   37,   38,   38,   38,   38,   38,   38,   38,   38,   38,
   38,   38,   38,   38,   38,   38,   38,   38,   38,   39,
   40,   40,   42,   42,   42,   42,   49,   41,   41,   41,
  137,   45,  138,   45,   43,   43,   43,   43,   43,   43,
   43,   43,   43,   43,   43,  117,  117,  118,  118,  118,
  136,   54,   54,   54,   54,   54,   54,   54,   54,   55,
   57,   57,   56,   56,   61,   61,   61,   64,   63,   63,
   65,   65,   62,   62,   81,   81,   81,   81,   81,  101,
  101,  101,  102,  102,  102,   98,   98,   98,   98,   98,
   98,   98,   98,  100,  100,  100,  100,  100,  100,  100,
  100,   99,   99,   99,   99,   99,   99,   99,   99,  103,
  103,  103,  103,  103,  103,  103,  103,  103,  103,  104,
  104,  104,  104,  104,  104,  104,   96,   96,   96,   96,
   89,   89,   90,   90,   90,   74,   75,   75,   75,   75,
   76,   76,   76,   76,   76,   76,   76,   76,   97,  121,
  121,  120,  120,  120,  120,  120,  120,  120,  120,  120,
  120,  120,  120,  120,  120,  139,   80,   87,   87,   31,
   31,   32,   32,   32,   82,   83,   83,   86,   86,   86,
   86,   86,   86,   86,   86,   86,   86,   86,   86,   84,
   84,   84,   84,   84,   84,   84,   84,   85,   85,   85,
   85,   85,   85,   85,   85,   85,   85,   85,   85,   85,
   85,   85,   85,   85,   85,   85,   85,   85,   85,   78,
  140,   79,   79,   77,   77,  142,   73,  143,   73,   72,
   72,   72,   70,   70,   71,   71,   71,  141,  141,   53,
   53,   53,   53,  105,  105,  105,  105,  105,  144,  144,
  145,  145,  146,   88,   88,   91,   91,   92,   92,  107,
  107,  107,  107,  107,  107,  108,  108,  108,  108,  111,
  109,  109,  109,  109,  109,  109,  110,  110,  110,  110,
  114,  114,  112,  112,  112,   51,   51,   50,   50,   50,
   50,   50,   50,  115,  115,  125,
};
short yylen[] = {                                         2,
    1,    3,    2,    3,    2,    5,    3,    2,    1,    1,
    1,    1,    1,    1,    1,    2,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    0,    7,    5,
    3,    5,    5,    3,    2,    2,    5,    2,    5,    2,
    4,    1,    1,    7,    7,    5,    0,    7,    1,    1,
    2,    2,    1,    5,    5,    5,    3,    4,    3,    7,
    8,    5,    3,    1,    1,    3,    1,    4,    7,    6,
    1,    1,    7,    9,    8,   10,    5,    7,    6,    8,
    1,    1,    5,    4,    5,    7,    1,    3,    6,    6,
    8,    1,    2,    3,    1,    2,    3,    6,    5,    9,
    2,    1,    1,    1,    0,    6,    5,    1,    4,    1,
    1,    2,    5,    6,    4,    7,    9,    6,    1,    1,
    1,    1,    1,    1,    0,    9,    0,    9,    1,    3,
    4,    0,    6,    3,    1,    1,    1,    1,    1,    1,
    1,    2,    1,    1,    1,    3,    1,    3,    4,    0,
    6,    0,    5,    5,    2,    0,    6,    1,    1,    0,
    3,    1,    1,    1,    1,    1,    1,    1,    1,    3,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    6,
    6,    7,    8,    8,    9,    7,    5,    2,    2,    2,
    0,    7,    0,    6,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    2,    2,    2,    4,    2,
    5,    1,    2,    1,    1,    1,    1,    1,    1,    1,
    2,    1,    3,    1,    2,    7,    3,    1,    2,    1,
    3,    1,    1,    1,    2,    5,    2,    2,    1,    2,
    2,    1,    1,    1,    1,    2,    3,    1,    2,    3,
    4,    5,    4,    1,    2,    3,    4,    5,    3,    4,
    4,    1,    2,    4,    4,    5,    3,    4,    4,    1,
    2,    2,    3,    1,    2,    1,    2,    3,    4,    3,
    4,    2,    3,    3,    4,    3,    3,    2,    2,    1,
    1,    2,    1,    1,    1,    1,    2,    1,    2,    3,
    1,    1,    1,    2,    2,    1,    1,    2,    1,    1,
    2,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    0,    2,    1,    1,    3,
    1,    1,    3,    1,    1,    1,    1,    1,    1,    5,
    1,    1,    3,    4,    5,    5,    6,    2,    2,    1,
    1,    1,    1,    1,    1,    1,    1,    3,    3,    3,
    3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
    3,    3,    3,    5,    2,    2,    2,    2,    2,    1,
    0,    3,    1,    1,    3,    0,    3,    0,    5,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    4,    5,    1,    3,    3,    4,    4,    3,    2,    1,
    1,    3,    2,    3,    1,    1,    1,    1,    1,    2,
    4,    1,    3,    1,    3,    3,    2,    2,    2,    2,
    2,    4,    1,    3,    1,    3,    3,    2,    2,    2,
    2,    1,    1,    1,    1,    3,    1,    3,    5,    1,
    3,    3,    5,    1,    1,    0,
};
short yydefred[] = {                                      0,
    0,    0,    0,    0,    0,    9,    3,    0,  322,  330,
  323,  324,  327,  328,  325,  326,  313,  329,  312,  331,
    0,  316,  332,  333,  334,  335,    0,  303,  304,  305,
  406,  407,    0,  403,  404,    0,    0,  434,    0,  405,
    0,    0,    0,    0,    0,  311,  317,    0,    0,  319,
    5,    0,    0,    0,    0,   63,   59,   60,    0,  225,
   13,    0,    0,    0,    0,   81,   82,    0,    0,    0,
    0,  224,  226,  227,    0,  228,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    8,   10,   17,   18,   19,   20,   21,   22,   23,   24,
   25,   26,   27,   28,   29,   30,   31,   32,   33,   34,
   35,   36,   37,   11,  112,  113,  114,   15,   12,  129,
  130,  131,  132,  133,  134,    0,    0,    0,  229,    0,
  440,  425,  314,    0,  315,    0,    0,    2,  318,    0,
    0,    0,    0,    0,    0,    0,  252,    0,    0,    0,
  235,    0,    0,    0,  249,  309,    0,  302,    0,    0,
  430,  321,    4,    7,    0,  230,    0,  232,   16,    0,
  452,    0,    0,    0,  457,    0,    0,    0,  306,    0,
    0,    0,    0,   77,    0,    0,    0,    0,    0,    0,
  162,    0,    0,   61,   62,    0,    0,   50,   48,   45,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  352,  360,  361,  362,  363,  364,  365,  366,  367,
    0,    0,    0,    0,    0,    0,    0,    0,  243,    0,
  238,    0,    0,    0,    0,  348,  351,    0,    0,  240,
  237,  435,    0,    0,    0,    0,    0,    0,    0,    0,
  247,    0,    0,  292,    0,  346,    0,    0,    0,    0,
    0,  264,    0,  298,  273,    0,    0,    0,    0,  250,
    0,    0,  251,    0,    0,    0,  310,  439,  438,    0,
    0,    0,  231,  234,  426,    0,    0,  451,  115,    0,
    0,   67,   44,  336,    0,    0,   69,    0,    0,    0,
    0,    0,    0,   97,    0,    0,    0,  158,    0,  466,
  160,    0,  102,    0,    0,    0,    0,  253,  106,  254,
  255,    0,  103,  105,   41,  428,    0,  429,    0,    0,
   53,    0,  151,  155,    0,    0,    0,  153,  144,    0,
    0,  339,  137,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  386,  385,  359,  387,  388,    0,  239,  242,  424,  389,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  431,    0,    0,
    0,    0,    0,  272,  296,    0,    0,    0,  294,    0,
    0,    0,  293,    0,    0,  265,    0,    0,  297,    0,
    0,    0,    0,  277,    0,    0,  290,    0,    0,    0,
  436,    6,    0,    0,  466,  456,    0,    0,    0,    0,
   68,   38,   76,    0,    0,    0,    0,    0,    0,    0,
  159,    0,    0,  466,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  152,  157,  142,  125,
    0,    0,  141,  391,    0,  390,  393,    0,    0,    0,
    0,  121,    0,   57,    0,    0,    0,    0,    0,   78,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  370,  371,  372,    0,    0,  287,  275,  274,    0,    0,
    0,    0,    0,  266,    0,    0,  269,    0,    0,    0,
    0,  279,  278,  295,  291,    0,  233,   65,   66,    0,
    0,  461,  465,    0,    0,    0,   42,   43,    0,   75,
   72,    0,  455,   92,  454,    0,    0,   91,   87,    0,
    0,    0,    0,    0,   98,    0,  197,  164,  163,    0,
    0,    0,    0,   49,   47,    0,   40,  104,    0,    0,
  445,    0,    0,   56,    0,  109,    0,    0,    0,    0,
  341,  344,  170,  189,    0,    0,    0,    0,    0,    0,
  213,  214,  206,  215,  187,  168,  211,  207,  205,  208,
  209,  210,  212,  188,  184,  185,  172,  178,  182,    0,
    0,  173,  174,  175,  177,  176,  179,  181,  183,    0,
    0,  186,    0,  135,    0,    0,    0,  118,  120,  117,
    0,  122,  466,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  241,    0,    0,  276,  246,  267,    0,
  271,  270,    0,  116,    0,    0,    0,    0,    0,    0,
    0,  413,    0,    0,    0,    0,    0,   89,    0,  161,
    0,    0,    0,  100,    0,   99,    0,    0,    0,  441,
    0,    0,   51,    0,  156,  145,  146,  149,  148,  147,
  150,  143,    0,    0,    0,    0,    0,  166,  199,  198,
  200,    0,    0,    0,  165,    0,    0,    0,    0,  408,
  394,    0,  409,    0,    0,    0,  336,    0,  128,    0,
    0,    0,    0,    0,   80,    0,    0,    0,  350,    0,
  236,  268,  459,  463,   39,    0,    0,   83,    0,    0,
    0,   88,    0,    0,    0,   96,   70,    0,    0,  108,
  450,    0,  449,    0,  446,    0,   54,   55,    0,  343,
    0,  340,  126,    0,  171,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  180,    0,  401,  400,  402,  398,
    0,    0,    0,    0,    0,  420,    0,    0,    0,    0,
   58,   79,    0,    0,    0,    0,   95,    0,   90,    0,
   85,   71,  101,  447,  442,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  203,    0,    0,    0,  395,    0,
  397,    0,    0,  418,    0,    0,  421,  414,  415,  123,
  119,    0,   93,  411,    0,   84,    0,  110,  127,    0,
    0,    0,  138,    0,    0,    0,    0,    0,  201,    0,
  221,    0,  139,  136,    0,  416,  417,  423,    0,  124,
  412,   86,  167,    0,    0,  191,    0,    0,    0,    0,
  204,    0,  190,  399,    0,  422,    0,  192,  216,  217,
  196,    0,    0,    0,  202,  140,  218,  220,  336,  194,
  193,    0,    0,  195,  219,
};
short yydgoto[] = {                                       4,
    5,   91,   92,   93,  601,  602,  603,  604,   98,  605,
  606,  101,  607,  103,  608,  105,  609,  610,  611,  612,
  613,  614,  615,  616,  617,  115,  640,  116,  117,  118,
  590,  591,  119,  120,  618,  619,  620,  621,  622,  623,
  624,  625,  626,  702,  627,  864,  628,  124,  629,  300,
  174,  481,  887,  630,  257,  293,  166,  315,  343,  467,
  167,  240,  241,  242,  377,  205,  206,  333,  345,   40,
   41,  790,  721,  243,  179,   43,  722,  475,  476,  262,
  482,  770,  245,  246,  247,  266,  351,  131,   44,   45,
  301,  337,  183,  551,  127,  219,   46,  149,  150,  270,
  151,  329,  152,  153,  729,  324,   47,  161,  582,  690,
   48,  558,  559,  249,  545,  347,  876,  888,  128,   49,
   50,  340,  341,  447,  168,  549,  643,  435,  725,  472,
  588,  454,  320,  776,  707,  632,  882,  858,  263,  633,
  724,  791,  830,  797,  836,  837,
};
short yysindex[] = {                                    657,
 4332, 4394,  -66,    0, 5273,    0,    0, -299,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 -299,    0,    0,    0,    0,    0, -221,    0,    0,    0,
    0,    0,   -1,    0,    0, -281, -241,    0, -170,    0,
   -1,  943,  745, 5516,  745,    0,    0,   -9, 6703,    0,
    0, -140,  -86, 5093,  -95,    0,    0,    0,  -54,    0,
    0,    8,   12, 4547,   19,    0,    0,   12,   20,   75,
  110,    0,    0,    0,  128,    0, -185, -138,  150,  -48,
  167,  481,   59, 5184, 5184,  179,  195,    8,  203,  207,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0, 5275,   12,  534,    0, 2825,
    0,    0,    0,   33,    0, -228,  178,    0,    0, 4156,
   47, 2947, 3435, -198,  114,   -1,    0, 1064, -230,   73,
    0, -230,  136,   -6,    0,    0,  745,    0,  257, -220,
    0,    0,    0,    0,  230,    0,  262,    0,    0,  -40,
    0,  -33,  -40,  -40,    0,  266, -183,  560,    0,  164,
   -1,  309,  322,    0,  -40, 5002, 5093,   -1,  305, -118,
    0,  310,  360,    0,    0,  -40,  369,    0,    0,    0,
  368, 5093,  339,  -70,  349,  366,  -40,    8,  368, 5093,
 5093,   -1,   31,  -26,  787,    8,   18,   52,  945,  -40,
  316,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 3435,  388, 3435, 3435, 3435, 3435, 3435, 3435,    0,  391,
    0,  351,  390,  943, 6590,    0,    0,    0,  368,    0,
    0,    0,   -9,  359, 4219, -117,  394, 1172,  419,  358,
    0,  412, 3435,    0, 3656,    0, 6590, 4219,   -1,  139,
  136,    0,  352,    0,    0, -198,  139,  136,  380,    0,
 5093, 3069,    0, 5093, 3191, 1020,    0,    0,    0,   -9,
  447, 5093,    0,    0,    0,  467,  368,    0,    0,  468,
  -78,    0,    0,    0, -172, -230,    0,  492,  494,  516,
  519,  184,  536,    0,  535,  543,  544,    0,   -1,    0,
    0,  545,    0,  548,  552,  554, 5184,    0,    0,    0,
    0, 5184,    0,    0,    0,    0,  555,    0,  -25,  241,
    0,  566,    0,    0,  568,    0,  524,    0,    0, -213,
  581,    0,    0, -121, 4281,  660, -225, -299,  146,  589,
  146,  531, -161,   52,  532,  603, 1020, 3031, 5363, 1874,
    0,    0,    0,    0,    0, 2825,    0,    0,    0,    0,
 3435, 3435, 3435, 3435, 3435, 3435, 3435, 3435, 3435, 3435,
 3435, 3435, 3435, 3435, 3435, 3435, 3435,    0,  178,  143,
  -32,  541,  363,    0,    0,  143,  422,  550,    0,  146,
 3435, 6590,    0, 1177, -255,    0, 5093, 3313,    0,  139,
  136, 3006,  608,    0, 3745,  626,    0, 3834,   52,  139,
    0,    0,  262,   82,    0,    0,  -40, 1493,  625,  628,
    0,    0,    0,  118,  148, 3167,  631, 5093,  560,  629,
    0,  635, 4909,    0,  428, 5184,  295,  634,  636,  349,
   36, 5093,  638,  -40, -208, 5093,    0,    0,    0,    0,
  690, 1250,    0,    0,  652,    0,    0,  661,  531,  684,
  251,    0,   58,    0,   -8,  146,   52, -236, 3007,    0,
 3435, 3374, 2886, -222,  943,  391, 1259, 1259, 2093, 2093,
 5306, 4388, 4439, 4489, 2619, 1874,  659,  659,  674,  674,
    0,    0,    0,   -1,  550,    0,    0,    0,  143,  578,
 3923,  620,  550,    0,   52,  694,    0, 6541,   52,  139,
  136,    0,    0,    0,    0,  139,    0,    0,    0,  368,
 5000,    0,    0,  -25,  683,  687,    0,    0, 1250,    0,
    0,  368,    0,    0,    0,  702,  387,    0,    0,  368,
  689,  368,  279,  148,    0,  387,    0,    0,    0, 5091,
    8, 5414,  562,    0,    0, 5093,    0,    0,   32,  616,
    0,  655,  705,    0,  712,    0,  568, 1137,  701,  145,
    0,    0,    0,    0,  711,  682,  685,  695,  649,  257,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  716,
 1250,    0,    0,    0,    0,    0,    0,    0,    0, 4598,
    0,    0,  730,    0,  640, 5093,  560,    0,    0,    0,
  726,    0,    0,  387, -193,   52, 6590, 3435, 2886, 3252,
 3435,  748,  765,    0, 3435, -230,    0,    0,    0,  646,
    0,    0,  139,    0,  -40,  -40,  763, 5093,  774, -208,
  744,    0, 1493,  808,  -40,  784,  387,    0,  785,    0,
  368, -149,  560,    0, 5184,    0,  788,  826, -196,    0,
 -184, 2825,    0,   45,    0,    0,    0,    0,    0,    0,
    0,    0, 4649, 3435,  690,  879, 1250,    0,    0,    0,
    0,  257,  793,  560,    0, 5363,  -62, -125,  792,    0,
    0,  794,    0,  728, 1250, 5093,    0,  766,    0,  798,
  660, 5093, 5182,  800,    0, 6590, 6590, 3435,    0, 3044,
    0,    0,    0,    0,    0,  804, 5093,    0,  801,  368,
  810,    0,  387,  915, -208,    0,    0,  803,  807,    0,
    0,   32,    0,   32,    0,  762,    0,    0, 2762,    0,
 6590,    0,    0,  660,    0, 1250,  820, 5093,  660,  -35,
  815, -121, 4281, 3435,    0,  730,    0,    0,    0,    0,
   -1,  817,  829,  833,   -1,    0,  399, -230,  251,  839,
    0,    0, 6590,  834,  840, 5093,    0,  844,    0,  387,
    0,    0,    0,    0,    0,  845,  251,  835, 5093,  842,
  251, 5093,  853, -256,    0,  851,  858, 3474,    0,  730,
    0, 1119,  425,    0,  860,  861,    0,    0,    0,    0,
    0, -230,    0,    0,  863,    0,  864,    0,    0, 1250,
  868,  387,    0,  869, 5093, 5093,  872, 1119,    0,  640,
    0,   -1,    0,    0,  660,    0,    0,    0,   -1,    0,
    0,    0,    0,  387,  432,    0,  387,  875,  880, 5093,
    0, 1119,    0,    0,  251,    0,  334,    0,    0,    0,
    0,  387,  387,  882,    0,    0,    0,    0,    0,    0,
    0,  387,  881,    0,    0,
};
short yyrindex[] = {                                   2726,
    0,    0,    0,    0, 1056,    0,    0, 1366,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 3693,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  471, 3871,    0, 3604,    0,    0, 1494, 3782,    0,
    0,    0,    0,  884, 2620,    0,    0,    0,    0,    0,
    0, 4700,  493,    0,    0,    0,    0,   43,  924,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  922,    0,    0,  843,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  884,
 3496,    0,    0,  658,  832,    0,    0, 5519,  471, 4066,
    0,  471, 5570, 2982,    0,    0, 3960,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  884,    0,    0,    0,
    0, 4751,    0,    0,    0,    0,  725,    0,    0,    0,
    0,    0,    0,    0,    0,  884,  884,    0,    0,  -97,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 1873,  884,    0,  848,  438,    0,  884,    0, 1982,  884,
  843,    0,  885,    0,    0,    0,    0,    0,    0,    0,
 5768,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  843,
    0,    0, 5971, 6040,    0,    0,    0, 3571, 6002,    0,
    0,    0, 1619,    0,  884,  897,    0,  899,    0, 1061,
    0,  508,    0,    0,    0,    0, 1161,  884,    0, 5551,
 5843,    0, 4049,    0,    0, 5852, 5602, 5903, 4893,    0,
  884,    0,    0,  884,    0, 5912,    0,    0,    0, 1745,
    0,    0,    0,    0,    0,    0, 2529,    0,    0,    0,
  901,    0,    0,    0,    0,  900,    0,    0,    0,    0,
 2092,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  357,    0,
    0,    0,    0,    0,  843, -203,    0,    0,    0,  109,
    0,    0,    0,  512,  884,  582,    0,   68,    0,    0,
    0,    0,    0,    0, 5825,    0,    0,    0,    0, 6411,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  897,
  899, 1061,    0,    0,    0,  903,    0,  656,    0,  904,
    0, 1110,    0,  899, 1144,    0,  884,    0,    0, 5634,
 5926, 5935,    0,    0,    0,    0,    0,    0,    0, 5653,
    0,    0,  884,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0, 5465,    0, 5465,    0,    0, 1303,    0,  438,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  187, 4445,    0,    0,    0,    0,    0,    0, 1061,    0,
    0,    0,  582,    0,    0,    0,    0,    0,    0,    0,
 6088,    0, -209,    0,  884,  843, 6285, 6316, 6348, 6379,
    0, 2036, 6536, 6505, 6474, 6442, 6218, 6249, 6134, 6182,
    0,    0,    0,    0,    0,    0,    0,    0,  905,    0,
    0,    0,  692,    0,  904,    0,    0,    0,    0, 5685,
 5978,    0,    0,    0,    0, 5717,    0,    0,    0, 2201,
 5465,    0,    0,  376,  554,  906,    0,    0, 4445,    0,
    0, 2310,    0,    0,    0,    0,  -77,    0,    0, 2420,
    0,  353,    0,    0,    0,  900,    0,    0,    0, 5465,
 4700,    0,    0,    0,    0,  884,    0,    0,  857,    0,
    0,    0,    0,    0,    0,    0,  843, 4802,  199,    0,
    0,    0,    0,    0,    0,    0,    0,    0, 4853,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 4445,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 4496,    0,  384,    0,  135,  884,    0,    0,    0,    0,
    0,    0,    0,  900,    0,    0, 4080,    0, -209, -124,
    0,    0,    0,    0,    0,  471,    0,    0,    0,    0,
    0,    0, 5736,    0,    0,    0,    0,  884,    0,    0,
  407,    0,    0,    0,    0,    0,  -77,    0,    0,    0,
 4751,  725,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  843,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  187,    0, 4445,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  885,    0,  688,    0,
    0,  908,    0,   61, 4445,  884,    0,  483,    0,    0,
  582,  884, 5465,    0,    0, 4885, 6572,    0,    0, 2364,
    0,    0,    0,    0,    0,    0,  884,    0,    0,  376,
  916,    0,  900,    0,    0,    0,    0,    0,    0,    0,
    0,  866,    0,  867,    0,    0,    0,    0,    0,    0,
  233,    0,    0,  582,    0, 4445,    0,  884,  582,    0,
    0,  512,  884,    0,    0,  384,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  582,    0,    0,
    0,    0, 6628,    0,    0,  884,    0,    0,    0,  900,
    0,    0,    0,    0,    0,    0,    0,    0,  884,    0,
    0,  884,    0,    0,    0,    0,    0,    0,    0,   43,
    0,    0,    0,    0,    0,  523,    0,    0,    0,    0,
    0,  582,    0,    0,    0,    0,    0,    0,    0, 4445,
    0,  483,    0,    0,  884,  884,    0,    0,    0,  135,
    0,    0,    0,    0,  582,    0,    0,    0,    0,    0,
    0,    0,    0,  378,    0,    0,  378,    0,    0,  884,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  378,  378,    0,    0,    0,    0,    0,    0,    0,
    0,  378,    0,    0,    0,
};
short yygindex[] = {                                      0,
 -317,    0,    0,    0,   35,   39,   41,   53,    0,   55,
   57,    0,   60,    0,   63,    0,   72,   83,   90,   94,
  107,  111,  121,  122,  125,  627, -370,    0,    0,    0,
    0,  503,    0,  650,    7,   22, -528,    0,  651,    0,
    0,  665,    0,    0,    0, -761,  130,    0,  134, -166,
  -19, -699, -483,   14,  331,  783,    0,    0,    0,  647,
  346,  870,    0,  556,  760,  -79, -309,  802,  778, -207,
  -45,    0,  472,  212,  103,  -30,    0, -639,    0, -302,
  -42,    0,  836, -391,    0, -122,    0, 1242,  -28,    0,
 -154,    0,    0,    0,    0,  -31,    0, -115, -217, -106,
  769,    0,  -84, -120,  405,    0,  756,   30,    0, -366,
 -126,  693, -440,  823, -439,    0,    0, -340,    0,    0,
 1224,    0,  837, -540,    1,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  444,    0,    0,    0,    0,  409,
};
#define YYTABLESIZE 6994
short yytable[] = {                                     147,
    6,  439,  453,  346,  203,  129,  563,  248,  132,  253,
  148,  121,  220,  157,  156,  296,  158,  459,  126,  302,
  667,  132,  676,  271,  586,  308,  122,  278,    8,  323,
  312,  799,  288,  290,  298,  856,    8,  524,  403,   94,
  407,  277,  155,   95,  212,   96,  543,  130,  181,  553,
  360,  171,  336,  405,  543,  259,  524,   97,    8,   99,
  762,  100,  306,  175,  102,  366,  298,  104,  175,  184,
  651,    8,  764,  670,  817,  141,  106,  781,  134,  821,
  214,  470,  679,  300,    8,  484,  554,  107,  328,  555,
  154,  254,  715,  268,  108,    8,  881,  143,  109,  356,
  136,  735,  525,   42,   42,  137,  280,  216,  258,  283,
  142,  110,  277,  274,  857,  111,  273,  541,  190,  330,
  895,  646,  304,  678,  138,  112,  113,  175,  287,  114,
  250,    8,  252,  130,  123,  271,  570,  278,  125,    8,
  289,  403,  826,  407,  154,  757,  573,  556,  652,  155,
  188,  728,  155,  405,  163,  421,   42,  405,   36,  191,
  734,  300,   38,  130,  763,  885,  783,  294,  299,  420,
  259,   36,  148,  473,  140,   38,  765,  132,  775,  430,
  466,  269,  466,  259,   36,  361,    8,  319,   38,   53,
  132,  142,  520,  755,    8,   36,  792,  130,  359,   38,
  466,  147,  784,  145,  331,  334,  474,  338,  164,  194,
  807,  348,  148,  352,  145,  437,  295,  466,   36,  171,
  466,  140,   38,  401,    8,   54,  298,  438,  130,  748,
    8,   36,  244,  751,  298,   38,  414,  170,  142,   36,
  378,  419,   42,   38,  155,  218,  299,  818,    8,  195,
  145,  171,  460,  248,  146,    8,  822,  420,  346,  255,
  430,  466,  146,  440,  299,  474,  169,  171,  520,  808,
  542,  353,  462,  361,  358,  178,  142,  171,  466,  271,
  278,  543,  398,  305,  466,  421,   36,  350,   42,   42,
   38,  561,  579,  278,   36,  204,  204,  585,   38,  466,
  144,  531,  767,  173,   42,  145,  155,  522,    8,  146,
  180,  182,   42,   42,  811,  530,  200,  396,  171,  431,
    6,  873,  536,  361,   36,  733,  847,  483,   38,  557,
   36,  487,  251,  566,   38,  823,  494,  215,  406,  538,
  145,  171,  768,  363,  261,  468,   39,   52,   36,  159,
  132,  286,   38,  514,  477,   36,  155,   42,  132,   38,
  426,  426,  218,  141,  281,  466,  185,  145,  875,  644,
   42,  146,  160,  426,   36,  550,  728,  171,   38,  539,
  522,  282,  530,   42,  165,  143,   42,   36,  359,  641,
  688,   38,  580,  145,   42,  814,  581,  815,  531,  466,
  466,  186,    8,  466,    8,  553,  338,  171,   36,  466,
  466,  466,   38,  689,   28,   29,   30,  396,  660,  187,
  295,  396,  663,  171,  794,  466,  466,  284,  841,  466,
  417,  269,  466,  294,  255,    6,  466,  305,  705,  305,
  642,  193,  554,  706,  285,  555,  849,  418,  677,  466,
  853,  142,  147,  129,    6,  129,  307,   42,  196,  121,
  334,  274,  466,  148,  650,  466,  126,  466,  572,  466,
  207,  592,  631,  713,  122,  239,  445,  446,  244,  466,
  466,   31,   32,  155,  466,  466,  208,   94,  466,  145,
  466,   95,  342,   96,  210,  155,  378,  342,  743,  744,
   34,   35,   36,  556,   36,   97,   38,   99,   38,  100,
   31,   32,  102,    8,  896,  104,  314,  316,  218,   42,
  561,  731,  291,  145,  106,  146,  345,  146,  671,   34,
   35,  345,  326,  463,  464,  107,  891,  671,  204,  660,
  342,  129,  108,  204,  637,  638,  109,  121,  639,  631,
   42,  900,  901,  211,  126,  292,  344,  672,  753,  110,
  303,  904,  122,  111,   42,  310,  672,  758,   42,  248,
  129,  674,  675,  112,  113,   94,  121,  114,  311,   95,
  495,   96,  123,  126,  717,  777,  125,  468,  129,  574,
  774,  122,  575,   97,  698,   99,  903,  100,  779,  318,
  102,  703,  356,  104,   94,  759,  671,  321,   95,  699,
   96,  423,  106,  741,  426,  671,  322,  274,  419,  157,
  650,  631,   97,  107,   99,  325,  100,  298,  897,  102,
  108,  898,  104,  723,  109,  672,  327,  433,  810,  899,
  466,  106,  332,    6,  672,  464,  464,  110,  671,   52,
   52,  111,  107,  356,  281,  517,  155,  214,  427,  108,
  335,  112,  113,  109,  410,  114,  410,  204,  464,  464,
  123,  282,  466,  367,  125,  466,  110,  672,   42,  369,
  111,  380,  132,  466,  376,  478,  405,   28,   29,   30,
  112,  113,  250,  838,  114,  466,  839,  466,  379,  123,
   60,  410,  483,  125,  410,  592,  399,  631,  466,  466,
  466,  409,  410,  281,  518,  410,  865,  477,  669,  866,
  411,  239,  867,  571,  671,  631,  889,  145,  796,  890,
  282,  155,  466,  129,  410,  466,  466,  422,   42,  121,
  466,  432,  865,  466,  466,  483,  126,  526,  824,  466,
  483,  361,  466,  672,  122,  840,   72,   73,   74,  434,
  436,   76,  410,  466,  466,  466,  865,   94,  318,  466,
   42,   95,  197,   96,  155,  198,  631,  466,  199,  155,
  466,  671,  477,  683,  441,   97,  723,   99,  135,  100,
    8,  442,  102,  565,  244,  104,  139,  154,  155,  870,
  245,  245,  245,  318,  106,  245,  245,  583,  443,  466,
  672,  344,  318,  318,  318,  107,    8,  419,  466,  553,
  419,  171,  108,  671,  444,  217,  109,  449,   42,  448,
  723,  671,  189,  192,   42,  450,  483,  455,  451,  110,
  456,  718,  155,  111,  457,  671,  458,  461,  671,   42,
  631,  305,  672,  112,  113,  245,  684,  114,  465,  555,
  672,  466,  123,  671,  671,  155,  125,  685,  318,  281,
  657,  469,  318,  671,  672,  466,  466,  672,  471,  466,
   42,  485,  318,  221,  172,   42,  282,  318,  486,  489,
   36,  318,  672,  672,   38,  260,  204,  490,  516,  272,
  532,  275,  672,  279,  218,  201,  687,  519,   42,  145,
  209,  417,  659,  146,  769,  286,   36,  286,  534,  547,
   38,   42,  548,  564,   42,  576,  567,  780,  418,  568,
  218,  726,  584,  221,  577,  145,  309,  417,  742,  146,
   28,   29,   30,  317,  317,  727,  589,  272,  272,  634,
  286,  286,  286,  635,  418,  286,  286,   42,   42,  154,
   28,   29,   30,  286,  272,  141,  730,  349,  135,  354,
  221,  669,  362,  272,  365,  636,  665,  265,  267,  317,
  666,  466,   42,  264,  264,  286,  661,  143,  317,  317,
  317,  480,  297,  668,  673,  297,  297,  691,  746,  154,
  264,  692,  297,  693,  694,  286,  704,  297,  708,  709,
  402,  404,  710,  408,  714,  317,  466,  732,  297,    1,
    2,    3,  711,  415,  416,  466,  466,  466,  712,  297,
  339,  272,  393,  394,  395,  396,  397,  239,  357,  297,
  738,  272,  297,    8,  317,   28,   29,   30,  317,  395,
  396,  397,  787,  788,  789,    1,  793,  739,  317,  720,
  362,  745,  800,  317,    8,  747,  368,  317,  370,  371,
  372,  373,  374,  375,  452,  749,  754,  805,  355,  756,
  760,  466,  761,  466,  778,  466,  785,  786,  300,  300,
  798,  300,  806,  795,  802,  466,  804,  812,  412,  305,
  466,  813,  752,  675,  466,  466,  466,  466,  820,  816,
  479,  819,  825,  827,  404,  832,  404,  425,  488,  272,
  428,  833,  272,  300,  300,  300,  300,  834,  843,  300,
  300,  842,  844,  850,  852,    8,  845,  300,  846,  848,
  300,  466,  466,   36,  855,  466,  466,   38,  859,  851,
  860,  868,  854,  466,  869,  871,  515,  218,  872,  300,
  874,  877,  145,  880,   36,  404,  146,  892,   38,  523,
  305,    8,  893,  773,  902,  905,  466,  272,  218,  300,
  466,  466,  466,  145,  272,  878,  879,  146,  300,  286,
  466,  280,  300,  460,  466,  282,  287,  288,  462,    8,
  221,    8,  300,  443,  221,  392,  305,  772,   94,  809,
  894,  300,  448,  444,  696,  537,  497,  498,  499,  500,
  501,  502,  503,  504,  505,  506,  507,  508,  509,  510,
  511,  512,  513,  695,  140,   36,  363,  697,  700,   38,
  221,  404,  272,  587,  272,  496,  521,  766,  141,  218,
  154,  142,  701,  528,  145,  654,  540,  829,  146,  297,
  544,  578,  133,  653,  883,  686,  552,  560,  562,  656,
  143,   36,  162,  862,  546,   38,    8,  886,    0,    0,
  272,    0,    0,    0,  272,  218,  297,  560,    0,    0,
  145,    0,    0,    0,  146,    0,    0,    0,    0,   36,
    0,   36,    0,   38,    0,   38,    0,  645,    0,    0,
    0,  363,  466,  144,    0,  364,    0,  317,  145,  221,
    8,    0,  146,    0,  269,    0,  647,  506,  511,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,  466,  466,  466,  466,  466,    0,    0,    0,
    0,    0,  317,  317,  317,  268,  466,  466,  466,  466,
  466,  317,  317,  317,    0,  466,  317,    0,    0,  317,
    0,    0,  142,    0,    0,    8,   36,  466,  466,  466,
   38,    0,    0,    0,    0,  719,  560,    0,  317,    0,
  429,    0,  221,  681,    0,  560,    0,    0,    0,  269,
  317,  272,  337,  337,  337,    0,    0,  337,  337,   60,
  305,    0,  466,  863,    0,    0,  466,  317,  337,    0,
   36,  317,    0,    0,   38,    0,    0,    0,    8,  221,
    0,  317,  571,    8,  276,  317,  317,  317,  221,    0,
  317,    0,    0,  269,  317,  317,  317,    0,    0,  317,
    0,    0,  317,  248,  248,  248,    0,  337,  248,  248,
    0,  221,    0,  140,    0,   72,   73,   74,  268,  221,
   76,  317,  782,  221,    0,   36,    0,    0,    0,   38,
  142,    0,    0,  736,  373,  142,  737,  297,  297,  218,
  740,    0,  560,  432,  145,  750,   90,  297,  146,    0,
  317,    0,    0,    0,  317,  593,    0,   56,  248,  221,
   57,   58,   59,    0,  317,    0,    0,    0,    0,  317,
    0,    0,   60,  317,  221,    0,    0,    0,   36,    0,
    0,    0,   38,   36,    0,    0,    0,   38,  479,  771,
    0,    0,  406,    0,  594,   62,  831,  406,    0,    0,
  835,  146,    0,    0,    0,    0,  269,    0,    0,    0,
  107,    0,  107,   64,   65,   66,   67,  595,   69,   70,
   71,    0,    0,  803,  596,  597,  598,  560,   72,  599,
   74,    0,   75,   76,   77,    0,    0,  221,   81,    0,
   83,   84,   85,   86,   87,   88,  107,  107,    0,    0,
  107,  107,    0,    0,   89,    0,    0,    0,  107,   90,
    0,    0,    0,  221,    0,    0,    0,  884,  433,  828,
    0,  466,  466,  466,  835,  466,  466,  466,  466,  600,
  391,  392,  393,  394,  395,  396,  397,  221,  466,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,    0,  466,  466,    0,  466,  466,  466,  466,
  466,  466,    0,  466,  466,  466,    0,  466,    0,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,    0,  466,    0,  466,  466,  466,  466,  466,
  466,  466,  466,  466,  466,  466,  466,  466,    0,    0,
    0,    0,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,  466,  466,  437,  466,    0,  466,    0,  432,
  432,  432,  171,  432,  432,  432,  432,  223,  224,  225,
  226,  227,  228,  229,  230,    0,  432,  432,  432,  432,
  432,  432,  432,  432,  432,  432,  432,  432,  432,  432,
  432,  432,  432,  432,  432,  432,  432,  432,  432,  432,
    0,  432,  432,    0,  432,  432,  432,  432,  432,  432,
    0,  432,  432,  432,    0,  432,    0,  432,  432,  432,
  432,  432,  432,  432,  432,  432,  432,  432,  432,  432,
  432,  432,  432,  432,  432,  432,  432,  432,  432,  432,
  432,  432,  432,  432,  432,  432,  432,  432,  432,  432,
    0,  432,    0,  432,  432,  432,  432,  432,  432,  432,
  432,  432,    0,  432,  432,  432,    0,    0,    0,    0,
  432,  432,  432,  432,  432,  432,  432,  432,  432,  432,
  432,  432,   46,  432,  433,  433,  433,    0,  433,  433,
  433,  433,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  433,  433,  433,  433,  433,  433,  433,  433,  433,
  433,  433,  433,  433,  433,  433,  433,  433,  433,  433,
  433,  433,  433,  433,  433,    0,  433,  433,    0,  433,
  433,  433,  433,  433,  433,    0,  433,  433,  433,    0,
  433,    0,  433,  433,  433,  433,  433,  433,  433,  433,
  433,  433,  433,  433,  433,  433,  433,  433,  433,  433,
  433,  433,  433,  433,  433,  433,  433,  433,  433,  433,
  433,  433,  433,  433,  433,    0,  433,    0,  433,  433,
  433,  433,  433,  433,  433,  433,  433,    0,  433,  433,
  433,  111,    0,    0,    0,  433,  433,  433,  433,  433,
  433,  433,  433,  433,  433,  433,  433,    0,  433,    0,
  437,  437,  437,    0,  437,  437,  437,  437,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  437,  437,  437,
  437,  437,  437,  437,  437,  437,  437,  437,  437,  437,
  437,  437,  437,  437,  437,  437,  437,  437,  437,  437,
  437,    0,  437,  437,    0,  437,  437,  437,  437,  437,
  437,    0,  437,  437,  437,    0,  437,    0,  437,  437,
  437,  437,  437,  437,  437,  437,  437,  437,  437,  437,
  437,  437,  437,  437,  437,  437,  437,  437,  437,  437,
  437,  437,  437,  437,  437,  437,  437,  437,  437,  437,
  437,   73,  437,    0,  437,  437,  437,  437,  437,  437,
  437,  437,  437,    0,  437,  437,  437,    0,    0,    0,
    0,  437,  437,  437,  437,  437,  437,  437,  437,  437,
  437,  437,  437,    0,  437,    0,    0,    0,   46,   46,
   46,    0,    0,   46,   46,   46,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   46,   46,   46,   46,   46,
   46,   46,   46,   46,   46,   46,   46,   46,   46,   46,
   46,   46,   46,   46,    0,    0,    0,   46,   46,    0,
    0,   46,    0,   46,   46,   46,   46,   46,    0,    0,
   46,    0,    0,    0,   46,    0,   46,   46,   46,   46,
   46,   46,   46,   46,   46,   46,   46,   46,   46,   46,
   64,   46,   46,   46,    0,   46,   46,   46,   46,   46,
   46,   46,   46,   46,   46,   46,   46,   46,   46,    0,
    0,    0,    0,  381,  382,  383,  384,   46,    0,   46,
    0,    0,   46,   46,   46,    0,    0,  111,  111,  111,
    0,    0,  111,  111,  111,  391,  392,  393,  394,  395,
  396,  397,   46,    0,  111,  111,  111,  111,  111,  111,
  111,  111,  111,  111,  111,  111,  111,  111,  111,  111,
  111,  111,  111,    0,    0,    0,  111,  111,    0,    0,
  111,    0,  111,  111,  111,  111,  111,    0,    0,  111,
    0,    0,    0,  111,    0,  111,  111,  111,  111,  111,
  111,  111,  111,  111,  111,  111,  111,  111,  111,   74,
  111,  111,  111,    0,  111,  111,  111,  111,  111,  111,
  111,  111,  111,  111,  111,  111,  111,  111,  379,  379,
  379,    0,    0,  379,  379,    0,  111,    0,  111,    0,
    0,  111,  111,  111,  379,  379,    0,   73,   73,   73,
    0,    0,   73,   73,   73,    0,    0,    0,    0,    0,
    0,  111,    0,  379,   73,   73,   73,   73,   73,   73,
   73,   73,   73,   73,   73,   73,   73,   73,   73,   73,
   73,   73,   73,  379,    0,    0,   73,   73,    0,  379,
   73,    0,   73,   73,   73,   73,   73,    0,    0,   73,
    0,    0,  379,   73,    0,   73,   73,   73,   73,   73,
   73,   73,   73,   73,   73,   73,   73,   73,   73,  453,
   73,   73,   73,    0,   73,   73,   73,   73,   73,   73,
   73,   73,   73,   73,   73,   73,   73,   73,    0,    0,
    0,    0,  381,  382,    0,    0,   73,    0,   73,    0,
    0,   73,   73,   73,    0,    0,   64,   64,   64,    0,
    0,   64,   64,   64,  391,  392,  393,  394,  395,  396,
  397,   73,    0,   64,   64,   64,   64,   64,   64,   64,
   64,   64,   64,   64,   64,   64,   64,   64,   64,   64,
   64,   64,    0,    0,    0,   64,   64,    0,    0,   64,
    0,   64,   64,   64,   64,   64,    0,    0,   64,    0,
    0,    0,   64,    0,   64,   64,   64,   64,   64,   64,
   64,   64,   64,   64,   64,   64,   64,   64,  427,   64,
   64,   64,    0,   64,   64,   64,   64,   64,   64,   64,
   64,   64,   64,   64,   64,   64,   64,    0,    0,    0,
    0,    0,    0,    0,    0,   64,    0,   64,    0,    0,
   64,   64,   64,    0,    0,   74,   74,   74,    0,    0,
   74,   74,   74,    0,    0,    0,    0,    0,    0,    0,
   64,    0,   74,   74,   74,   74,   74,   74,   74,   74,
   74,   74,   74,   74,   74,   74,   74,   74,   74,   74,
   74,    0,    0,    0,   74,   74,    0,    0,   74,    0,
   74,   74,   74,   74,   74,    0,    0,   74,    0,   14,
    0,   74,    0,   74,   74,   74,   74,   74,   74,   74,
   74,   74,   74,   74,   74,   74,   74,    0,   74,   74,
   74,    0,   74,   74,   74,   74,   74,   74,   74,   74,
   74,   74,   74,   74,   74,   74,  384,  384,  384,    0,
    0,  384,  384,    0,   74,    0,   74,    0,    0,   74,
   74,   74,  384,  384,    0,  453,  453,  453,    0,    0,
  453,  453,  453,    0,    0,    0,    0,    0,    0,   74,
    0,  384,  453,  453,  453,  453,  453,  453,  453,  453,
  453,  453,  453,  453,  453,  453,  453,  453,  453,  453,
  453,  384,    0,    0,  453,  453,    0,  384,  453,    0,
  453,  453,  453,  453,  453,  466,    0,  453,    0,    0,
    0,  453,    0,  453,  453,  453,  453,  453,  453,  453,
  453,  453,  453,  453,  453,  453,  453,    0,  453,  453,
  453,    0,  453,  453,  453,  453,  453,  453,  453,  453,
  453,  453,  453,  453,  453,  453,    0,    0,    0,    0,
    0,    0,    0,    0,  453,    0,  453,    0,    0,  453,
  453,  453,    0,    0,  427,  427,  427,    0,    0,  427,
  427,  427,    0,    0,    0,    0,    0,    0,    0,  453,
    0,  427,  427,  427,  427,  427,  427,  427,  427,  427,
  427,  427,  427,  427,  427,  427,  427,  427,  427,  427,
    0,  427,  427,  427,  427,    0,    0,  427,    0,  427,
  427,  427,  427,  427,  427,    0,  427,    0,    0,    0,
  427,    0,  427,  427,  427,  427,  427,  427,  427,  427,
  427,  427,  427,    0,    0,    0,    0,  427,  427,  427,
    0,  427,  427,  427,  427,  427,  427,  427,  427,  427,
  427,  427,  427,  427,  427,   14,   14,   14,    0,    0,
   14,   14,   14,  427,    0,  427,    0,    0,  427,  427,
    0,    0,   14,   14,   14,   14,   14,   14,   14,   14,
   14,   14,   14,   14,   14,   14,   14,   14,   14,   14,
   14,    0,    0,    0,   14,   14,    0,    0,   14,    0,
   14,   14,   14,   14,   14,    0,    0,   14,    0,    0,
    0,   14,    0,   14,   14,   14,   14,   14,   14,   14,
   14,   14,   14,   14,    0,    0,    0,    0,   14,   14,
   14,    0,   14,   14,   14,   14,   14,   14,   14,   14,
   14,   14,   14,   14,   14,   14,    0,    0,  381,  382,
  383,  384,    0,    0,   14,    0,   14,    0,    0,   14,
   14,  466,  466,  466,    0,    0,  466,  466,  466,  390,
  391,  392,  393,  394,  395,  396,  397,    0,  466,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,  466,  466,  466,  466,  466,    0,    8,    0,
  466,  466,    0,    0,    0,    0,  466,  466,  466,  466,
  466,    0,    0,  466,    0,    0,    0,    0,    0,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
    0,    0,    0,  783,  466,  466,  466,    0,  466,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,    0,    0,    0,    0,    0,    0,    0,    0,
  466,    8,  466,    0,  171,  466,  466,    0,  222,  223,
  224,  225,  226,  227,  228,  229,  230,    0,    9,   10,
   11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
   21,   22,   23,   24,   25,   26,  231,    0,   36,    0,
    0,    0,   38,    0,   27,   28,   29,   30,   31,   32,
    0,  232,  218,    0,    0,    0,    0,  145,    0,    0,
    0,  146,    8,    0,    0,  171,   33,   34,   35,  222,
  223,  224,  225,  226,  227,  228,  229,  230,    0,    9,
   10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
   20,   21,   22,   23,   24,   25,   26,  231,    0,    0,
    0,   36,    0,    0,   37,   38,   28,   29,   30,   31,
   32,    0,  232,    0,    0,  233,    0,    0,  234,  235,
  236,    0,    0,    8,  237,  238,  171,   33,   34,   35,
  222,  223,  224,  225,  226,  227,  228,  229,  230,    0,
    9,   10,   11,   12,   13,   14,   15,   16,   17,   18,
   19,   20,   21,   22,   23,   24,   25,   26,  231,  272,
    0,  272,   36,    0,    0,    0,   38,   28,   29,   30,
   31,   32,    0,  232,    0,    0,  264,    0,    0,  234,
  235,  649,    8,    8,    0,  237,  238,    0,   33,   34,
   35,    0,    0,  272,  272,  272,  272,    0,    0,  272,
  272,    0,    0,    0,    0,    0,    0,  272,    0,    0,
  272,    0,    0,    0,    0,    0,    0,  268,  363,    0,
    0,    0,    0,   36,    0,    0,    0,   38,    0,  272,
    0,    0,    0,    0,  142,    0,    0,  233,    0,    0,
  234,  235,  236,  491,    0,    8,  237,  238,  171,  272,
    0,    0,  222,  223,  224,  225,  226,  227,  228,  229,
  230,    0,    9,   10,   11,   12,   13,   14,   15,   16,
   17,   18,   19,   20,   21,   22,   23,   24,   25,   26,
  231,    0,   36,   36,    0,    0,   38,   38,    0,   28,
   29,   30,   31,   32,    0,  232,  529,  529,  424,    0,
  381,  382,  383,  384,  385,  269,  269,    0,    0,    0,
   33,   34,   35,  381,  382,  383,  384,  386,  387,  388,
  389,  492,  391,  392,  393,  394,  493,  396,  397,    0,
  386,  387,  388,  389,  390,  391,  392,  393,  394,  395,
  396,  397,    0,  295,    0,   36,  171,    0,    0,   38,
    0,  223,  224,  225,  226,  227,  228,  229,  230,  233,
    0,    0,  234,  235,  236,    0,    0,    8,  237,  238,
  171,    0,    0,    0,  222,  223,  224,  225,  226,  227,
  228,  229,  230,    0,    9,   10,   11,   12,   13,   14,
   15,   16,   17,   18,   19,   20,   21,   22,   23,   24,
   25,   26,  231,    0,    0,    0,    0,    0,    0,    0,
    0,   28,   29,   30,   31,   32,    0,  232,    0,    0,
  427,    0,    0,    0,    0,    0,    0,    0,    8,    0,
    0,    0,   33,   34,   35,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    9,   10,   11,   12,   13,
   14,   15,   16,   17,   18,   19,   20,   21,   22,   23,
   24,   25,   26,    0,    0,    0,    0,   36,    0,    0,
    0,   38,    0,    0,    0,   31,   32,    0,    0,    0,
    0,  233,    0,    0,  234,  235,  236,    0,    0,    8,
  237,  238,  171,   33,   34,   35,  222,  223,  224,  225,
  226,  227,  228,  229,  230,    0,    9,   10,   11,   12,
   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
   23,   24,   25,   26,  231,    0,    0,    0,   36,    0,
    0,    0,   38,   28,   29,   30,   31,   32,    0,  232,
    0,    0,  527,    0,    0,    0,    0,  145,    0,    0,
    8,    0,    0,  171,   33,   34,   35,  222,  223,  224,
  225,  226,  227,  228,  229,  230,    0,    9,   10,   11,
   12,   13,   14,   15,   16,   17,   18,   19,   20,   21,
   22,   23,   24,   25,   26,  231,  648,    0,    0,   36,
    0,    0,    0,   38,   28,   29,   30,   31,   32,    0,
  232,    0,    0,  233,    0,    0,  234,  235,  236,    0,
    0,    8,  237,  238,  171,   33,   34,   35,  222,  223,
  224,  225,  226,  227,  228,  229,  230,    0,    9,   10,
   11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
   21,   22,   23,   24,   25,   26,  231,    0,    0,    0,
   36,    0,    0,    0,   38,   28,   29,   30,   31,   32,
    0,  232,    0,    0,  233,    0,    0,  234,  235,  236,
    0,    0,  336,  237,  238,  336,   33,   34,   35,  336,
  336,  336,  336,  336,  336,  336,  336,  336,  861,  336,
  336,  336,  336,  336,  336,  336,  336,  336,  336,  336,
  336,  336,  336,  336,  336,  336,  336,  336,    0,    0,
    0,   36,    0,    0,    0,   38,  336,  336,  336,  336,
  336,    0,  336,    0,    0,  233,    0,    0,  234,  235,
  236,    0,    0,    0,  237,  238,    0,  336,  336,  336,
    0,    0,    0,  381,  382,  383,  384,  385,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  386,  387,  388,  389,  390,  391,  392,  393,  394,  395,
  396,  397,  336,    0,    0,    0,  336,    0,    0,    0,
  301,  301,    0,  301,  244,    0,  336,    0,    0,  336,
  336,  336,    0,    0,    0,  336,  336,  301,  301,  301,
  301,  301,  301,  301,  301,  301,  301,  301,  301,  301,
  301,  301,  301,  301,  301,  301,  301,  301,  301,    0,
    0,  301,  301,    0,    0,    0,    0,  301,  301,  301,
    0,    0,  301,  301,    0,    0,    0,    0,  244,    0,
  346,  346,  346,  346,  346,  301,  301,  301,    0,    0,
    0,  301,    0,    0,    0,  301,    0,  346,  346,  346,
  346,  346,  346,  346,  346,  346,  346,  346,  346,  466,
  466,  301,  466,  301,  301,  301,  301,  301,    0,  301,
  301,    0,    0,    0,  301,  413,    0,    0,    0,    0,
  301,  301,  301,  301,  301,  301,  301,  301,  301,  301,
  301,  301,    0,  301,  466,  466,  466,  466,    0,    0,
  466,  466,    0,  466,  466,  466,    0,    0,  466,    0,
    0,  466,  466,    0,    0,  381,  382,  383,  384,  385,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  466,    0,  386,  387,  388,  389,  390,  391,  392,  393,
  394,  395,  396,  397,    0,    0,    0,    0,  320,  320,
  466,  320,  466,  466,  466,  466,  466,    0,    0,  466,
    0,    0,    0,  466,  533,    0,    0,    0,    0,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,    0,  466,  320,  320,  320,  320,    0,    0,  320,
  320,    0,  320,  320,  320,    0,    0,  320,    0,    0,
  320,  320,    0,    0,  381,  382,  383,  384,  385,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  320,
    0,  386,  387,  388,  389,  390,  391,  392,  393,  394,
  395,  396,  397,    0,    0,    0,    0,  308,  308,  320,
  308,  320,  320,  320,  320,  320,    0,    0,  320,    0,
    0,    0,  320,  535,    0,    0,    0,    0,  320,  320,
  320,  320,  320,  320,  320,  320,  320,  320,  320,  320,
    0,  320,  308,  308,  308,  308,    0,    0,  308,  308,
    0,    0,    0,    0,    0,    0,  308,    0,    0,  308,
  308,    0,    0,  381,  382,  383,  384,  385,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  308,    0,
  386,  387,  388,  389,  390,  391,  392,  393,  394,  395,
  396,  397,    0,    0,    0,    0,  307,  307,  308,  307,
  308,  308,  308,  308,  308,    0,    0,  308,    0,    0,
    0,  308,  658,    0,    0,    0,    0,  308,  308,  308,
  308,  308,  308,  308,  308,  308,  308,  308,  308,    0,
  308,  307,  307,  307,  307,    0,    0,  307,  307,    0,
    0,    0,    0,    0,    0,  307,    0,    0,  307,  307,
    0,    0,  381,  382,  383,  384,  385,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  307,    0,  386,
  387,  388,  389,  390,  391,  392,  393,  394,  395,  396,
  397,    0,    0,    0,    0,  299,  299,  307,  299,  307,
  307,  307,  307,  307,    0,    0,  307,    0,    0,    0,
  307,    0,  258,  258,    0,  258,  307,  307,  307,  307,
  307,  307,  307,  307,  307,  307,  307,  307,    0,  307,
  299,  299,  299,  299,    0,    0,  299,  299,    0,    0,
    0,    0,    0,    0,  299,    0,    0,  299,  258,  258,
  258,    0,    0,  258,  258,    0,  258,  258,  258,    0,
    0,  258,  354,  354,  354,    0,  299,  354,  354,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  354,  354,
    0,    0,    0,  258,    0,    0,  299,  258,    0,    0,
    0,    0,    0,    0,    0,  299,    0,  354,    0,  299,
    0,    0,    8,  258,    0,    0,    0,    0,    0,  299,
    0,  258,    0,    0,    0,    0,    0,  354,  299,    9,
   10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
   20,   21,   22,   23,   24,   25,   26,  255,    0,    0,
    0,    0,    0,    0,    0,   27,   28,   29,   30,   31,
   32,    0,    0,    0,  142,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    8,    0,   33,   34,   35,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    9,   10,   11,   12,   13,   14,   15,   16,
   17,   18,   19,   20,   21,   22,   23,   24,   25,   26,
  255,    0,   36,    0,    0,   37,   38,    0,   27,   28,
   29,   30,   31,   32,    0,    0,  256,  142,    0,    0,
    0,  145,    0,    0,    0,    0,    0,    8,    0,    0,
   33,   34,   35,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    9,   10,   11,   12,   13,   14,
   15,   16,   17,   18,   19,   20,   21,   22,   23,   24,
   25,   26,    0,    0,    0,   36,    0,    0,   37,   38,
   27,   28,   29,   30,   31,   32,    0,    7,    8,  400,
    0,    0,    0,    0,  145,    0,    0,    0,    0,    0,
    0,    0,   33,   34,   35,    9,   10,   11,   12,   13,
   14,   15,   16,   17,   18,   19,   20,   21,   22,   23,
   24,   25,   26,    0,    0,    0,    0,    0,    0,    0,
    0,   27,   28,   29,   30,   31,   32,   36,    0,    0,
   37,   38,    0,    0,    0,    0,    0,    0,    0,   51,
    8,  359,    0,   33,   34,   35,  145,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    9,   10,   11,
   12,   13,   14,   15,   16,   17,   18,   19,   20,   21,
   22,   23,   24,   25,   26,    0,    0,    0,   36,    0,
    0,   37,   38,   27,   28,   29,   30,   31,   32,    0,
    0,  466,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   33,   34,   35,  466,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,  466,  466,  466,  466,    0,  381,  382,  383,
  384,    0,    0,  466,    0,  466,  466,  466,  466,  466,
   36,    0,  229,   37,   38,  387,  388,  389,  390,  391,
  392,  393,  394,  395,  396,  397,  466,  466,  466,  229,
  229,  229,  229,  229,  229,  229,  229,  229,  229,  229,
  229,  229,  229,  229,  229,  229,  229,    0,  381,  382,
  383,  384,    0,    0,  169,    0,  229,  229,  229,  229,
  229,  466,  176,  177,    0,  466,  466,  388,  389,  390,
  391,  392,  393,  394,  395,  396,  397,  229,  229,  229,
    9,   10,   11,   12,   13,   14,   15,   16,   17,   18,
   19,   20,   21,   22,   23,   24,   25,   26,  381,  382,
  383,  384,    0,    0,    0,    0,    0,   28,   29,   30,
   31,   32,  229,    0,    8,    0,  229,  229,  389,  390,
  391,  392,  393,  394,  395,  396,  397,    0,   33,   34,
   35,    9,   10,   11,   12,   13,   14,   15,   16,   17,
   18,   19,   20,   21,   22,   23,   24,   25,   26,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   28,   29,
   30,   31,   32,   36,    0,    8,    0,   38,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  213,
   34,   35,    9,   10,   11,   12,   13,   14,   15,   16,
   17,   18,   19,   20,   21,   22,   23,   24,   25,   26,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   28,
   29,   30,   31,   32,   36,    0,  222,    0,   38,  716,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   33,   34,   35,  222,  222,  222,  222,  222,  222,  222,
  222,  222,  222,  222,  222,  222,  222,  222,  222,  222,
  222,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  222,  222,  222,  222,  222,   36,    0,  223,    0,   38,
  716,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  222,  222,  222,  223,  223,  223,  223,  223,  223,
  223,  223,  223,  223,  223,  223,  223,  223,  223,  223,
  223,  223,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  223,  223,  223,  223,  223,  222,    0,  466,    0,
  222,  222,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  223,  223,  223,  466,  466,  466,  466,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,  466,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  466,  466,  466,  466,  466,  223,    0,  226,
    0,  223,  223,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  466,  466,  466,  226,  226,  226,  226,
  226,  226,  226,  226,  226,  226,  226,  226,  226,  226,
  226,  226,  226,  226,    0,    0,    0,    0,    0,    0,
  264,    0,  264,  226,  226,  226,  226,  226,  466,    0,
    0,    0,  466,  466,   55,    0,   56,    0,    0,   57,
   58,   59,    0,    0,  226,  226,  226,  356,  356,  356,
    0,   60,  356,  356,  264,  264,  264,  264,    0,    0,
  264,  264,    0,  356,  356,    0,    0,    0,  264,    0,
    0,  264,    0,   61,   62,    0,    0,  569,    0,  226,
    0,    0,  356,  226,  226,    0,   63,    0,    0,    0,
  264,    0,   64,   65,   66,   67,   68,   69,   70,   71,
    0,    0,  356,    0,    0,    0,    0,   72,   73,   74,
  264,   75,   76,   77,   78,   79,   80,   81,   82,   83,
   84,   85,   86,   87,   88,   55,    0,   56,    8,    0,
   57,   58,   59,   89,    0,    0,  313,    0,   90,    0,
    0,    0,   60,    0,    0,    9,   10,   11,   12,   13,
   14,   15,   16,   17,   18,   19,   20,   21,   22,   23,
   24,   25,   26,    0,   61,   62,    0,    0,  664,    0,
    0,   27,   28,   29,   30,   31,   32,   63,    0,    0,
    0,    0,    0,   64,   65,   66,   67,   68,   69,   70,
   71,    0,    0,   33,   34,   35,    0,    0,   72,   73,
   74,    0,   75,   76,   77,   78,   79,   80,   81,   82,
   83,   84,   85,   86,   87,   88,   55,    0,   56,    8,
    0,   57,   58,   59,   89,    0,    0,    0,   36,   90,
    0,   37,   38,   60,    0,    0,    9,   10,   11,   12,
   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
   23,   24,   25,   26,    0,   61,   62,    0,    0,  680,
    0,    0,   27,   28,   29,   30,   31,   32,   63,    0,
    0,    0,    0,    0,   64,   65,   66,   67,   68,   69,
   70,   71,    0,    0,   33,   34,   35,    0,    0,   72,
   73,   74,    0,   75,   76,   77,   78,   79,   80,   81,
   82,   83,   84,   85,   86,   87,   88,   55,    0,   56,
    8,    0,   57,   58,   59,   89,    0,    0,    0,   36,
   90,    0,   37,   38,   60,    0,    0,    9,   10,   11,
   12,   13,   14,   15,   16,   17,   18,   19,   20,   21,
   22,   23,   24,   25,   26,  202,   61,   62,    0,    0,
    0,    0,    0,    0,   28,   29,   30,   31,   32,   63,
    0,    0,    0,  801,    0,   64,   65,   66,   67,   68,
   69,   70,   71,    0,    0,   33,   34,   35,    0,    0,
   72,   73,   74,    0,   75,   76,   77,   78,   79,   80,
   81,   82,   83,   84,   85,   86,   87,   88,   55,    0,
   56,    8,    0,   57,   58,   59,   89,    0,    0,    0,
   36,   90,    0,    0,   38,   60,    0,    0,    9,   10,
   11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
   21,   22,   23,   24,   25,   26,    0,   61,   62,    0,
    0,    0,    0,    0,    0,   28,   29,   30,   31,   32,
   63,    0,    0,    0,    0,    0,   64,   65,   66,   67,
   68,   69,   70,   71,    0,    0,  213,   34,   35,    0,
    0,   72,   73,   74,    0,   75,   76,   77,   78,   79,
   80,   81,   82,   83,   84,   85,   86,   87,   88,    8,
    0,    0,    0,    0,    0,    0,    0,   89,    0,    0,
    0,   36,   90,  655,    0,   38,    9,   10,   11,   12,
   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
   23,   24,   25,   26,    0,  381,  382,  383,  384,  385,
    0,    0,    0,   28,   29,   30,   31,   32,    0,    0,
  682,    0,  386,  387,  388,  389,  390,  391,  392,  393,
  394,  395,  396,  397,   33,   34,   35,    9,   10,   11,
   12,   13,   14,   15,   16,   17,   18,   19,   20,   21,
   22,   23,   24,   25,   26,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   28,   29,   30,   31,   32,   36,
    0,  466,    0,   38,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   33,   34,   35,  466,  466,
  466,  466,  466,  466,  466,  466,  466,  466,  466,  466,
  466,  466,  466,  466,  466,  466,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  466,  466,  466,  466,  466,
   36,    0,    8,    0,   38,    0,  280,    0,  280,    0,
    0,    0,    0,    0,    0,    0,  466,  466,  466,    9,
   10,   11,   12,   13,   14,   15,   16,   17,   18,   19,
   20,   21,   22,   23,   24,   25,   26,  259,  259,    0,
  259,  280,  280,  280,    0,    0,  280,  280,    0,   31,
   32,  466,    0,    0,  280,  466,    0,  284,    0,  284,
    0,    0,    0,    0,    0,    0,    0,   33,   34,   35,
    0,    0,    0,  259,  259,  259,  280,    0,  259,  259,
    0,  259,  259,  259,    0,    0,  259,    0,  256,  256,
    0,  256,  284,  284,  284,    0,  280,  284,  284,    0,
    0,    0,   36,    0,    0,  284,   38,    0,  259,    0,
    0,    0,  259,    0,    0,    0,    0,    0,    0,    0,
  257,  257,    0,  257,  256,  256,  256,  284,  259,  256,
  256,    0,  256,  256,  256,    0,  259,  256,    0,  260,
  260,    0,  260,    0,    0,    0,    0,  284,    0,    0,
    0,    0,    0,    0,    0,    0,  257,  257,  257,  256,
    0,  257,  257,  256,  257,  257,  257,    0,    0,  257,
    0,  261,  261,    0,  261,  260,  260,  260,    0,  256,
  260,  260,    0,  260,  260,  260,    0,  256,  260,    0,
    0,  257,    0,    0,    0,  257,    0,    0,    0,    0,
    0,    0,    0,  263,  263,    0,  263,  261,  261,  261,
  260,  257,  261,  261,  260,  261,  261,  261,    0,  257,
  261,    0,  262,  262,    0,  262,    0,    0,    0,    0,
  260,    0,    0,    0,    0,    0,    0,    0,  260,  263,
  263,  263,  261,    0,  263,  263,  261,  263,  263,  263,
    0,    0,  263,    0,  272,  272,    0,  272,  262,  262,
  262,    0,  261,  262,  262,    0,  262,  262,  262,    0,
  261,  262,    0,    0,  263,    0,    0,    0,  263,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  272,
    0,  272,  272,  262,  263,  272,    0,  262,  272,  272,
  272,    0,  263,  272,    0,    0,  272,    0,    0,    0,
    0,  264,  264,  262,  264,    0,    0,    0,    0,    0,
    0,  262,    0,    0,    0,  272,    0,    0,    0,  272,
  285,    0,  285,    0,    0,    0,    0,    0,    0,  282,
    0,  282,    0,    0,    0,    0,  264,    0,  264,  264,
    0,    0,  264,  272,    0,  264,  264,  264,    0,    0,
  264,    0,    0,  264,    0,  285,  285,  285,    0,    0,
  285,  285,    0,    0,  282,  282,  282,    0,  285,  282,
  282,    0,  264,    0,    0,    0,  264,  282,    0,    0,
  281,    0,  281,    0,    0,    0,    0,    0,    0,  287,
  285,  287,    0,    0,    0,    0,    0,    0,    0,  282,
  264,    0,    0,  283,    0,  283,    0,    0,    0,    0,
  285,    0,  288,    0,  288,  281,  281,  281,    0,  282,
  281,  281,    0,    0,  287,  287,  287,    0,  281,  287,
  287,    0,    0,    0,    0,    0,    0,  287,  283,  283,
  283,    0,    0,  283,  283,    0,    0,  288,  288,  288,
  281,  283,  288,  288,    0,  289,    0,  289,    0,  287,
  288,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  281,    0,    0,  283,    0,    0,    0,    0,    0,  287,
    0,    0,  288,  347,  347,  347,    0,    0,  347,  347,
  289,  289,  289,  283,    0,  289,  289,    0,    0,  347,
  347,    0,  288,  289,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  349,  349,  349,    0,  347,  349,
  349,    0,    0,    0,    0,  289,    0,    0,    0,    0,
  349,  349,    0,    0,    0,    0,    0,    0,  347,    0,
  347,  347,  347,  347,  347,  289,    0,    0,    0,  349,
    0,    0,    0,  466,    0,    0,    0,  347,  347,  347,
  347,  347,  347,  347,  347,  347,  347,  347,  347,  349,
    0,  349,  349,  349,  349,  349,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  349,  349,
  349,  349,  349,  349,  349,  349,  349,  349,  349,  349,
  353,  353,  353,    0,    0,  353,  353,  466,    0,  306,
  306,  306,  306,  306,    0,    0,  353,  353,    0,    0,
    0,    0,    0,    0,    0,    0,  306,  306,  306,  306,
    0,  306,  306,  306,  306,  353,  306,  306,    0,    0,
    0,    0,    0,    0,    0,    0,  368,  368,  368,    0,
    0,  368,  368,    0,    0,  353,    0,  353,  353,  353,
  353,  353,  368,  368,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  353,  353,  353,  353,    0,  353,
  353,  368,    0,    0,  353,  353,    0,    0,    0,    0,
    0,    0,    0,    0,  369,  369,  369,    0,    0,  369,
  369,  368,    0,  368,  368,  368,  368,  368,    0,    0,
  369,  369,    0,    0,    0,    0,    0,    0,    0,    0,
  368,  368,  368,  368,  368,  368,  368,  368,  368,  369,
  376,  376,  376,    0,    0,  376,  376,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  376,  376,    0,  369,
    0,  369,  369,  369,  369,  369,    0,    0,    0,    0,
    0,  377,  377,  377,    0,  376,  377,  377,  369,  369,
  369,  369,  369,  369,  369,  369,  369,  377,  377,    0,
    0,    0,    0,    0,    0,  376,    0,  376,  376,  376,
  376,  376,    0,    0,    0,    0,  377,  383,  383,  383,
    0,    0,  383,  383,  376,  376,  376,  376,  376,  376,
  376,    0,    0,  383,  383,    0,  377,    0,  377,  377,
  377,  377,  377,    0,    0,    0,    0,    0,  382,  382,
  382,    0,  383,  382,  382,  377,  377,  377,  377,  377,
  377,  377,    0,    0,  382,  382,    0,    0,    0,    0,
    0,    0,  383,    0,  383,  383,  383,  383,  383,    0,
  380,  380,  380,  382,    0,  380,  380,    0,    0,    0,
    0,  383,  383,  383,  383,  383,  380,  380,    0,    0,
    0,    0,    0,  382,    0,  382,  382,  382,  382,  382,
    0,  381,  381,  381,    0,  380,  381,  381,    0,    0,
    0,    0,  382,  382,  382,  382,  382,  381,  381,    0,
    0,    0,    0,    0,    0,  380,    0,    0,    0,  380,
  380,  380,    0,  358,  358,  358,  381,    0,  358,  358,
    0,    0,    0,    0,  380,  380,  380,  380,  380,  358,
  358,    0,    0,    0,    0,    0,  381,    0,    0,    0,
  381,  381,  381,    0,  373,  373,  373,    0,  358,  373,
  373,    0,    0,    0,    0,  381,  381,  381,  381,  381,
  373,  373,    0,    0,    0,    0,    0,    0,  358,    0,
    0,    0,    0,    0,  358,    0,  375,  375,  375,  373,
    0,  375,  375,    0,    0,    0,    0,  358,  358,  358,
  358,  358,  375,  375,    0,    0,    0,    0,    0,  373,
    0,    0,    0,    0,    0,  373,    0,  374,  374,  374,
    0,  375,  374,  374,    0,    0,    0,    0,  373,  373,
  373,  373,  373,  374,  374,    0,    0,    0,    0,    0,
    0,  375,    0,    0,    0,    0,    0,  375,  378,  378,
  378,    0,  374,  378,  378,    0,    0,    0,    0,    0,
  375,  375,  375,  375,  378,  378,    0,    0,    0,    0,
  662,    0,  374,    0,    0,    0,    0,    0,  374,    0,
    0,    0,    0,  378,  355,  355,  355,    0,    0,  355,
  355,  374,  374,  374,    0,    0,    0,    0,    0,    0,
  355,  355,    0,  378,    0,    0,    0,    0,    0,  378,
  381,  382,  383,  384,  385,    0,    0,    0,    0,  355,
    0,    0,  378,  378,    0,    0,    0,  386,  387,  388,
  389,  390,  391,  392,  393,  394,  395,  396,  397,  355,
  357,  357,  357,    0,    0,  357,  357,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  357,  357,    0,  381,
  382,  383,  384,  385,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  357,  386,  387,  388,  389,
  390,  391,  392,  393,  394,  395,  396,  397,    0,    0,
    0,    0,    0,    0,    0,  357,    9,   10,   11,   12,
   13,   14,   15,   16,    0,   18,    0,   20,    0,    0,
   23,   24,   25,   26,
};
short yycheck[] = {                                      42,
    0,  304,  320,  211,   84,    5,  446,  130,    8,  136,
   42,    5,  128,   44,   43,  170,   45,  327,    5,  174,
  549,   21,  563,  144,  465,  180,    5,  148,  257,  196,
  185,  731,  159,  160,  260,  292,  257,  293,  256,    5,
  258,  148,   42,    5,   90,    5,  438,  347,   68,  258,
  217,  260,  207,  257,  446,  140,  293,    5,  257,    5,
  257,    5,  178,   63,    5,  220,  260,    5,   68,   69,
  293,  257,  257,  557,  774,  306,    5,  717,  300,  779,
  126,  295,  566,  293,  257,  311,  295,    5,  204,  298,
  294,  137,  621,  292,    5,  257,  858,  328,    5,  215,
  382,  295,  358,    1,    2,  347,  149,  127,  140,  152,
  309,    5,  219,  145,  371,    5,  145,  435,  257,  204,
  882,  358,  306,  564,  295,    5,    5,  127,  157,    5,
  130,  257,  361,  347,    5,  256,  454,  258,    5,  257,
  361,  359,  782,  361,  348,  295,  456,  356,  371,  149,
  336,  635,  152,  357,  295,  276,   54,  361,  357,  298,
  644,  371,  361,  347,  361,  865,  292,  167,  293,  276,
  255,  357,  204,  295,  292,  361,  361,  177,  707,  286,
  258,  380,  260,  268,  357,  217,  257,  306,  361,  256,
  190,  309,  410,  677,  257,  357,  725,  347,  371,  361,
  298,  244,  328,  376,  204,  205,  328,  207,  295,  258,
  751,  211,  244,  213,  376,  294,  257,  295,  357,  260,
  298,  292,  361,  255,  257,  292,  260,  306,  347,  670,
  257,  357,  130,  673,  260,  361,  268,  292,  309,  357,
  240,  273,  140,  361,  244,  371,  371,  776,  257,  298,
  376,  260,  332,  376,  380,  257,  292,  364,  466,  292,
  367,  359,  380,  306,  298,  328,  362,  260,  486,  753,
  437,  298,  298,  305,  257,   64,  309,  260,  356,  400,
  401,  673,  253,  292,  382,  406,  357,  257,  186,  187,
  361,  446,  257,  414,  357,   84,   85,  464,  361,  257,
  371,  422,  258,  292,  202,  376,  306,  414,  257,  380,
  292,  292,  210,  211,  755,  422,  258,  257,  260,  290,
  320,  850,  429,  355,  357,  643,  810,  356,  361,  445,
  357,  363,  300,  449,  361,  371,  368,  126,  371,  258,
  376,  260,  298,  292,  298,  345,    1,    2,  357,  359,
  350,  358,  361,  399,  354,  357,  356,  255,  358,  361,
  293,  294,  371,  306,  292,  257,  292,  376,  852,  485,
  268,  380,  382,  306,  357,  258,  860,  260,  361,  298,
  487,  309,  489,  281,   54,  328,  284,  357,  371,  332,
  359,  361,  357,  376,  292,  762,  361,  764,  519,  357,
  292,  292,  257,  361,  257,  258,  298,  260,  357,  301,
  302,  303,  361,  382,  301,  302,  303,  357,  525,  292,
  257,  361,  529,  260,  727,  358,  359,  292,  799,  295,
  292,  380,  298,  433,  292,  435,  328,  292,  294,  292,
  483,  292,  295,  299,  309,  298,  817,  309,  564,  382,
  821,  309,  495,  453,  454,  455,  293,  355,  292,  453,
  460,  493,  328,  495,  493,  357,  453,  359,  455,  361,
  292,  471,  472,  600,  453,  130,  293,  294,  376,  371,
  294,  304,  305,  483,  376,  299,  292,  453,  380,  376,
  382,  453,  294,  453,  292,  495,  496,  299,  665,  666,
  323,  324,  357,  356,  357,  453,  361,  453,  361,  453,
  304,  305,  453,  257,  885,  453,  186,  187,  371,  417,
  675,  637,  293,  376,  453,  380,  294,  380,  557,  323,
  324,  299,  202,  293,  294,  453,  877,  566,  327,  646,
  210,  541,  453,  332,  294,  295,  453,  541,  298,  549,
  448,  892,  893,  347,  541,  294,  211,  557,  674,  453,
  295,  902,  541,  453,  462,  257,  566,  683,  466,  692,
  570,  293,  294,  453,  453,  541,  570,  453,  257,  541,
  369,  541,  453,  570,  630,  712,  453,  587,  588,  295,
  706,  570,  298,  541,  588,  541,  899,  541,  714,  295,
  541,  588,  718,  541,  570,  685,  635,  298,  570,  588,
  570,  281,  541,  656,  284,  644,  257,  649,  650,  650,
  649,  621,  570,  541,  570,  257,  570,  260,  295,  570,
  541,  298,  570,  633,  541,  635,  298,  292,  754,  306,
  257,  570,  294,  643,  644,  293,  294,  541,  677,  293,
  294,  541,  570,  769,  292,  293,  656,  703,  306,  570,
  295,  541,  541,  570,  258,  541,  260,  456,  293,  294,
  541,  309,  295,  358,  541,  298,  570,  677,  576,  292,
  570,  292,  682,  306,  294,  355,  293,  301,  302,  303,
  570,  570,  692,  295,  570,  258,  298,  260,  348,  570,
  273,  295,  731,  570,  298,  705,  348,  707,  325,  326,
  327,  293,  306,  292,  293,  358,  832,  717,  332,  295,
  309,  376,  298,  296,  753,  725,  295,  376,  728,  298,
  309,  731,  295,  733,  328,  298,  299,  358,  636,  733,
  357,  295,  858,  306,  361,  774,  733,  417,  780,  257,
  779,  783,  260,  753,  733,  798,  329,  330,  331,  293,
  293,  334,  356,  293,  294,  295,  882,  733,  257,  299,
  668,  733,  292,  733,  774,  295,  776,  295,  298,  779,
  298,  810,  782,  572,  293,  733,  786,  733,   33,  733,
  257,  298,  733,  448,  692,  733,   41,   42,  798,  842,
  293,  294,  295,  292,  733,  298,  299,  462,  293,  298,
  810,  466,  301,  302,  303,  733,  257,  295,  348,  258,
  298,  260,  733,  852,  306,  292,  733,  293,  726,  294,
  830,  860,   77,   78,  732,  293,  865,  293,  295,  733,
  293,  630,  842,  733,  293,  874,  293,  293,  877,  747,
  850,  292,  852,  733,  733,  348,  295,  733,  293,  298,
  860,  294,  733,  892,  893,  865,  733,  306,  357,  292,
  293,  348,  361,  902,  874,  294,  295,  877,  298,  298,
  778,  293,  371,  128,   62,  783,  309,  376,  358,  358,
  357,  380,  892,  893,  361,  140,  685,  295,  358,  144,
  293,  146,  902,  148,  371,   83,  576,  358,  806,  376,
   88,  292,  293,  380,  703,  258,  357,  260,  293,  295,
  361,  819,  295,  293,  822,  292,  298,  716,  309,  295,
  371,  292,  295,  178,  299,  376,  181,  292,  293,  380,
  301,  302,  303,  188,  257,  306,  257,  292,  293,  298,
  293,  294,  295,  293,  309,  298,  299,  855,  856,  204,
  301,  302,  303,  306,  309,  306,  636,  212,  213,  214,
  215,  332,  217,  218,  219,  292,  294,  142,  143,  292,
  294,  257,  880,  292,  293,  328,  293,  328,  301,  302,
  303,  332,  170,  292,  306,  173,  174,  382,  668,  244,
  309,  347,  180,  299,  293,  348,  306,  185,  298,  328,
  255,  256,  328,  258,  299,  328,  292,  292,  196,  363,
  364,  365,  328,  268,  269,  301,  302,  303,  380,  207,
  208,  276,  374,  375,  376,  377,  378,  692,  216,  217,
  293,  286,  220,  257,  357,  301,  302,  303,  361,  376,
  377,  378,  325,  326,  327,    0,  726,  293,  371,  330,
  305,  299,  732,  376,  257,  292,  231,  380,  233,  234,
  235,  236,  237,  238,  319,  332,  293,  747,  292,  295,
  293,  357,  257,  359,  292,  361,  295,  294,  257,  258,
  293,  260,  292,  328,  295,  371,  293,  295,  263,  292,
  376,  295,  295,  294,  380,  258,  382,  260,  778,  348,
  355,  292,  298,  783,  359,  299,  361,  282,  363,  364,
  285,  293,  367,  292,  293,  294,  295,  295,  295,  298,
  299,  293,  293,  299,  293,  257,  806,  306,  295,  295,
  309,  294,  295,  357,  292,  298,  299,  361,  298,  819,
  293,  292,  822,  306,  294,  293,  401,  371,  295,  328,
  293,  293,  376,  292,  357,  410,  380,  293,  361,  414,
  292,  257,  293,  295,  293,  295,  293,  422,  371,  348,
  257,  260,  298,  376,  429,  855,  856,  380,  357,  293,
  348,  293,  361,  293,  295,  293,  293,  293,  293,  257,
  445,  257,  371,  347,  449,  298,  292,  705,  293,  295,
  880,  380,  347,  347,  588,  433,  381,  382,  383,  384,
  385,  386,  387,  388,  389,  390,  391,  392,  393,  394,
  395,  396,  397,  587,  292,  357,  292,  588,  588,  361,
  485,  486,  487,  466,  489,  376,  411,  692,  306,  371,
  495,  309,  588,  418,  376,  496,  434,  786,  380,  437,
  438,  460,   21,  495,  860,  573,  444,  445,  446,  514,
  328,  357,   49,  830,  438,  361,  257,  869,   -1,   -1,
  525,   -1,   -1,   -1,  529,  371,  464,  465,   -1,   -1,
  376,   -1,   -1,   -1,  380,   -1,   -1,   -1,   -1,  357,
   -1,  357,   -1,  361,   -1,  361,   -1,  485,   -1,   -1,
   -1,  292,  257,  371,   -1,  371,   -1,  257,  376,  564,
  257,   -1,  380,   -1,  380,   -1,  491,  492,  493,  274,
  275,  276,  277,  278,  279,  280,  281,  282,  283,  284,
  285,  286,  287,  288,  289,  290,  291,   -1,   -1,   -1,
   -1,   -1,  292,  293,  294,  292,  301,  302,  303,  304,
  305,  301,  302,  303,   -1,    0,  306,   -1,   -1,  309,
   -1,   -1,  309,   -1,   -1,  257,  357,  322,  323,  324,
  361,   -1,   -1,   -1,   -1,  630,  564,   -1,  328,   -1,
  371,   -1,  637,  571,   -1,  573,   -1,   -1,   -1,  380,
  257,  646,  293,  294,  295,   -1,   -1,  298,  299,  273,
  292,   -1,  357,  295,   -1,   -1,  361,  357,  309,   -1,
  357,  361,   -1,   -1,  361,   -1,   -1,   -1,  257,  674,
   -1,  371,  296,  257,  371,  292,  376,  294,  683,   -1,
  380,   -1,   -1,  380,  301,  302,  303,   -1,   -1,  306,
   -1,   -1,  309,  293,  294,  295,   -1,  348,  298,  299,
   -1,  706,   -1,  292,   -1,  329,  330,  331,  292,  714,
  334,  328,  717,  718,   -1,  357,   -1,   -1,   -1,  361,
  309,   -1,   -1,  648,  649,  309,  651,  665,  666,  371,
  655,   -1,  670,    0,  376,  673,  360,  675,  380,   -1,
  357,   -1,   -1,   -1,  361,  256,   -1,  258,  348,  754,
  261,  262,  263,   -1,  371,   -1,   -1,   -1,   -1,  376,
   -1,   -1,  273,  380,  769,   -1,   -1,   -1,  357,   -1,
   -1,   -1,  361,  357,   -1,   -1,   -1,  361,  783,  704,
   -1,   -1,  371,   -1,  295,  296,  791,  371,   -1,   -1,
  795,  380,   -1,   -1,   -1,   -1,  380,   -1,   -1,   -1,
  258,   -1,  260,  314,  315,  316,  317,  318,  319,  320,
  321,   -1,   -1,  738,  325,  326,  327,  755,  329,  330,
  331,   -1,  333,  334,  335,   -1,   -1,  832,  339,   -1,
  341,  342,  343,  344,  345,  346,  294,  295,   -1,   -1,
  298,  299,   -1,   -1,  355,   -1,   -1,   -1,  306,  360,
   -1,   -1,   -1,  858,   -1,   -1,   -1,  862,    0,  784,
   -1,  256,  257,  258,  869,  260,  261,  262,  263,  380,
  372,  373,  374,  375,  376,  377,  378,  882,  273,  274,
  275,  276,  277,  278,  279,  280,  281,  282,  283,  284,
  285,  286,  287,  288,  289,  290,  291,  292,  293,  294,
  295,  296,   -1,  298,  299,   -1,  301,  302,  303,  304,
  305,  306,   -1,  308,  309,  310,   -1,  312,   -1,  314,
  315,  316,  317,  318,  319,  320,  321,  322,  323,  324,
  325,  326,  327,  328,  329,  330,  331,  332,  333,  334,
  335,  336,  337,  338,  339,  340,  341,  342,  343,  344,
  345,  346,   -1,  348,   -1,  350,  351,  352,  353,  354,
  355,  356,  357,  358,  359,  360,  361,  362,   -1,   -1,
   -1,   -1,  367,  368,  369,  370,  371,  372,  373,  374,
  375,  376,  377,  378,    0,  380,   -1,  382,   -1,  256,
  257,  258,  260,  260,  261,  262,  263,  265,  266,  267,
  268,  269,  270,  271,  272,   -1,  273,  274,  275,  276,
  277,  278,  279,  280,  281,  282,  283,  284,  285,  286,
  287,  288,  289,  290,  291,  292,  293,  294,  295,  296,
   -1,  298,  299,   -1,  301,  302,  303,  304,  305,  306,
   -1,  308,  309,  310,   -1,  312,   -1,  314,  315,  316,
  317,  318,  319,  320,  321,  322,  323,  324,  325,  326,
  327,  328,  329,  330,  331,  332,  333,  334,  335,  336,
  337,  338,  339,  340,  341,  342,  343,  344,  345,  346,
   -1,  348,   -1,  350,  351,  352,  353,  354,  355,  356,
  357,  358,   -1,  360,  361,  362,   -1,   -1,   -1,   -1,
  367,  368,  369,  370,  371,  372,  373,  374,  375,  376,
  377,  378,    0,  380,  256,  257,  258,   -1,  260,  261,
  262,  263,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  273,  274,  275,  276,  277,  278,  279,  280,  281,
  282,  283,  284,  285,  286,  287,  288,  289,  290,  291,
  292,  293,  294,  295,  296,   -1,  298,  299,   -1,  301,
  302,  303,  304,  305,  306,   -1,  308,  309,  310,   -1,
  312,   -1,  314,  315,  316,  317,  318,  319,  320,  321,
  322,  323,  324,  325,  326,  327,  328,  329,  330,  331,
  332,  333,  334,  335,  336,  337,  338,  339,  340,  341,
  342,  343,  344,  345,  346,   -1,  348,   -1,  350,  351,
  352,  353,  354,  355,  356,  357,  358,   -1,  360,  361,
  362,    0,   -1,   -1,   -1,  367,  368,  369,  370,  371,
  372,  373,  374,  375,  376,  377,  378,   -1,  380,   -1,
  256,  257,  258,   -1,  260,  261,  262,  263,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  273,  274,  275,
  276,  277,  278,  279,  280,  281,  282,  283,  284,  285,
  286,  287,  288,  289,  290,  291,  292,  293,  294,  295,
  296,   -1,  298,  299,   -1,  301,  302,  303,  304,  305,
  306,   -1,  308,  309,  310,   -1,  312,   -1,  314,  315,
  316,  317,  318,  319,  320,  321,  322,  323,  324,  325,
  326,  327,  328,  329,  330,  331,  332,  333,  334,  335,
  336,  337,  338,  339,  340,  341,  342,  343,  344,  345,
  346,    0,  348,   -1,  350,  351,  352,  353,  354,  355,
  356,  357,  358,   -1,  360,  361,  362,   -1,   -1,   -1,
   -1,  367,  368,  369,  370,  371,  372,  373,  374,  375,
  376,  377,  378,   -1,  380,   -1,   -1,   -1,  256,  257,
  258,   -1,   -1,  261,  262,  263,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  273,  274,  275,  276,  277,
  278,  279,  280,  281,  282,  283,  284,  285,  286,  287,
  288,  289,  290,  291,   -1,   -1,   -1,  295,  296,   -1,
   -1,  299,   -1,  301,  302,  303,  304,  305,   -1,   -1,
  308,   -1,   -1,   -1,  312,   -1,  314,  315,  316,  317,
  318,  319,  320,  321,  322,  323,  324,  325,  326,  327,
    0,  329,  330,  331,   -1,  333,  334,  335,  336,  337,
  338,  339,  340,  341,  342,  343,  344,  345,  346,   -1,
   -1,   -1,   -1,  350,  351,  352,  353,  355,   -1,  357,
   -1,   -1,  360,  361,  362,   -1,   -1,  256,  257,  258,
   -1,   -1,  261,  262,  263,  372,  373,  374,  375,  376,
  377,  378,  380,   -1,  273,  274,  275,  276,  277,  278,
  279,  280,  281,  282,  283,  284,  285,  286,  287,  288,
  289,  290,  291,   -1,   -1,   -1,  295,  296,   -1,   -1,
  299,   -1,  301,  302,  303,  304,  305,   -1,   -1,  308,
   -1,   -1,   -1,  312,   -1,  314,  315,  316,  317,  318,
  319,  320,  321,  322,  323,  324,  325,  326,  327,    0,
  329,  330,  331,   -1,  333,  334,  335,  336,  337,  338,
  339,  340,  341,  342,  343,  344,  345,  346,  293,  294,
  295,   -1,   -1,  298,  299,   -1,  355,   -1,  357,   -1,
   -1,  360,  361,  362,  309,  310,   -1,  256,  257,  258,
   -1,   -1,  261,  262,  263,   -1,   -1,   -1,   -1,   -1,
   -1,  380,   -1,  328,  273,  274,  275,  276,  277,  278,
  279,  280,  281,  282,  283,  284,  285,  286,  287,  288,
  289,  290,  291,  348,   -1,   -1,  295,  296,   -1,  354,
  299,   -1,  301,  302,  303,  304,  305,   -1,   -1,  308,
   -1,   -1,  367,  312,   -1,  314,  315,  316,  317,  318,
  319,  320,  321,  322,  323,  324,  325,  326,  327,    0,
  329,  330,  331,   -1,  333,  334,  335,  336,  337,  338,
  339,  340,  341,  342,  343,  344,  345,  346,   -1,   -1,
   -1,   -1,  350,  351,   -1,   -1,  355,   -1,  357,   -1,
   -1,  360,  361,  362,   -1,   -1,  256,  257,  258,   -1,
   -1,  261,  262,  263,  372,  373,  374,  375,  376,  377,
  378,  380,   -1,  273,  274,  275,  276,  277,  278,  279,
  280,  281,  282,  283,  284,  285,  286,  287,  288,  289,
  290,  291,   -1,   -1,   -1,  295,  296,   -1,   -1,  299,
   -1,  301,  302,  303,  304,  305,   -1,   -1,  308,   -1,
   -1,   -1,  312,   -1,  314,  315,  316,  317,  318,  319,
  320,  321,  322,  323,  324,  325,  326,  327,    0,  329,
  330,  331,   -1,  333,  334,  335,  336,  337,  338,  339,
  340,  341,  342,  343,  344,  345,  346,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  355,   -1,  357,   -1,   -1,
  360,  361,  362,   -1,   -1,  256,  257,  258,   -1,   -1,
  261,  262,  263,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  380,   -1,  273,  274,  275,  276,  277,  278,  279,  280,
  281,  282,  283,  284,  285,  286,  287,  288,  289,  290,
  291,   -1,   -1,   -1,  295,  296,   -1,   -1,  299,   -1,
  301,  302,  303,  304,  305,   -1,   -1,  308,   -1,    0,
   -1,  312,   -1,  314,  315,  316,  317,  318,  319,  320,
  321,  322,  323,  324,  325,  326,  327,   -1,  329,  330,
  331,   -1,  333,  334,  335,  336,  337,  338,  339,  340,
  341,  342,  343,  344,  345,  346,  293,  294,  295,   -1,
   -1,  298,  299,   -1,  355,   -1,  357,   -1,   -1,  360,
  361,  362,  309,  310,   -1,  256,  257,  258,   -1,   -1,
  261,  262,  263,   -1,   -1,   -1,   -1,   -1,   -1,  380,
   -1,  328,  273,  274,  275,  276,  277,  278,  279,  280,
  281,  282,  283,  284,  285,  286,  287,  288,  289,  290,
  291,  348,   -1,   -1,  295,  296,   -1,  354,  299,   -1,
  301,  302,  303,  304,  305,    0,   -1,  308,   -1,   -1,
   -1,  312,   -1,  314,  315,  316,  317,  318,  319,  320,
  321,  322,  323,  324,  325,  326,  327,   -1,  329,  330,
  331,   -1,  333,  334,  335,  336,  337,  338,  339,  340,
  341,  342,  343,  344,  345,  346,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  355,   -1,  357,   -1,   -1,  360,
  361,  362,   -1,   -1,  256,  257,  258,   -1,   -1,  261,
  262,  263,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  380,
   -1,  273,  274,  275,  276,  277,  278,  279,  280,  281,
  282,  283,  284,  285,  286,  287,  288,  289,  290,  291,
   -1,  293,  294,  295,  296,   -1,   -1,  299,   -1,  301,
  302,  303,  304,  305,  306,   -1,  308,   -1,   -1,   -1,
  312,   -1,  314,  315,  316,  317,  318,  319,  320,  321,
  322,  323,  324,   -1,   -1,   -1,   -1,  329,  330,  331,
   -1,  333,  334,  335,  336,  337,  338,  339,  340,  341,
  342,  343,  344,  345,  346,  256,  257,  258,   -1,   -1,
  261,  262,  263,  355,   -1,  357,   -1,   -1,  360,  361,
   -1,   -1,  273,  274,  275,  276,  277,  278,  279,  280,
  281,  282,  283,  284,  285,  286,  287,  288,  289,  290,
  291,   -1,   -1,   -1,  295,  296,   -1,   -1,  299,   -1,
  301,  302,  303,  304,  305,   -1,   -1,  308,   -1,   -1,
   -1,  312,   -1,  314,  315,  316,  317,  318,  319,  320,
  321,  322,  323,  324,   -1,   -1,   -1,   -1,  329,  330,
  331,   -1,  333,  334,  335,  336,  337,  338,  339,  340,
  341,  342,  343,  344,  345,  346,   -1,   -1,  350,  351,
  352,  353,   -1,   -1,  355,   -1,  357,   -1,   -1,  360,
  361,  256,  257,  258,   -1,   -1,  261,  262,  263,  371,
  372,  373,  374,  375,  376,  377,  378,   -1,  273,  274,
  275,  276,  277,  278,  279,  280,  281,  282,  283,  284,
  285,  286,  287,  288,  289,  290,  291,   -1,  257,   -1,
  295,  296,   -1,   -1,   -1,   -1,  301,  302,  303,  304,
  305,   -1,   -1,  308,   -1,   -1,   -1,   -1,   -1,  314,
  315,  316,  317,  318,  319,  320,  321,  322,  323,  324,
   -1,   -1,   -1,  292,  329,  330,  331,   -1,  333,  334,
  335,  336,  337,  338,  339,  340,  341,  342,  343,  344,
  345,  346,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  355,  257,  357,   -1,  260,  360,  361,   -1,  264,  265,
  266,  267,  268,  269,  270,  271,  272,   -1,  274,  275,
  276,  277,  278,  279,  280,  281,  282,  283,  284,  285,
  286,  287,  288,  289,  290,  291,  292,   -1,  357,   -1,
   -1,   -1,  361,   -1,  300,  301,  302,  303,  304,  305,
   -1,  307,  371,   -1,   -1,   -1,   -1,  376,   -1,   -1,
   -1,  380,  257,   -1,   -1,  260,  322,  323,  324,  264,
  265,  266,  267,  268,  269,  270,  271,  272,   -1,  274,
  275,  276,  277,  278,  279,  280,  281,  282,  283,  284,
  285,  286,  287,  288,  289,  290,  291,  292,   -1,   -1,
   -1,  357,   -1,   -1,  360,  361,  301,  302,  303,  304,
  305,   -1,  307,   -1,   -1,  371,   -1,   -1,  374,  375,
  376,   -1,   -1,  257,  380,  381,  260,  322,  323,  324,
  264,  265,  266,  267,  268,  269,  270,  271,  272,   -1,
  274,  275,  276,  277,  278,  279,  280,  281,  282,  283,
  284,  285,  286,  287,  288,  289,  290,  291,  292,  258,
   -1,  260,  357,   -1,   -1,   -1,  361,  301,  302,  303,
  304,  305,   -1,  307,   -1,   -1,  310,   -1,   -1,  374,
  375,  376,  257,  257,   -1,  380,  381,   -1,  322,  323,
  324,   -1,   -1,  292,  293,  294,  295,   -1,   -1,  298,
  299,   -1,   -1,   -1,   -1,   -1,   -1,  306,   -1,   -1,
  309,   -1,   -1,   -1,   -1,   -1,   -1,  292,  292,   -1,
   -1,   -1,   -1,  357,   -1,   -1,   -1,  361,   -1,  328,
   -1,   -1,   -1,   -1,  309,   -1,   -1,  371,   -1,   -1,
  374,  375,  376,  293,   -1,  257,  380,  381,  260,  348,
   -1,   -1,  264,  265,  266,  267,  268,  269,  270,  271,
  272,   -1,  274,  275,  276,  277,  278,  279,  280,  281,
  282,  283,  284,  285,  286,  287,  288,  289,  290,  291,
  292,   -1,  357,  357,   -1,   -1,  361,  361,   -1,  301,
  302,  303,  304,  305,   -1,  307,  371,  371,  310,   -1,
  350,  351,  352,  353,  354,  380,  380,   -1,   -1,   -1,
  322,  323,  324,  350,  351,  352,  353,  367,  368,  369,
  370,  371,  372,  373,  374,  375,  376,  377,  378,   -1,
  367,  368,  369,  370,  371,  372,  373,  374,  375,  376,
  377,  378,   -1,  257,   -1,  357,  260,   -1,   -1,  361,
   -1,  265,  266,  267,  268,  269,  270,  271,  272,  371,
   -1,   -1,  374,  375,  376,   -1,   -1,  257,  380,  381,
  260,   -1,   -1,   -1,  264,  265,  266,  267,  268,  269,
  270,  271,  272,   -1,  274,  275,  276,  277,  278,  279,
  280,  281,  282,  283,  284,  285,  286,  287,  288,  289,
  290,  291,  292,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  301,  302,  303,  304,  305,   -1,  307,   -1,   -1,
  310,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  257,   -1,
   -1,   -1,  322,  323,  324,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  274,  275,  276,  277,  278,
  279,  280,  281,  282,  283,  284,  285,  286,  287,  288,
  289,  290,  291,   -1,   -1,   -1,   -1,  357,   -1,   -1,
   -1,  361,   -1,   -1,   -1,  304,  305,   -1,   -1,   -1,
   -1,  371,   -1,   -1,  374,  375,  376,   -1,   -1,  257,
  380,  381,  260,  322,  323,  324,  264,  265,  266,  267,
  268,  269,  270,  271,  272,   -1,  274,  275,  276,  277,
  278,  279,  280,  281,  282,  283,  284,  285,  286,  287,
  288,  289,  290,  291,  292,   -1,   -1,   -1,  357,   -1,
   -1,   -1,  361,  301,  302,  303,  304,  305,   -1,  307,
   -1,   -1,  310,   -1,   -1,   -1,   -1,  376,   -1,   -1,
  257,   -1,   -1,  260,  322,  323,  324,  264,  265,  266,
  267,  268,  269,  270,  271,  272,   -1,  274,  275,  276,
  277,  278,  279,  280,  281,  282,  283,  284,  285,  286,
  287,  288,  289,  290,  291,  292,  293,   -1,   -1,  357,
   -1,   -1,   -1,  361,  301,  302,  303,  304,  305,   -1,
  307,   -1,   -1,  371,   -1,   -1,  374,  375,  376,   -1,
   -1,  257,  380,  381,  260,  322,  323,  324,  264,  265,
  266,  267,  268,  269,  270,  271,  272,   -1,  274,  275,
  276,  277,  278,  279,  280,  281,  282,  283,  284,  285,
  286,  287,  288,  289,  290,  291,  292,   -1,   -1,   -1,
  357,   -1,   -1,   -1,  361,  301,  302,  303,  304,  305,
   -1,  307,   -1,   -1,  371,   -1,   -1,  374,  375,  376,
   -1,   -1,  257,  380,  381,  260,  322,  323,  324,  264,
  265,  266,  267,  268,  269,  270,  271,  272,  295,  274,
  275,  276,  277,  278,  279,  280,  281,  282,  283,  284,
  285,  286,  287,  288,  289,  290,  291,  292,   -1,   -1,
   -1,  357,   -1,   -1,   -1,  361,  301,  302,  303,  304,
  305,   -1,  307,   -1,   -1,  371,   -1,   -1,  374,  375,
  376,   -1,   -1,   -1,  380,  381,   -1,  322,  323,  324,
   -1,   -1,   -1,  350,  351,  352,  353,  354,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  367,  368,  369,  370,  371,  372,  373,  374,  375,  376,
  377,  378,  357,   -1,   -1,   -1,  361,   -1,   -1,   -1,
  257,  258,   -1,  260,  294,   -1,  371,   -1,   -1,  374,
  375,  376,   -1,   -1,   -1,  380,  381,  274,  275,  276,
  277,  278,  279,  280,  281,  282,  283,  284,  285,  286,
  287,  288,  289,  290,  291,  292,  293,  294,  295,   -1,
   -1,  298,  299,   -1,   -1,   -1,   -1,  304,  305,  306,
   -1,   -1,  309,  310,   -1,   -1,   -1,   -1,  348,   -1,
  350,  351,  352,  353,  354,  322,  323,  324,   -1,   -1,
   -1,  328,   -1,   -1,   -1,  332,   -1,  367,  368,  369,
  370,  371,  372,  373,  374,  375,  376,  377,  378,  257,
  258,  348,  260,  350,  351,  352,  353,  354,   -1,  356,
  357,   -1,   -1,   -1,  361,  310,   -1,   -1,   -1,   -1,
  367,  368,  369,  370,  371,  372,  373,  374,  375,  376,
  377,  378,   -1,  380,  292,  293,  294,  295,   -1,   -1,
  298,  299,   -1,  301,  302,  303,   -1,   -1,  306,   -1,
   -1,  309,  310,   -1,   -1,  350,  351,  352,  353,  354,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  328,   -1,  367,  368,  369,  370,  371,  372,  373,  374,
  375,  376,  377,  378,   -1,   -1,   -1,   -1,  257,  258,
  348,  260,  350,  351,  352,  353,  354,   -1,   -1,  357,
   -1,   -1,   -1,  361,  310,   -1,   -1,   -1,   -1,  367,
  368,  369,  370,  371,  372,  373,  374,  375,  376,  377,
  378,   -1,  380,  292,  293,  294,  295,   -1,   -1,  298,
  299,   -1,  301,  302,  303,   -1,   -1,  306,   -1,   -1,
  309,  310,   -1,   -1,  350,  351,  352,  353,  354,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  328,
   -1,  367,  368,  369,  370,  371,  372,  373,  374,  375,
  376,  377,  378,   -1,   -1,   -1,   -1,  257,  258,  348,
  260,  350,  351,  352,  353,  354,   -1,   -1,  357,   -1,
   -1,   -1,  361,  310,   -1,   -1,   -1,   -1,  367,  368,
  369,  370,  371,  372,  373,  374,  375,  376,  377,  378,
   -1,  380,  292,  293,  294,  295,   -1,   -1,  298,  299,
   -1,   -1,   -1,   -1,   -1,   -1,  306,   -1,   -1,  309,
  310,   -1,   -1,  350,  351,  352,  353,  354,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  328,   -1,
  367,  368,  369,  370,  371,  372,  373,  374,  375,  376,
  377,  378,   -1,   -1,   -1,   -1,  257,  258,  348,  260,
  350,  351,  352,  353,  354,   -1,   -1,  357,   -1,   -1,
   -1,  361,  310,   -1,   -1,   -1,   -1,  367,  368,  369,
  370,  371,  372,  373,  374,  375,  376,  377,  378,   -1,
  380,  292,  293,  294,  295,   -1,   -1,  298,  299,   -1,
   -1,   -1,   -1,   -1,   -1,  306,   -1,   -1,  309,  310,
   -1,   -1,  350,  351,  352,  353,  354,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  328,   -1,  367,
  368,  369,  370,  371,  372,  373,  374,  375,  376,  377,
  378,   -1,   -1,   -1,   -1,  257,  258,  348,  260,  350,
  351,  352,  353,  354,   -1,   -1,  357,   -1,   -1,   -1,
  361,   -1,  257,  258,   -1,  260,  367,  368,  369,  370,
  371,  372,  373,  374,  375,  376,  377,  378,   -1,  380,
  292,  293,  294,  295,   -1,   -1,  298,  299,   -1,   -1,
   -1,   -1,   -1,   -1,  306,   -1,   -1,  309,  293,  294,
  295,   -1,   -1,  298,  299,   -1,  301,  302,  303,   -1,
   -1,  306,  293,  294,  295,   -1,  328,  298,  299,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  309,  310,
   -1,   -1,   -1,  328,   -1,   -1,  348,  332,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  357,   -1,  328,   -1,  361,
   -1,   -1,  257,  348,   -1,   -1,   -1,   -1,   -1,  371,
   -1,  356,   -1,   -1,   -1,   -1,   -1,  348,  380,  274,
  275,  276,  277,  278,  279,  280,  281,  282,  283,  284,
  285,  286,  287,  288,  289,  290,  291,  292,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  300,  301,  302,  303,  304,
  305,   -1,   -1,   -1,  309,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  257,   -1,  322,  323,  324,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  274,  275,  276,  277,  278,  279,  280,  281,
  282,  283,  284,  285,  286,  287,  288,  289,  290,  291,
  292,   -1,  357,   -1,   -1,  360,  361,   -1,  300,  301,
  302,  303,  304,  305,   -1,   -1,  371,  309,   -1,   -1,
   -1,  376,   -1,   -1,   -1,   -1,   -1,  257,   -1,   -1,
  322,  323,  324,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  274,  275,  276,  277,  278,  279,
  280,  281,  282,  283,  284,  285,  286,  287,  288,  289,
  290,  291,   -1,   -1,   -1,  357,   -1,   -1,  360,  361,
  300,  301,  302,  303,  304,  305,   -1,  256,  257,  371,
   -1,   -1,   -1,   -1,  376,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  322,  323,  324,  274,  275,  276,  277,  278,
  279,  280,  281,  282,  283,  284,  285,  286,  287,  288,
  289,  290,  291,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  300,  301,  302,  303,  304,  305,  357,   -1,   -1,
  360,  361,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  256,
  257,  371,   -1,  322,  323,  324,  376,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  274,  275,  276,
  277,  278,  279,  280,  281,  282,  283,  284,  285,  286,
  287,  288,  289,  290,  291,   -1,   -1,   -1,  357,   -1,
   -1,  360,  361,  300,  301,  302,  303,  304,  305,   -1,
   -1,  257,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  322,  323,  324,  274,  275,
  276,  277,  278,  279,  280,  281,  282,  283,  284,  285,
  286,  287,  288,  289,  290,  291,   -1,  350,  351,  352,
  353,   -1,   -1,  299,   -1,  301,  302,  303,  304,  305,
  357,   -1,  257,  360,  361,  368,  369,  370,  371,  372,
  373,  374,  375,  376,  377,  378,  322,  323,  324,  274,
  275,  276,  277,  278,  279,  280,  281,  282,  283,  284,
  285,  286,  287,  288,  289,  290,  291,   -1,  350,  351,
  352,  353,   -1,   -1,  299,   -1,  301,  302,  303,  304,
  305,  357,  256,  257,   -1,  361,  362,  369,  370,  371,
  372,  373,  374,  375,  376,  377,  378,  322,  323,  324,
  274,  275,  276,  277,  278,  279,  280,  281,  282,  283,
  284,  285,  286,  287,  288,  289,  290,  291,  350,  351,
  352,  353,   -1,   -1,   -1,   -1,   -1,  301,  302,  303,
  304,  305,  357,   -1,  257,   -1,  361,  362,  370,  371,
  372,  373,  374,  375,  376,  377,  378,   -1,  322,  323,
  324,  274,  275,  276,  277,  278,  279,  280,  281,  282,
  283,  284,  285,  286,  287,  288,  289,  290,  291,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  301,  302,
  303,  304,  305,  357,   -1,  257,   -1,  361,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  322,
  323,  324,  274,  275,  276,  277,  278,  279,  280,  281,
  282,  283,  284,  285,  286,  287,  288,  289,  290,  291,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  301,
  302,  303,  304,  305,  357,   -1,  257,   -1,  361,  362,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  322,  323,  324,  274,  275,  276,  277,  278,  279,  280,
  281,  282,  283,  284,  285,  286,  287,  288,  289,  290,
  291,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  301,  302,  303,  304,  305,  357,   -1,  257,   -1,  361,
  362,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  322,  323,  324,  274,  275,  276,  277,  278,  279,
  280,  281,  282,  283,  284,  285,  286,  287,  288,  289,
  290,  291,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  301,  302,  303,  304,  305,  357,   -1,  257,   -1,
  361,  362,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  322,  323,  324,  274,  275,  276,  277,  278,
  279,  280,  281,  282,  283,  284,  285,  286,  287,  288,
  289,  290,  291,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  301,  302,  303,  304,  305,  357,   -1,  257,
   -1,  361,  362,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,  322,  323,  324,  274,  275,  276,  277,
  278,  279,  280,  281,  282,  283,  284,  285,  286,  287,
  288,  289,  290,  291,   -1,   -1,   -1,   -1,   -1,   -1,
  258,   -1,  260,  301,  302,  303,  304,  305,  357,   -1,
   -1,   -1,  361,  362,  256,   -1,  258,   -1,   -1,  261,
  262,  263,   -1,   -1,  322,  323,  324,  293,  294,  295,
   -1,  273,  298,  299,  292,  293,  294,  295,   -1,   -1,
  298,  299,   -1,  309,  310,   -1,   -1,   -1,  306,   -1,
   -1,  309,   -1,  295,  296,   -1,   -1,  299,   -1,  357,
   -1,   -1,  328,  361,  362,   -1,  308,   -1,   -1,   -1,
  328,   -1,  314,  315,  316,  317,  318,  319,  320,  321,
   -1,   -1,  348,   -1,   -1,   -1,   -1,  329,  330,  331,
  348,  333,  334,  335,  336,  337,  338,  339,  340,  341,
  342,  343,  344,  345,  346,  256,   -1,  258,  257,   -1,
  261,  262,  263,  355,   -1,   -1,  265,   -1,  360,   -1,
   -1,   -1,  273,   -1,   -1,  274,  275,  276,  277,  278,
  279,  280,  281,  282,  283,  284,  285,  286,  287,  288,
  289,  290,  291,   -1,  295,  296,   -1,   -1,  299,   -1,
   -1,  300,  301,  302,  303,  304,  305,  308,   -1,   -1,
   -1,   -1,   -1,  314,  315,  316,  317,  318,  319,  320,
  321,   -1,   -1,  322,  323,  324,   -1,   -1,  329,  330,
  331,   -1,  333,  334,  335,  336,  337,  338,  339,  340,
  341,  342,  343,  344,  345,  346,  256,   -1,  258,  257,
   -1,  261,  262,  263,  355,   -1,   -1,   -1,  357,  360,
   -1,  360,  361,  273,   -1,   -1,  274,  275,  276,  277,
  278,  279,  280,  281,  282,  283,  284,  285,  286,  287,
  288,  289,  290,  291,   -1,  295,  296,   -1,   -1,  299,
   -1,   -1,  300,  301,  302,  303,  304,  305,  308,   -1,
   -1,   -1,   -1,   -1,  314,  315,  316,  317,  318,  319,
  320,  321,   -1,   -1,  322,  323,  324,   -1,   -1,  329,
  330,  331,   -1,  333,  334,  335,  336,  337,  338,  339,
  340,  341,  342,  343,  344,  345,  346,  256,   -1,  258,
  257,   -1,  261,  262,  263,  355,   -1,   -1,   -1,  357,
  360,   -1,  360,  361,  273,   -1,   -1,  274,  275,  276,
  277,  278,  279,  280,  281,  282,  283,  284,  285,  286,
  287,  288,  289,  290,  291,  292,  295,  296,   -1,   -1,
   -1,   -1,   -1,   -1,  301,  302,  303,  304,  305,  308,
   -1,   -1,   -1,  312,   -1,  314,  315,  316,  317,  318,
  319,  320,  321,   -1,   -1,  322,  323,  324,   -1,   -1,
  329,  330,  331,   -1,  333,  334,  335,  336,  337,  338,
  339,  340,  341,  342,  343,  344,  345,  346,  256,   -1,
  258,  257,   -1,  261,  262,  263,  355,   -1,   -1,   -1,
  357,  360,   -1,   -1,  361,  273,   -1,   -1,  274,  275,
  276,  277,  278,  279,  280,  281,  282,  283,  284,  285,
  286,  287,  288,  289,  290,  291,   -1,  295,  296,   -1,
   -1,   -1,   -1,   -1,   -1,  301,  302,  303,  304,  305,
  308,   -1,   -1,   -1,   -1,   -1,  314,  315,  316,  317,
  318,  319,  320,  321,   -1,   -1,  322,  323,  324,   -1,
   -1,  329,  330,  331,   -1,  333,  334,  335,  336,  337,
  338,  339,  340,  341,  342,  343,  344,  345,  346,  257,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  355,   -1,   -1,
   -1,  357,  360,  328,   -1,  361,  274,  275,  276,  277,
  278,  279,  280,  281,  282,  283,  284,  285,  286,  287,
  288,  289,  290,  291,   -1,  350,  351,  352,  353,  354,
   -1,   -1,   -1,  301,  302,  303,  304,  305,   -1,   -1,
  257,   -1,  367,  368,  369,  370,  371,  372,  373,  374,
  375,  376,  377,  378,  322,  323,  324,  274,  275,  276,
  277,  278,  279,  280,  281,  282,  283,  284,  285,  286,
  287,  288,  289,  290,  291,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  301,  302,  303,  304,  305,  357,
   -1,  257,   -1,  361,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  322,  323,  324,  274,  275,
  276,  277,  278,  279,  280,  281,  282,  283,  284,  285,
  286,  287,  288,  289,  290,  291,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  301,  302,  303,  304,  305,
  357,   -1,  257,   -1,  361,   -1,  258,   -1,  260,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  322,  323,  324,  274,
  275,  276,  277,  278,  279,  280,  281,  282,  283,  284,
  285,  286,  287,  288,  289,  290,  291,  257,  258,   -1,
  260,  293,  294,  295,   -1,   -1,  298,  299,   -1,  304,
  305,  357,   -1,   -1,  306,  361,   -1,  258,   -1,  260,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  322,  323,  324,
   -1,   -1,   -1,  293,  294,  295,  328,   -1,  298,  299,
   -1,  301,  302,  303,   -1,   -1,  306,   -1,  257,  258,
   -1,  260,  293,  294,  295,   -1,  348,  298,  299,   -1,
   -1,   -1,  357,   -1,   -1,  306,  361,   -1,  328,   -1,
   -1,   -1,  332,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  257,  258,   -1,  260,  293,  294,  295,  328,  348,  298,
  299,   -1,  301,  302,  303,   -1,  356,  306,   -1,  257,
  258,   -1,  260,   -1,   -1,   -1,   -1,  348,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  293,  294,  295,  328,
   -1,  298,  299,  332,  301,  302,  303,   -1,   -1,  306,
   -1,  257,  258,   -1,  260,  293,  294,  295,   -1,  348,
  298,  299,   -1,  301,  302,  303,   -1,  356,  306,   -1,
   -1,  328,   -1,   -1,   -1,  332,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,  257,  258,   -1,  260,  293,  294,  295,
  328,  348,  298,  299,  332,  301,  302,  303,   -1,  356,
  306,   -1,  257,  258,   -1,  260,   -1,   -1,   -1,   -1,
  348,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  356,  293,
  294,  295,  328,   -1,  298,  299,  332,  301,  302,  303,
   -1,   -1,  306,   -1,  257,  258,   -1,  260,  293,  294,
  295,   -1,  348,  298,  299,   -1,  301,  302,  303,   -1,
  356,  306,   -1,   -1,  328,   -1,   -1,   -1,  332,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  292,
   -1,  294,  295,  328,  348,  298,   -1,  332,  301,  302,
  303,   -1,  356,  306,   -1,   -1,  309,   -1,   -1,   -1,
   -1,  257,  258,  348,  260,   -1,   -1,   -1,   -1,   -1,
   -1,  356,   -1,   -1,   -1,  328,   -1,   -1,   -1,  332,
  258,   -1,  260,   -1,   -1,   -1,   -1,   -1,   -1,  258,
   -1,  260,   -1,   -1,   -1,   -1,  292,   -1,  294,  295,
   -1,   -1,  298,  356,   -1,  301,  302,  303,   -1,   -1,
  306,   -1,   -1,  309,   -1,  293,  294,  295,   -1,   -1,
  298,  299,   -1,   -1,  293,  294,  295,   -1,  306,  298,
  299,   -1,  328,   -1,   -1,   -1,  332,  306,   -1,   -1,
  258,   -1,  260,   -1,   -1,   -1,   -1,   -1,   -1,  258,
  328,  260,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  328,
  356,   -1,   -1,  258,   -1,  260,   -1,   -1,   -1,   -1,
  348,   -1,  258,   -1,  260,  293,  294,  295,   -1,  348,
  298,  299,   -1,   -1,  293,  294,  295,   -1,  306,  298,
  299,   -1,   -1,   -1,   -1,   -1,   -1,  306,  293,  294,
  295,   -1,   -1,  298,  299,   -1,   -1,  293,  294,  295,
  328,  306,  298,  299,   -1,  258,   -1,  260,   -1,  328,
  306,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  348,   -1,   -1,  328,   -1,   -1,   -1,   -1,   -1,  348,
   -1,   -1,  328,  293,  294,  295,   -1,   -1,  298,  299,
  293,  294,  295,  348,   -1,  298,  299,   -1,   -1,  309,
  310,   -1,  348,  306,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  293,  294,  295,   -1,  328,  298,
  299,   -1,   -1,   -1,   -1,  328,   -1,   -1,   -1,   -1,
  309,  310,   -1,   -1,   -1,   -1,   -1,   -1,  348,   -1,
  350,  351,  352,  353,  354,  348,   -1,   -1,   -1,  328,
   -1,   -1,   -1,  294,   -1,   -1,   -1,  367,  368,  369,
  370,  371,  372,  373,  374,  375,  376,  377,  378,  348,
   -1,  350,  351,  352,  353,  354,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  367,  368,
  369,  370,  371,  372,  373,  374,  375,  376,  377,  378,
  293,  294,  295,   -1,   -1,  298,  299,  348,   -1,  350,
  351,  352,  353,  354,   -1,   -1,  309,  310,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  367,  368,  369,  370,
   -1,  372,  373,  374,  375,  328,  377,  378,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  293,  294,  295,   -1,
   -1,  298,  299,   -1,   -1,  348,   -1,  350,  351,  352,
  353,  354,  309,  310,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  367,  368,  369,  370,   -1,  372,
  373,  328,   -1,   -1,  377,  378,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  293,  294,  295,   -1,   -1,  298,
  299,  348,   -1,  350,  351,  352,  353,  354,   -1,   -1,
  309,  310,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  367,  368,  369,  370,  371,  372,  373,  374,  375,  328,
  293,  294,  295,   -1,   -1,  298,  299,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  309,  310,   -1,  348,
   -1,  350,  351,  352,  353,  354,   -1,   -1,   -1,   -1,
   -1,  293,  294,  295,   -1,  328,  298,  299,  367,  368,
  369,  370,  371,  372,  373,  374,  375,  309,  310,   -1,
   -1,   -1,   -1,   -1,   -1,  348,   -1,  350,  351,  352,
  353,  354,   -1,   -1,   -1,   -1,  328,  293,  294,  295,
   -1,   -1,  298,  299,  367,  368,  369,  370,  371,  372,
  373,   -1,   -1,  309,  310,   -1,  348,   -1,  350,  351,
  352,  353,  354,   -1,   -1,   -1,   -1,   -1,  293,  294,
  295,   -1,  328,  298,  299,  367,  368,  369,  370,  371,
  372,  373,   -1,   -1,  309,  310,   -1,   -1,   -1,   -1,
   -1,   -1,  348,   -1,  350,  351,  352,  353,  354,   -1,
  293,  294,  295,  328,   -1,  298,  299,   -1,   -1,   -1,
   -1,  367,  368,  369,  370,  371,  309,  310,   -1,   -1,
   -1,   -1,   -1,  348,   -1,  350,  351,  352,  353,  354,
   -1,  293,  294,  295,   -1,  328,  298,  299,   -1,   -1,
   -1,   -1,  367,  368,  369,  370,  371,  309,  310,   -1,
   -1,   -1,   -1,   -1,   -1,  348,   -1,   -1,   -1,  352,
  353,  354,   -1,  293,  294,  295,  328,   -1,  298,  299,
   -1,   -1,   -1,   -1,  367,  368,  369,  370,  371,  309,
  310,   -1,   -1,   -1,   -1,   -1,  348,   -1,   -1,   -1,
  352,  353,  354,   -1,  293,  294,  295,   -1,  328,  298,
  299,   -1,   -1,   -1,   -1,  367,  368,  369,  370,  371,
  309,  310,   -1,   -1,   -1,   -1,   -1,   -1,  348,   -1,
   -1,   -1,   -1,   -1,  354,   -1,  293,  294,  295,  328,
   -1,  298,  299,   -1,   -1,   -1,   -1,  367,  368,  369,
  370,  371,  309,  310,   -1,   -1,   -1,   -1,   -1,  348,
   -1,   -1,   -1,   -1,   -1,  354,   -1,  293,  294,  295,
   -1,  328,  298,  299,   -1,   -1,   -1,   -1,  367,  368,
  369,  370,  371,  309,  310,   -1,   -1,   -1,   -1,   -1,
   -1,  348,   -1,   -1,   -1,   -1,   -1,  354,  293,  294,
  295,   -1,  328,  298,  299,   -1,   -1,   -1,   -1,   -1,
  367,  368,  369,  370,  309,  310,   -1,   -1,   -1,   -1,
  310,   -1,  348,   -1,   -1,   -1,   -1,   -1,  354,   -1,
   -1,   -1,   -1,  328,  293,  294,  295,   -1,   -1,  298,
  299,  367,  368,  369,   -1,   -1,   -1,   -1,   -1,   -1,
  309,  310,   -1,  348,   -1,   -1,   -1,   -1,   -1,  354,
  350,  351,  352,  353,  354,   -1,   -1,   -1,   -1,  328,
   -1,   -1,  367,  368,   -1,   -1,   -1,  367,  368,  369,
  370,  371,  372,  373,  374,  375,  376,  377,  378,  348,
  293,  294,  295,   -1,   -1,  298,  299,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  309,  310,   -1,  350,
  351,  352,  353,  354,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  328,  367,  368,  369,  370,
  371,  372,  373,  374,  375,  376,  377,  378,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  348,  274,  275,  276,  277,
  278,  279,  280,  281,   -1,  283,   -1,  285,   -1,   -1,
  288,  289,  290,  291,
};
#define YYFINAL 4
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 382
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"ID","HBLOCK","POUND","STRING",
"INCLUDE","IMPORT","INSERT","CHARCONST","NUM_INT","NUM_FLOAT","NUM_UNSIGNED",
"NUM_LONG","NUM_ULONG","NUM_LONGLONG","NUM_ULONGLONG","NUM_BOOL","TYPEDEF",
"TYPE_INT","TYPE_UNSIGNED","TYPE_SHORT","TYPE_LONG","TYPE_FLOAT","TYPE_DOUBLE",
"TYPE_CHAR","TYPE_WCHAR","TYPE_VOID","TYPE_SIGNED","TYPE_BOOL","TYPE_COMPLEX",
"TYPE_TYPEDEF","TYPE_RAW","TYPE_NON_ISO_INT8","TYPE_NON_ISO_INT16",
"TYPE_NON_ISO_INT32","TYPE_NON_ISO_INT64","LPAREN","RPAREN","COMMA","SEMI",
"EXTERN","INIT","LBRACE","RBRACE","PERIOD","CONST_QUAL","VOLATILE","REGISTER",
"STRUCT","UNION","EQUAL","SIZEOF","MODULE","LBRACKET","RBRACKET","BEGINFILE",
"ENDOFFILE","ILLEGAL","CONSTANT","NAME","RENAME","NAMEWARN","EXTEND","PRAGMA",
"FEATURE","VARARGS","ENUM","CLASS","TYPENAME","PRIVATE","PUBLIC","PROTECTED",
"COLON","STATIC","VIRTUAL","FRIEND","THROW","CATCH","EXPLICIT","USING",
"NAMESPACE","NATIVE","INLINE","TYPEMAP","EXCEPT","ECHO","APPLY","CLEAR",
"SWIGTEMPLATE","FRAGMENT","WARN","LESSTHAN","GREATERTHAN","DELETE_KW",
"LESSTHANOREQUALTO","GREATERTHANOREQUALTO","EQUALTO","NOTEQUALTO",
"QUESTIONMARK","TYPES","PARMS","NONID","DSTAR","DCNOT","TEMPLATE","OPERATOR",
"COPERATOR","PARSETYPE","PARSEPARM","PARSEPARMS","CAST","LOR","LAND","OR","XOR",
"AND","LSHIFT","RSHIFT","PLUS","MINUS","STAR","SLASH","MODULO","UMINUS","NOT",
"LNOT","DCOLON",
};
char *yyrule[] = {
"$accept : program",
"program : interface",
"program : PARSETYPE parm SEMI",
"program : PARSETYPE error",
"program : PARSEPARM parm SEMI",
"program : PARSEPARM error",
"program : PARSEPARMS LPAREN parms RPAREN SEMI",
"program : PARSEPARMS error SEMI",
"interface : interface declaration",
"interface : empty",
"declaration : swig_directive",
"declaration : c_declaration",
"declaration : cpp_declaration",
"declaration : SEMI",
"declaration : error",
"declaration : c_constructor_decl",
"declaration : error COPERATOR",
"swig_directive : extend_directive",
"swig_directive : apply_directive",
"swig_directive : clear_directive",
"swig_directive : constant_directive",
"swig_directive : echo_directive",
"swig_directive : except_directive",
"swig_directive : fragment_directive",
"swig_directive : include_directive",
"swig_directive : inline_directive",
"swig_directive : insert_directive",
"swig_directive : module_directive",
"swig_directive : name_directive",
"swig_directive : native_directive",
"swig_directive : pragma_directive",
"swig_directive : rename_directive",
"swig_directive : feature_directive",
"swig_directive : varargs_directive",
"swig_directive : typemap_directive",
"swig_directive : types_directive",
"swig_directive : template_directive",
"swig_directive : warn_directive",
"$$1 :",
"extend_directive : EXTEND options idcolon LBRACE $$1 cpp_members RBRACE",
"apply_directive : APPLY typemap_parm LBRACE tm_list RBRACE",
"clear_directive : CLEAR tm_list SEMI",
"constant_directive : CONSTANT ID EQUAL definetype SEMI",
"constant_directive : CONSTANT type declarator def_args SEMI",
"constant_directive : CONSTANT error SEMI",
"echo_directive : ECHO HBLOCK",
"echo_directive : ECHO string",
"except_directive : EXCEPT LPAREN ID RPAREN LBRACE",
"except_directive : EXCEPT LBRACE",
"except_directive : EXCEPT LPAREN ID RPAREN SEMI",
"except_directive : EXCEPT SEMI",
"stringtype : string LBRACE parm RBRACE",
"fname : string",
"fname : stringtype",
"fragment_directive : FRAGMENT LPAREN fname COMMA kwargs RPAREN HBLOCK",
"fragment_directive : FRAGMENT LPAREN fname COMMA kwargs RPAREN LBRACE",
"fragment_directive : FRAGMENT LPAREN fname RPAREN SEMI",
"$$2 :",
"include_directive : includetype options string BEGINFILE $$2 interface ENDOFFILE",
"includetype : INCLUDE",
"includetype : IMPORT",
"inline_directive : INLINE HBLOCK",
"inline_directive : INLINE LBRACE",
"insert_directive : HBLOCK",
"insert_directive : INSERT LPAREN idstring RPAREN string",
"insert_directive : INSERT LPAREN idstring RPAREN HBLOCK",
"insert_directive : INSERT LPAREN idstring RPAREN LBRACE",
"module_directive : MODULE options idstring",
"name_directive : NAME LPAREN idstring RPAREN",
"name_directive : NAME LPAREN RPAREN",
"native_directive : NATIVE LPAREN ID RPAREN storage_class ID SEMI",
"native_directive : NATIVE LPAREN ID RPAREN storage_class type declarator SEMI",
"pragma_directive : PRAGMA pragma_lang ID EQUAL pragma_arg",
"pragma_directive : PRAGMA pragma_lang ID",
"pragma_arg : string",
"pragma_arg : HBLOCK",
"pragma_lang : LPAREN ID RPAREN",
"pragma_lang : empty",
"rename_directive : rename_namewarn declarator idstring SEMI",
"rename_directive : rename_namewarn LPAREN kwargs RPAREN declarator cpp_const SEMI",
"rename_directive : rename_namewarn LPAREN kwargs RPAREN string SEMI",
"rename_namewarn : RENAME",
"rename_namewarn : NAMEWARN",
"feature_directive : FEATURE LPAREN idstring RPAREN declarator cpp_const stringbracesemi",
"feature_directive : FEATURE LPAREN idstring COMMA stringnum RPAREN declarator cpp_const SEMI",
"feature_directive : FEATURE LPAREN idstring featattr RPAREN declarator cpp_const stringbracesemi",
"feature_directive : FEATURE LPAREN idstring COMMA stringnum featattr RPAREN declarator cpp_const SEMI",
"feature_directive : FEATURE LPAREN idstring RPAREN stringbracesemi",
"feature_directive : FEATURE LPAREN idstring COMMA stringnum RPAREN SEMI",
"feature_directive : FEATURE LPAREN idstring featattr RPAREN stringbracesemi",
"feature_directive : FEATURE LPAREN idstring COMMA stringnum featattr RPAREN SEMI",
"stringbracesemi : stringbrace",
"stringbracesemi : SEMI",
"stringbracesemi : PARMS LPAREN parms RPAREN SEMI",
"featattr : COMMA idstring EQUAL stringnum",
"featattr : COMMA idstring EQUAL stringnum featattr",
"varargs_directive : VARARGS LPAREN varargs_parms RPAREN declarator cpp_const SEMI",
"varargs_parms : parms",
"varargs_parms : NUM_INT COMMA parm",
"typemap_directive : TYPEMAP LPAREN typemap_type RPAREN tm_list stringbrace",
"typemap_directive : TYPEMAP LPAREN typemap_type RPAREN tm_list SEMI",
"typemap_directive : TYPEMAP LPAREN typemap_type RPAREN tm_list EQUAL typemap_parm SEMI",
"typemap_type : kwargs",
"tm_list : typemap_parm tm_tail",
"tm_tail : COMMA typemap_parm tm_tail",
"tm_tail : empty",
"typemap_parm : type typemap_parameter_declarator",
"typemap_parm : LPAREN parms RPAREN",
"typemap_parm : LPAREN parms RPAREN LPAREN parms RPAREN",
"types_directive : TYPES LPAREN parms RPAREN stringbracesemi",
"template_directive : SWIGTEMPLATE LPAREN idstringopt RPAREN idcolonnt LESSTHAN valparms GREATERTHAN SEMI",
"warn_directive : WARN string",
"c_declaration : c_decl",
"c_declaration : c_enum_decl",
"c_declaration : c_enum_forward_decl",
"$$3 :",
"c_declaration : EXTERN string LBRACE $$3 interface RBRACE",
"c_decl : storage_class type declarator initializer c_decl_tail",
"c_decl_tail : SEMI",
"c_decl_tail : COMMA declarator initializer c_decl_tail",
"c_decl_tail : LBRACE",
"initializer : def_args",
"initializer : type_qualifier def_args",
"initializer : THROW LPAREN parms RPAREN def_args",
"initializer : type_qualifier THROW LPAREN parms RPAREN def_args",
"c_enum_forward_decl : storage_class ENUM ID SEMI",
"c_enum_decl : storage_class ENUM ename LBRACE enumlist RBRACE SEMI",
"c_enum_decl : storage_class ENUM ename LBRACE enumlist RBRACE declarator initializer c_decl_tail",
"c_constructor_decl : storage_class type LPAREN parms RPAREN ctor_end",
"cpp_declaration : cpp_class_decl",
"cpp_declaration : cpp_forward_class_decl",
"cpp_declaration : cpp_template_decl",
"cpp_declaration : cpp_using_decl",
"cpp_declaration : cpp_namespace_decl",
"cpp_declaration : cpp_catch_decl",
"$$4 :",
"cpp_class_decl : storage_class cpptype idcolon inherit LBRACE $$4 cpp_members RBRACE cpp_opt_declarators",
"$$5 :",
"cpp_class_decl : storage_class cpptype LBRACE $$5 cpp_members RBRACE declarator initializer c_decl_tail",
"cpp_opt_declarators : SEMI",
"cpp_opt_declarators : declarator initializer c_decl_tail",
"cpp_forward_class_decl : storage_class cpptype idcolon SEMI",
"$$6 :",
"cpp_template_decl : TEMPLATE LESSTHAN template_parms GREATERTHAN $$6 cpp_temp_possible",
"cpp_template_decl : TEMPLATE cpptype idcolon",
"cpp_temp_possible : c_decl",
"cpp_temp_possible : cpp_class_decl",
"cpp_temp_possible : cpp_constructor_decl",
"cpp_temp_possible : cpp_template_decl",
"cpp_temp_possible : cpp_forward_class_decl",
"cpp_temp_possible : cpp_conversion_operator",
"template_parms : templateparameters",
"templateparameters : templateparameter templateparameterstail",
"templateparameters : empty",
"templateparameter : templcpptype",
"templateparameter : parm",
"templateparameterstail : COMMA templateparameter templateparameterstail",
"templateparameterstail : empty",
"cpp_using_decl : USING idcolon SEMI",
"cpp_using_decl : USING NAMESPACE idcolon SEMI",
"$$7 :",
"cpp_namespace_decl : NAMESPACE idcolon LBRACE $$7 interface RBRACE",
"$$8 :",
"cpp_namespace_decl : NAMESPACE LBRACE $$8 interface RBRACE",
"cpp_namespace_decl : NAMESPACE ID EQUAL idcolon SEMI",
"cpp_members : cpp_member cpp_members",
"$$9 :",
"cpp_members : EXTEND LBRACE $$9 cpp_members RBRACE cpp_members",
"cpp_members : include_directive",
"cpp_members : empty",
"$$10 :",
"cpp_members : error $$10 cpp_members",
"cpp_member : c_declaration",
"cpp_member : cpp_constructor_decl",
"cpp_member : cpp_destructor_decl",
"cpp_member : cpp_protection_decl",
"cpp_member : cpp_swig_directive",
"cpp_member : cpp_conversion_operator",
"cpp_member : cpp_forward_class_decl",
"cpp_member : cpp_nested",
"cpp_member : storage_class idcolon SEMI",
"cpp_member : cpp_using_decl",
"cpp_member : cpp_template_decl",
"cpp_member : cpp_catch_decl",
"cpp_member : template_directive",
"cpp_member : warn_directive",
"cpp_member : anonymous_bitfield",
"cpp_member : fragment_directive",
"cpp_member : types_directive",
"cpp_member : SEMI",
"cpp_constructor_decl : storage_class type LPAREN parms RPAREN ctor_end",
"cpp_destructor_decl : NOT idtemplate LPAREN parms RPAREN cpp_end",
"cpp_destructor_decl : VIRTUAL NOT idtemplate LPAREN parms RPAREN cpp_vend",
"cpp_conversion_operator : storage_class COPERATOR type pointer LPAREN parms RPAREN cpp_vend",
"cpp_conversion_operator : storage_class COPERATOR type AND LPAREN parms RPAREN cpp_vend",
"cpp_conversion_operator : storage_class COPERATOR type pointer AND LPAREN parms RPAREN cpp_vend",
"cpp_conversion_operator : storage_class COPERATOR type LPAREN parms RPAREN cpp_vend",
"cpp_catch_decl : CATCH LPAREN parms RPAREN LBRACE",
"cpp_protection_decl : PUBLIC COLON",
"cpp_protection_decl : PRIVATE COLON",
"cpp_protection_decl : PROTECTED COLON",
"$$11 :",
"cpp_nested : storage_class cpptype idcolon inherit LBRACE $$11 cpp_opt_declarators",
"$$12 :",
"cpp_nested : storage_class cpptype inherit LBRACE $$12 cpp_opt_declarators",
"cpp_swig_directive : pragma_directive",
"cpp_swig_directive : constant_directive",
"cpp_swig_directive : name_directive",
"cpp_swig_directive : rename_directive",
"cpp_swig_directive : feature_directive",
"cpp_swig_directive : varargs_directive",
"cpp_swig_directive : insert_directive",
"cpp_swig_directive : typemap_directive",
"cpp_swig_directive : apply_directive",
"cpp_swig_directive : clear_directive",
"cpp_swig_directive : echo_directive",
"cpp_end : cpp_const SEMI",
"cpp_end : cpp_const LBRACE",
"cpp_vend : cpp_const SEMI",
"cpp_vend : cpp_const EQUAL definetype SEMI",
"cpp_vend : cpp_const LBRACE",
"anonymous_bitfield : storage_class type COLON expr SEMI",
"storage_class : EXTERN",
"storage_class : EXTERN string",
"storage_class : STATIC",
"storage_class : TYPEDEF",
"storage_class : VIRTUAL",
"storage_class : FRIEND",
"storage_class : EXPLICIT",
"storage_class : empty",
"parms : rawparms",
"rawparms : parm ptail",
"rawparms : empty",
"ptail : COMMA parm ptail",
"ptail : empty",
"parm : rawtype parameter_declarator",
"parm : TEMPLATE LESSTHAN cpptype GREATERTHAN cpptype idcolon def_args",
"parm : PERIOD PERIOD PERIOD",
"valparms : rawvalparms",
"rawvalparms : valparm valptail",
"rawvalparms : empty",
"valptail : COMMA valparm valptail",
"valptail : empty",
"valparm : parm",
"valparm : valexpr",
"def_args : EQUAL definetype",
"def_args : EQUAL definetype LBRACKET expr RBRACKET",
"def_args : EQUAL LBRACE",
"def_args : COLON expr",
"def_args : empty",
"parameter_declarator : declarator def_args",
"parameter_declarator : abstract_declarator def_args",
"parameter_declarator : def_args",
"typemap_parameter_declarator : declarator",
"typemap_parameter_declarator : abstract_declarator",
"typemap_parameter_declarator : empty",
"declarator : pointer notso_direct_declarator",
"declarator : pointer AND notso_direct_declarator",
"declarator : direct_declarator",
"declarator : AND notso_direct_declarator",
"declarator : idcolon DSTAR notso_direct_declarator",
"declarator : pointer idcolon DSTAR notso_direct_declarator",
"declarator : pointer idcolon DSTAR AND notso_direct_declarator",
"declarator : idcolon DSTAR AND notso_direct_declarator",
"notso_direct_declarator : idcolon",
"notso_direct_declarator : NOT idcolon",
"notso_direct_declarator : LPAREN idcolon RPAREN",
"notso_direct_declarator : LPAREN pointer notso_direct_declarator RPAREN",
"notso_direct_declarator : LPAREN idcolon DSTAR notso_direct_declarator RPAREN",
"notso_direct_declarator : notso_direct_declarator LBRACKET RBRACKET",
"notso_direct_declarator : notso_direct_declarator LBRACKET expr RBRACKET",
"notso_direct_declarator : notso_direct_declarator LPAREN parms RPAREN",
"direct_declarator : idcolon",
"direct_declarator : NOT idcolon",
"direct_declarator : LPAREN pointer direct_declarator RPAREN",
"direct_declarator : LPAREN AND direct_declarator RPAREN",
"direct_declarator : LPAREN idcolon DSTAR direct_declarator RPAREN",
"direct_declarator : direct_declarator LBRACKET RBRACKET",
"direct_declarator : direct_declarator LBRACKET expr RBRACKET",
"direct_declarator : direct_declarator LPAREN parms RPAREN",
"abstract_declarator : pointer",
"abstract_declarator : pointer direct_abstract_declarator",
"abstract_declarator : pointer AND",
"abstract_declarator : pointer AND direct_abstract_declarator",
"abstract_declarator : direct_abstract_declarator",
"abstract_declarator : AND direct_abstract_declarator",
"abstract_declarator : AND",
"abstract_declarator : idcolon DSTAR",
"abstract_declarator : pointer idcolon DSTAR",
"abstract_declarator : pointer idcolon DSTAR direct_abstract_declarator",
"direct_abstract_declarator : direct_abstract_declarator LBRACKET RBRACKET",
"direct_abstract_declarator : direct_abstract_declarator LBRACKET expr RBRACKET",
"direct_abstract_declarator : LBRACKET RBRACKET",
"direct_abstract_declarator : LBRACKET expr RBRACKET",
"direct_abstract_declarator : LPAREN abstract_declarator RPAREN",
"direct_abstract_declarator : direct_abstract_declarator LPAREN parms RPAREN",
"direct_abstract_declarator : LPAREN parms RPAREN",
"pointer : STAR type_qualifier pointer",
"pointer : STAR pointer",
"pointer : STAR type_qualifier",
"pointer : STAR",
"type_qualifier : type_qualifier_raw",
"type_qualifier : type_qualifier_raw type_qualifier",
"type_qualifier_raw : CONST_QUAL",
"type_qualifier_raw : VOLATILE",
"type_qualifier_raw : REGISTER",
"type : rawtype",
"rawtype : type_qualifier type_right",
"rawtype : type_right",
"rawtype : type_right type_qualifier",
"rawtype : type_qualifier type_right type_qualifier",
"type_right : primitive_type",
"type_right : TYPE_BOOL",
"type_right : TYPE_VOID",
"type_right : TYPE_TYPEDEF template_decl",
"type_right : ENUM idcolon",
"type_right : TYPE_RAW",
"type_right : idcolon",
"type_right : cpptype idcolon",
"primitive_type : primitive_type_list",
"primitive_type_list : type_specifier",
"primitive_type_list : type_specifier primitive_type_list",
"type_specifier : TYPE_INT",
"type_specifier : TYPE_SHORT",
"type_specifier : TYPE_LONG",
"type_specifier : TYPE_CHAR",
"type_specifier : TYPE_WCHAR",
"type_specifier : TYPE_FLOAT",
"type_specifier : TYPE_DOUBLE",
"type_specifier : TYPE_SIGNED",
"type_specifier : TYPE_UNSIGNED",
"type_specifier : TYPE_COMPLEX",
"type_specifier : TYPE_NON_ISO_INT8",
"type_specifier : TYPE_NON_ISO_INT16",
"type_specifier : TYPE_NON_ISO_INT32",
"type_specifier : TYPE_NON_ISO_INT64",
"$$13 :",
"definetype : $$13 expr",
"ename : ID",
"ename : empty",
"enumlist : enumlist COMMA edecl",
"enumlist : edecl",
"edecl : ID",
"edecl : ID EQUAL etype",
"edecl : empty",
"etype : expr",
"expr : valexpr",
"expr : type",
"valexpr : exprnum",
"valexpr : string",
"valexpr : SIZEOF LPAREN type parameter_declarator RPAREN",
"valexpr : exprcompound",
"valexpr : CHARCONST",
"valexpr : LPAREN expr RPAREN",
"valexpr : LPAREN expr RPAREN expr",
"valexpr : LPAREN expr pointer RPAREN expr",
"valexpr : LPAREN expr AND RPAREN expr",
"valexpr : LPAREN expr pointer AND RPAREN expr",
"valexpr : AND expr",
"valexpr : STAR expr",
"exprnum : NUM_INT",
"exprnum : NUM_FLOAT",
"exprnum : NUM_UNSIGNED",
"exprnum : NUM_LONG",
"exprnum : NUM_ULONG",
"exprnum : NUM_LONGLONG",
"exprnum : NUM_ULONGLONG",
"exprnum : NUM_BOOL",
"exprcompound : expr PLUS expr",
"exprcompound : expr MINUS expr",
"exprcompound : expr STAR expr",
"exprcompound : expr SLASH expr",
"exprcompound : expr MODULO expr",
"exprcompound : expr AND expr",
"exprcompound : expr OR expr",
"exprcompound : expr XOR expr",
"exprcompound : expr LSHIFT expr",
"exprcompound : expr RSHIFT expr",
"exprcompound : expr LAND expr",
"exprcompound : expr LOR expr",
"exprcompound : expr EQUALTO expr",
"exprcompound : expr NOTEQUALTO expr",
"exprcompound : expr GREATERTHANOREQUALTO expr",
"exprcompound : expr LESSTHANOREQUALTO expr",
"exprcompound : expr QUESTIONMARK expr COLON expr",
"exprcompound : MINUS expr",
"exprcompound : PLUS expr",
"exprcompound : NOT expr",
"exprcompound : LNOT expr",
"exprcompound : type LPAREN",
"inherit : raw_inherit",
"$$14 :",
"raw_inherit : COLON $$14 base_list",
"raw_inherit : empty",
"base_list : base_specifier",
"base_list : base_list COMMA base_specifier",
"$$15 :",
"base_specifier : opt_virtual $$15 idcolon",
"$$16 :",
"base_specifier : opt_virtual access_specifier $$16 opt_virtual idcolon",
"access_specifier : PUBLIC",
"access_specifier : PRIVATE",
"access_specifier : PROTECTED",
"templcpptype : CLASS",
"templcpptype : TYPENAME",
"cpptype : templcpptype",
"cpptype : STRUCT",
"cpptype : UNION",
"opt_virtual : VIRTUAL",
"opt_virtual : empty",
"cpp_const : type_qualifier",
"cpp_const : THROW LPAREN parms RPAREN",
"cpp_const : type_qualifier THROW LPAREN parms RPAREN",
"cpp_const : empty",
"ctor_end : cpp_const ctor_initializer SEMI",
"ctor_end : cpp_const ctor_initializer LBRACE",
"ctor_end : LPAREN parms RPAREN SEMI",
"ctor_end : LPAREN parms RPAREN LBRACE",
"ctor_end : EQUAL definetype SEMI",
"ctor_initializer : COLON mem_initializer_list",
"ctor_initializer : empty",
"mem_initializer_list : mem_initializer",
"mem_initializer_list : mem_initializer_list COMMA mem_initializer",
"mem_initializer : idcolon LPAREN",
"template_decl : LESSTHAN valparms GREATERTHAN",
"template_decl : empty",
"idstring : ID",
"idstring : string",
"idstringopt : idstring",
"idstringopt : empty",
"idcolon : idtemplate idcolontail",
"idcolon : NONID DCOLON idtemplate idcolontail",
"idcolon : idtemplate",
"idcolon : NONID DCOLON idtemplate",
"idcolon : OPERATOR",
"idcolon : NONID DCOLON OPERATOR",
"idcolontail : DCOLON idtemplate idcolontail",
"idcolontail : DCOLON idtemplate",
"idcolontail : DCOLON OPERATOR",
"idcolontail : DCNOT idtemplate",
"idtemplate : ID template_decl",
"idcolonnt : ID idcolontailnt",
"idcolonnt : NONID DCOLON ID idcolontailnt",
"idcolonnt : ID",
"idcolonnt : NONID DCOLON ID",
"idcolonnt : OPERATOR",
"idcolonnt : NONID DCOLON OPERATOR",
"idcolontailnt : DCOLON ID idcolontailnt",
"idcolontailnt : DCOLON ID",
"idcolontailnt : DCOLON OPERATOR",
"idcolontailnt : DCNOT ID",
"string : string STRING",
"string : STRING",
"stringbrace : string",
"stringbrace : LBRACE",
"stringbrace : HBLOCK",
"options : LPAREN kwargs RPAREN",
"options : empty",
"kwargs : idstring EQUAL stringnum",
"kwargs : idstring EQUAL stringnum COMMA kwargs",
"kwargs : idstring",
"kwargs : idstring COMMA kwargs",
"kwargs : idstring EQUAL stringtype",
"kwargs : idstring EQUAL stringtype COMMA kwargs",
"stringnum : string",
"stringnum : exprnum",
"empty :",
};
#endif
#if YYDEBUG
#include <stdio.h>
#endif

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#endif
#endif

#define YYINITSTACKSIZE 500

int      yydebug;
int      yynerrs;
int      yyerrflag;
int      yychar;
short   *yyssp;
YYSTYPE *yyvsp;
YYSTYPE  yyval;
YYSTYPE  yylval;

/* variables for the parser stack */
static short   *yyss;
static short   *yysslim;
static YYSTYPE *yyvs;
static int      yystacksize;
#line 6295 "parser.y"

SwigType *Swig_cparse_type(String *s) {
   String *ns;
   ns = NewStringf("%s;",s);
   Seek(ns,0,SEEK_SET);
   scanner_file(ns);
   top = 0;
   scanner_next_token(PARSETYPE);
   yyparse();
   /*   Printf(stdout,"typeparse: '%s' ---> '%s'\n", s, top); */
   return top;
}


Parm *Swig_cparse_parm(String *s) {
   String *ns;
   ns = NewStringf("%s;",s);
   Seek(ns,0,SEEK_SET);
   scanner_file(ns);
   top = 0;
   scanner_next_token(PARSEPARM);
   yyparse();
   /*   Printf(stdout,"typeparse: '%s' ---> '%s'\n", s, top); */
   Delete(ns);
   return top;
}


ParmList *Swig_cparse_parms(String *s, Node *file_line_node) {
   String *ns;
   char *cs = Char(s);
   if (cs && cs[0] != '(') {
     ns = NewStringf("(%s);",s);
   } else {
     ns = NewStringf("%s;",s);
   }
   Setfile(ns, Getfile(file_line_node));
   Setline(ns, Getline(file_line_node));
   Seek(ns,0,SEEK_SET);
   scanner_file(ns);
   top = 0;
   scanner_next_token(PARSEPARMS);
   yyparse();
   /*   Printf(stdout,"typeparse: '%s' ---> '%s'\n", s, top); */
   return top;
}

#line 4228 "CParse/parser.c"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(void)
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = yyssp - yyss;
    newss = (yyss != 0)
          ? (short *)realloc(yyss, newsize * sizeof(*newss))
          : (short *)malloc(newsize * sizeof(*newss));
    if (newss == 0)
        return -1;

    yyss  = newss;
    yyssp = newss + i;
    newvs = (yyvs != 0)
          ? (YYSTYPE *)realloc(yyvs, newsize * sizeof(*newvs))
          : (YYSTYPE *)malloc(newsize * sizeof(*newvs));
    if (newvs == 0)
        return -1;

    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse(void)
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register const char *yys;

    if ((yys = getenv("YYDEBUG")) != 0)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;

    yyerror("syntax error");

#ifdef lint
    goto yyerrlab;
#endif

yyerrlab:
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym)
        yyval = yyvsp[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);
    switch (yyn)
    {
case 1:
#line 1806 "parser.y"
{
                   if (!classes) classes = NewHash();
		   Setattr(yyvsp[0].node,"classes",classes); 
		   Setattr(yyvsp[0].node,"name",ModuleName);
		   
		   if ((!module_node) && ModuleName) {
		     module_node = new_node("module");
		     Setattr(module_node,"name",ModuleName);
		   }
		   Setattr(yyvsp[0].node,"module",module_node);
		   check_extensions();
	           top = yyvsp[0].node;
               }
break;
case 2:
#line 1819 "parser.y"
{
                 top = Copy(Getattr(yyvsp[-1].p,"type"));
		 Delete(yyvsp[-1].p);
               }
break;
case 3:
#line 1823 "parser.y"
{
                 top = 0;
               }
break;
case 4:
#line 1826 "parser.y"
{
                 top = yyvsp[-1].p;
               }
break;
case 5:
#line 1829 "parser.y"
{
                 top = 0;
               }
break;
case 6:
#line 1832 "parser.y"
{
                 top = yyvsp[-2].pl;
               }
break;
case 7:
#line 1835 "parser.y"
{
                 top = 0;
               }
break;
case 8:
#line 1840 "parser.y"
{  
                   /* add declaration to end of linked list (the declaration isn't always a single declaration, sometimes it is a linked list itself) */
                   appendChild(yyvsp[-1].node,yyvsp[0].node);
                   yyval.node = yyvsp[-1].node;
               }
break;
case 9:
#line 1845 "parser.y"
{
                   yyval.node = new_node("top");
               }
break;
case 10:
#line 1850 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 11:
#line 1851 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 12:
#line 1852 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 13:
#line 1853 "parser.y"
{ yyval.node = 0; }
break;
case 14:
#line 1854 "parser.y"
{
                  yyval.node = 0;
		  Swig_error(cparse_file, cparse_line,"Syntax error in input(1).\n");
		  exit(1);
               }
break;
case 15:
#line 1860 "parser.y"
{ 
                  if (yyval.node) {
   		      add_symbols(yyval.node);
                  }
                  yyval.node = yyvsp[0].node; 
	       }
break;
case 16:
#line 1876 "parser.y"
{
                  yyval.node = 0;
                  skip_decl();
               }
break;
case 17:
#line 1886 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 18:
#line 1887 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 19:
#line 1888 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 20:
#line 1889 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 21:
#line 1890 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 22:
#line 1891 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 23:
#line 1892 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 24:
#line 1893 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 25:
#line 1894 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 26:
#line 1895 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 27:
#line 1896 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 28:
#line 1897 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 29:
#line 1898 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 30:
#line 1899 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 31:
#line 1900 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 32:
#line 1901 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 33:
#line 1902 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 34:
#line 1903 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 35:
#line 1904 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 36:
#line 1905 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 37:
#line 1906 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 38:
#line 1913 "parser.y"
{
               Node *cls;
	       String *clsname;
	       cplus_mode = CPLUS_PUBLIC;
	       if (!classes) classes = NewHash();
	       if (!extendhash) extendhash = NewHash();
	       clsname = make_class_name(yyvsp[-1].str);
	       cls = Getattr(classes,clsname);
	       if (!cls) {
		 /* No previous definition. Create a new scope */
		 Node *am = Getattr(extendhash,clsname);
		 if (!am) {
		   Swig_symbol_newscope();
		   Swig_symbol_setscopename(yyvsp[-1].str);
		   prev_symtab = 0;
		 } else {
		   prev_symtab = Swig_symbol_setscope(Getattr(am,"symtab"));
		 }
		 current_class = 0;
	       } else {
		 /* Previous class definition.  Use its symbol table */
		 prev_symtab = Swig_symbol_setscope(Getattr(cls,"symtab"));
		 current_class = cls;
		 extendmode = 1;
	       }
	       Classprefix = NewString(yyvsp[-1].str);
	       Namespaceprefix= Swig_symbol_qualifiedscopename(0);
	       Delete(clsname);
	     }
break;
case 39:
#line 1941 "parser.y"
{
               String *clsname;
	       extendmode = 0;
               yyval.node = new_node("extend");
	       Setattr(yyval.node,"symtab",Swig_symbol_popscope());
	       if (prev_symtab) {
		 Swig_symbol_setscope(prev_symtab);
	       }
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
               clsname = make_class_name(yyvsp[-4].str);
	       Setattr(yyval.node,"name",clsname);

	       /* Mark members as extend */

	       tag_nodes(yyvsp[-1].node,"feature:extend",(char*) "1");
	       if (current_class) {
		 /* We add the extension to the previously defined class */
		 appendChild(yyval.node,yyvsp[-1].node);
		 appendChild(current_class,yyval.node);
	       } else {
		 /* We store the extensions in the extensions hash */
		 Node *am = Getattr(extendhash,clsname);
		 if (am) {
		   /* Append the members to the previous extend methods */
		   appendChild(am,yyvsp[-1].node);
		 } else {
		   appendChild(yyval.node,yyvsp[-1].node);
		   Setattr(extendhash,clsname,yyval.node);
		 }
	       }
	       current_class = 0;
	       Delete(Classprefix);
	       Delete(clsname);
	       Classprefix = 0;
	       prev_symtab = 0;
	       yyval.node = 0;

	     }
break;
case 40:
#line 1985 "parser.y"
{
                    yyval.node = new_node("apply");
                    Setattr(yyval.node,"pattern",Getattr(yyvsp[-3].p,"pattern"));
		    appendChild(yyval.node,yyvsp[-1].p);
               }
break;
case 41:
#line 1995 "parser.y"
{
		 yyval.node = new_node("clear");
		 appendChild(yyval.node,yyvsp[-1].p);
               }
break;
case 42:
#line 2006 "parser.y"
{
		   if ((yyvsp[-1].dtype.type != T_ERROR) && (yyvsp[-1].dtype.type != T_SYMBOL)) {
		     SwigType *type = NewSwigType(yyvsp[-1].dtype.type);
		     yyval.node = new_node("constant");
		     Setattr(yyval.node,"name",yyvsp[-3].id);
		     Setattr(yyval.node,"type",type);
		     Setattr(yyval.node,"value",yyvsp[-1].dtype.val);
		     if (yyvsp[-1].dtype.rawval) Setattr(yyval.node,"rawval", yyvsp[-1].dtype.rawval);
		     Setattr(yyval.node,"storage","%constant");
		     SetFlag(yyval.node,"feature:immutable");
		     add_symbols(yyval.node);
		     Delete(type);
		   } else {
		     if (yyvsp[-1].dtype.type == T_ERROR) {
		       Swig_warning(WARN_PARSE_UNSUPPORTED_VALUE,cparse_file,cparse_line,"Unsupported constant value (ignored)\n");
		     }
		     yyval.node = 0;
		   }

	       }
break;
case 43:
#line 2027 "parser.y"
{
		 if ((yyvsp[-1].dtype.type != T_ERROR) && (yyvsp[-1].dtype.type != T_SYMBOL)) {
		   SwigType_push(yyvsp[-3].type,yyvsp[-2].decl.type);
		   /* Sneaky callback function trick */
		   if (SwigType_isfunction(yyvsp[-3].type)) {
		     SwigType_add_pointer(yyvsp[-3].type);
		   }
		   yyval.node = new_node("constant");
		   Setattr(yyval.node,"name",yyvsp[-2].decl.id);
		   Setattr(yyval.node,"type",yyvsp[-3].type);
		   Setattr(yyval.node,"value",yyvsp[-1].dtype.val);
		   if (yyvsp[-1].dtype.rawval) Setattr(yyval.node,"rawval", yyvsp[-1].dtype.rawval);
		   Setattr(yyval.node,"storage","%constant");
		   SetFlag(yyval.node,"feature:immutable");
		   add_symbols(yyval.node);
		 } else {
		     if (yyvsp[-1].dtype.type == T_ERROR) {
		       Swig_warning(WARN_PARSE_UNSUPPORTED_VALUE,cparse_file,cparse_line,"Unsupported constant value\n");
		     }
		   yyval.node = 0;
		 }
               }
break;
case 44:
#line 2049 "parser.y"
{
		 Swig_warning(WARN_PARSE_BAD_VALUE,cparse_file,cparse_line,"Bad constant value (ignored).\n");
		 yyval.node = 0;
	       }
break;
case 45:
#line 2060 "parser.y"
{
		 char temp[64];
		 Replace(yyvsp[0].str,"$file",cparse_file, DOH_REPLACE_ANY);
		 sprintf(temp,"%d", cparse_line);
		 Replace(yyvsp[0].str,"$line",temp,DOH_REPLACE_ANY);
		 Printf(stderr,"%s\n", yyvsp[0].str);
		 Delete(yyvsp[0].str);
                 yyval.node = 0;
	       }
break;
case 46:
#line 2069 "parser.y"
{
		 char temp[64];
		 String *s = NewString(yyvsp[0].id);
		 Replace(s,"$file",cparse_file, DOH_REPLACE_ANY);
		 sprintf(temp,"%d", cparse_line);
		 Replace(s,"$line",temp,DOH_REPLACE_ANY);
		 Printf(stderr,"%s\n", s);
		 Delete(s);
                 yyval.node = 0;
               }
break;
case 47:
#line 2088 "parser.y"
{
                    skip_balanced('{','}');
		    yyval.node = 0;
		    Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
	       }
break;
case 48:
#line 2094 "parser.y"
{
                    skip_balanced('{','}');
		    yyval.node = 0;
		    Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
               }
break;
case 49:
#line 2100 "parser.y"
{
		 yyval.node = 0;
		 Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
               }
break;
case 50:
#line 2105 "parser.y"
{
		 yyval.node = 0;
		 Swig_warning(WARN_DEPRECATED_EXCEPT,cparse_file, cparse_line, "%%except is deprecated.  Use %%exception instead.\n");
	       }
break;
case 51:
#line 2112 "parser.y"
{		 
                 yyval.node = NewHash();
                 Setattr(yyval.node,"value",yyvsp[-3].id);
		 Setattr(yyval.node,"type",Getattr(yyvsp[-1].p,"type"));
               }
break;
case 52:
#line 2119 "parser.y"
{
                 yyval.node = NewHash();
                 Setattr(yyval.node,"value",yyvsp[0].id);
              }
break;
case 53:
#line 2123 "parser.y"
{
                yyval.node = yyvsp[0].node;
              }
break;
case 54:
#line 2136 "parser.y"
{
                   Hash *p = yyvsp[-2].node;
		   yyval.node = new_node("fragment");
		   Setattr(yyval.node,"value",Getattr(yyvsp[-4].node,"value"));
		   Setattr(yyval.node,"type",Getattr(yyvsp[-4].node,"type"));
		   Setattr(yyval.node,"section",Getattr(p,"name"));
		   Setattr(yyval.node,"kwargs",nextSibling(p));
		   Setattr(yyval.node,"code",yyvsp[0].str);
                 }
break;
case 55:
#line 2145 "parser.y"
{
		   Hash *p = yyvsp[-2].node;
		   String *code;
                   skip_balanced('{','}');
		   yyval.node = new_node("fragment");
		   Setattr(yyval.node,"value",Getattr(yyvsp[-4].node,"value"));
		   Setattr(yyval.node,"type",Getattr(yyvsp[-4].node,"type"));
		   Setattr(yyval.node,"section",Getattr(p,"name"));
		   Setattr(yyval.node,"kwargs",nextSibling(p));
		   Delitem(scanner_ccode,0);
		   Delitem(scanner_ccode,DOH_END);
		   code = Copy(scanner_ccode);
		   Setattr(yyval.node,"code",code);
		   Delete(code);
                 }
break;
case 56:
#line 2160 "parser.y"
{
		   yyval.node = new_node("fragment");
		   Setattr(yyval.node,"value",Getattr(yyvsp[-2].node,"value"));
		   Setattr(yyval.node,"type",Getattr(yyvsp[-2].node,"type"));
		   Setattr(yyval.node,"emitonly","1");
		 }
break;
case 57:
#line 2173 "parser.y"
{
                     yyvsp[-3].loc.filename = Copy(cparse_file);
		     yyvsp[-3].loc.line = cparse_line;
		     scanner_set_location(NewString(yyvsp[-1].id),1);
                     if (yyvsp[-2].node) { 
		       String *maininput = Getattr(yyvsp[-2].node, "maininput");
		       if (maininput)
		         scanner_set_main_input_file(NewString(maininput));
		     }
               }
break;
case 58:
#line 2182 "parser.y"
{
                     String *mname = 0;
                     yyval.node = yyvsp[-1].node;
		     scanner_set_location(yyvsp[-6].loc.filename,yyvsp[-6].loc.line+1);
		     if (strcmp(yyvsp[-6].loc.type,"include") == 0) set_nodeType(yyval.node,"include");
		     if (strcmp(yyvsp[-6].loc.type,"import") == 0) {
		       mname = yyvsp[-5].node ? Getattr(yyvsp[-5].node,"module") : 0;
		       set_nodeType(yyval.node,"import");
		       if (import_mode) --import_mode;
		     }
		     
		     Setattr(yyval.node,"name",yyvsp[-4].id);
		     /* Search for the module (if any) */
		     {
			 Node *n = firstChild(yyval.node);
			 while (n) {
			     if (Strcmp(nodeType(n),"module") == 0) {
			         if (mname) {
				   Setattr(n,"name", mname);
				   mname = 0;
				 }
				 Setattr(yyval.node,"module",Getattr(n,"name"));
				 break;
			     }
			     n = nextSibling(n);
			 }
			 if (mname) {
			   /* There is no module node in the import
			      node, ie, you imported a .h file
			      directly.  We are forced then to create
			      a new import node with a module node.
			   */			      
			   Node *nint = new_node("import");
			   Node *mnode = new_node("module");
			   Setattr(mnode,"name", mname);
			   appendChild(nint,mnode);
			   Delete(mnode);
			   appendChild(nint,firstChild(yyval.node));
			   yyval.node = nint;
			   Setattr(yyval.node,"module",mname);
			 }
		     }
		     Setattr(yyval.node,"options",yyvsp[-5].node);
               }
break;
case 59:
#line 2228 "parser.y"
{ yyval.loc.type = (char *) "include"; }
break;
case 60:
#line 2229 "parser.y"
{ yyval.loc.type = (char *) "import"; ++import_mode;}
break;
case 61:
#line 2236 "parser.y"
{
                 String *cpps;
		 if (Namespaceprefix) {
		   Swig_error(cparse_file, cparse_start_line, "%%inline directive inside a namespace is disallowed.\n");
		   yyval.node = 0;
		 } else {
		   yyval.node = new_node("insert");
		   Setattr(yyval.node,"code",yyvsp[0].str);
		   /* Need to run through the preprocessor */
		   Seek(yyvsp[0].str,0,SEEK_SET);
		   Setline(yyvsp[0].str,cparse_start_line);
		   Setfile(yyvsp[0].str,cparse_file);
		   cpps = Preprocessor_parse(yyvsp[0].str);
		   start_inline(Char(cpps), cparse_start_line);
		   Delete(yyvsp[0].str);
		   Delete(cpps);
		 }
		 
	       }
break;
case 62:
#line 2255 "parser.y"
{
                 String *cpps;
		 int start_line = cparse_line;
		 skip_balanced('{','}');
		 if (Namespaceprefix) {
		   Swig_error(cparse_file, cparse_start_line, "%%inline directive inside a namespace is disallowed.\n");
		   
		   yyval.node = 0;
		 } else {
		   String *code;
                   yyval.node = new_node("insert");
		   Delitem(scanner_ccode,0);
		   Delitem(scanner_ccode,DOH_END);
		   code = Copy(scanner_ccode);
		   Setattr(yyval.node,"code", code);
		   Delete(code);		   
		   cpps=Copy(scanner_ccode);
		   start_inline(Char(cpps), start_line);
		   Delete(cpps);
		 }
               }
break;
case 63:
#line 2286 "parser.y"
{
                 yyval.node = new_node("insert");
		 Setattr(yyval.node,"code",yyvsp[0].str);
	       }
break;
case 64:
#line 2290 "parser.y"
{
		 String *code = NewStringEmpty();
		 yyval.node = new_node("insert");
		 Setattr(yyval.node,"section",yyvsp[-2].id);
		 Setattr(yyval.node,"code",code);
		 if (Swig_insert_file(yyvsp[0].id,code) < 0) {
		   Swig_error(cparse_file, cparse_line, "Couldn't find '%s'.\n", yyvsp[0].id);
		   yyval.node = 0;
		 } 
               }
break;
case 65:
#line 2300 "parser.y"
{
		 yyval.node = new_node("insert");
		 Setattr(yyval.node,"section",yyvsp[-2].id);
		 Setattr(yyval.node,"code",yyvsp[0].str);
               }
break;
case 66:
#line 2305 "parser.y"
{
		 String *code;
                 skip_balanced('{','}');
		 yyval.node = new_node("insert");
		 Setattr(yyval.node,"section",yyvsp[-2].id);
		 Delitem(scanner_ccode,0);
		 Delitem(scanner_ccode,DOH_END);
		 code = Copy(scanner_ccode);
		 Setattr(yyval.node,"code", code);
		 Delete(code);
	       }
break;
case 67:
#line 2323 "parser.y"
{
                 yyval.node = new_node("module");
		 if (yyvsp[-1].node) {
		   Setattr(yyval.node,"options",yyvsp[-1].node);
		   if (Getattr(yyvsp[-1].node,"directors")) {
		     Wrapper_director_mode_set(1);
		   } 
		   if (Getattr(yyvsp[-1].node,"dirprot")) {
		     Wrapper_director_protected_mode_set(1);
		   } 
		   if (Getattr(yyvsp[-1].node,"allprotected")) {
		     Wrapper_all_protected_mode_set(1);
		   } 
		   if (Getattr(yyvsp[-1].node,"templatereduce")) {
		     template_reduce = 1;
		   }
		   if (Getattr(yyvsp[-1].node,"notemplatereduce")) {
		     template_reduce = 0;
		   }
		 }
		 if (!ModuleName) ModuleName = NewString(yyvsp[0].id);
		 if (!import_mode) {
		   /* first module included, we apply global
		      ModuleName, which can be modify by -module */
		   String *mname = Copy(ModuleName);
		   Setattr(yyval.node,"name",mname);
		   Delete(mname);
		 } else { 
		   /* import mode, we just pass the idstring */
		   Setattr(yyval.node,"name",yyvsp[0].id);   
		 }		 
		 if (!module_node) module_node = yyval.node;
	       }
break;
case 68:
#line 2363 "parser.y"
{
                 Swig_warning(WARN_DEPRECATED_NAME,cparse_file,cparse_line, "%%name is deprecated.  Use %%rename instead.\n");
		 Delete(yyrename);
                 yyrename = NewString(yyvsp[-1].id);
		 yyval.node = 0;
               }
break;
case 69:
#line 2369 "parser.y"
{
		 Swig_warning(WARN_DEPRECATED_NAME,cparse_file,cparse_line, "%%name is deprecated.  Use %%rename instead.\n");
		 yyval.node = 0;
		 Swig_error(cparse_file,cparse_line,"Missing argument to %%name directive.\n");
	       }
break;
case 70:
#line 2382 "parser.y"
{
                 yyval.node = new_node("native");
		 Setattr(yyval.node,"name",yyvsp[-4].id);
		 Setattr(yyval.node,"wrap:name",yyvsp[-1].id);
	         add_symbols(yyval.node);
	       }
break;
case 71:
#line 2388 "parser.y"
{
		 if (!SwigType_isfunction(yyvsp[-1].decl.type)) {
		   Swig_error(cparse_file,cparse_line,"%%native declaration '%s' is not a function.\n", yyvsp[-1].decl.id);
		   yyval.node = 0;
		 } else {
		     Delete(SwigType_pop_function(yyvsp[-1].decl.type));
		     /* Need check for function here */
		     SwigType_push(yyvsp[-2].type,yyvsp[-1].decl.type);
		     yyval.node = new_node("native");
	             Setattr(yyval.node,"name",yyvsp[-5].id);
		     Setattr(yyval.node,"wrap:name",yyvsp[-1].decl.id);
		     Setattr(yyval.node,"type",yyvsp[-2].type);
		     Setattr(yyval.node,"parms",yyvsp[-1].decl.parms);
		     Setattr(yyval.node,"decl",yyvsp[-1].decl.type);
		 }
	         add_symbols(yyval.node);
	       }
break;
case 72:
#line 2414 "parser.y"
{
                 yyval.node = new_node("pragma");
		 Setattr(yyval.node,"lang",yyvsp[-3].id);
		 Setattr(yyval.node,"name",yyvsp[-2].id);
		 Setattr(yyval.node,"value",yyvsp[0].str);
	       }
break;
case 73:
#line 2420 "parser.y"
{
		yyval.node = new_node("pragma");
		Setattr(yyval.node,"lang",yyvsp[-1].id);
		Setattr(yyval.node,"name",yyvsp[0].id);
	      }
break;
case 74:
#line 2427 "parser.y"
{ yyval.str = NewString(yyvsp[0].id); }
break;
case 75:
#line 2428 "parser.y"
{ yyval.str = yyvsp[0].str; }
break;
case 76:
#line 2431 "parser.y"
{ yyval.id = yyvsp[-1].id; }
break;
case 77:
#line 2432 "parser.y"
{ yyval.id = (char *) "swig"; }
break;
case 78:
#line 2440 "parser.y"
{
                SwigType *t = yyvsp[-2].decl.type;
		Hash *kws = NewHash();
		String *fixname;
		fixname = feature_identifier_fix(yyvsp[-2].decl.id);
		Setattr(kws,"name",yyvsp[-1].id);
		if (!Len(t)) t = 0;
		/* Special declarator check */
		if (t) {
		  if (SwigType_isfunction(t)) {
		    SwigType *decl = SwigType_pop_function(t);
		    if (SwigType_ispointer(t)) {
		      String *nname = NewStringf("*%s",fixname);
		      if (yyvsp[-3].intvalue) {
			Swig_name_rename_add(Namespaceprefix, nname,decl,kws,yyvsp[-2].decl.parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,nname,decl,kws);
		      }
		      Delete(nname);
		    } else {
		      if (yyvsp[-3].intvalue) {
			Swig_name_rename_add(Namespaceprefix,(fixname),decl,kws,yyvsp[-2].decl.parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,(fixname),decl,kws);
		      }
		    }
		    Delete(decl);
		  } else if (SwigType_ispointer(t)) {
		    String *nname = NewStringf("*%s",fixname);
		    if (yyvsp[-3].intvalue) {
		      Swig_name_rename_add(Namespaceprefix,(nname),0,kws,yyvsp[-2].decl.parms);
		    } else {
		      Swig_name_namewarn_add(Namespaceprefix,(nname),0,kws);
		    }
		    Delete(nname);
		  }
		} else {
		  if (yyvsp[-3].intvalue) {
		    Swig_name_rename_add(Namespaceprefix,(fixname),0,kws,yyvsp[-2].decl.parms);
		  } else {
		    Swig_name_namewarn_add(Namespaceprefix,(fixname),0,kws);
		  }
		}
                yyval.node = 0;
		scanner_clear_rename();
              }
break;
case 79:
#line 2486 "parser.y"
{
		String *fixname;
		Hash *kws = yyvsp[-4].node;
		SwigType *t = yyvsp[-2].decl.type;
		fixname = feature_identifier_fix(yyvsp[-2].decl.id);
		if (!Len(t)) t = 0;
		/* Special declarator check */
		if (t) {
		  if (yyvsp[-1].dtype.qualifier) SwigType_push(t,yyvsp[-1].dtype.qualifier);
		  if (SwigType_isfunction(t)) {
		    SwigType *decl = SwigType_pop_function(t);
		    if (SwigType_ispointer(t)) {
		      String *nname = NewStringf("*%s",fixname);
		      if (yyvsp[-6].intvalue) {
			Swig_name_rename_add(Namespaceprefix, nname,decl,kws,yyvsp[-2].decl.parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,nname,decl,kws);
		      }
		      Delete(nname);
		    } else {
		      if (yyvsp[-6].intvalue) {
			Swig_name_rename_add(Namespaceprefix,(fixname),decl,kws,yyvsp[-2].decl.parms);
		      } else {
			Swig_name_namewarn_add(Namespaceprefix,(fixname),decl,kws);
		      }
		    }
		    Delete(decl);
		  } else if (SwigType_ispointer(t)) {
		    String *nname = NewStringf("*%s",fixname);
		    if (yyvsp[-6].intvalue) {
		      Swig_name_rename_add(Namespaceprefix,(nname),0,kws,yyvsp[-2].decl.parms);
		    } else {
		      Swig_name_namewarn_add(Namespaceprefix,(nname),0,kws);
		    }
		    Delete(nname);
		  }
		} else {
		  if (yyvsp[-6].intvalue) {
		    Swig_name_rename_add(Namespaceprefix,(fixname),0,kws,yyvsp[-2].decl.parms);
		  } else {
		    Swig_name_namewarn_add(Namespaceprefix,(fixname),0,kws);
		  }
		}
                yyval.node = 0;
		scanner_clear_rename();
              }
break;
case 80:
#line 2532 "parser.y"
{
		if (yyvsp[-5].intvalue) {
		  Swig_name_rename_add(Namespaceprefix,yyvsp[-1].id,0,yyvsp[-3].node,0);
		} else {
		  Swig_name_namewarn_add(Namespaceprefix,yyvsp[-1].id,0,yyvsp[-3].node);
		}
		yyval.node = 0;
		scanner_clear_rename();
              }
break;
case 81:
#line 2543 "parser.y"
{
		    yyval.intvalue = 1;
                }
break;
case 82:
#line 2546 "parser.y"
{
                    yyval.intvalue = 0;
                }
break;
case 83:
#line 2573 "parser.y"
{
                    String *val = yyvsp[0].str ? NewString(yyvsp[0].str) : NewString("1");
                    new_feature(yyvsp[-4].id, val, 0, yyvsp[-2].decl.id, yyvsp[-2].decl.type, yyvsp[-2].decl.parms, yyvsp[-1].dtype.qualifier);
                    yyval.node = 0;
                    scanner_clear_rename();
                  }
break;
case 84:
#line 2579 "parser.y"
{
                    String *val = Len(yyvsp[-4].id) ? NewString(yyvsp[-4].id) : 0;
                    new_feature(yyvsp[-6].id, val, 0, yyvsp[-2].decl.id, yyvsp[-2].decl.type, yyvsp[-2].decl.parms, yyvsp[-1].dtype.qualifier);
                    yyval.node = 0;
                    scanner_clear_rename();
                  }
break;
case 85:
#line 2585 "parser.y"
{
                    String *val = yyvsp[0].str ? NewString(yyvsp[0].str) : NewString("1");
                    new_feature(yyvsp[-5].id, val, yyvsp[-4].node, yyvsp[-2].decl.id, yyvsp[-2].decl.type, yyvsp[-2].decl.parms, yyvsp[-1].dtype.qualifier);
                    yyval.node = 0;
                    scanner_clear_rename();
                  }
break;
case 86:
#line 2591 "parser.y"
{
                    String *val = Len(yyvsp[-5].id) ? NewString(yyvsp[-5].id) : 0;
                    new_feature(yyvsp[-7].id, val, yyvsp[-4].node, yyvsp[-2].decl.id, yyvsp[-2].decl.type, yyvsp[-2].decl.parms, yyvsp[-1].dtype.qualifier);
                    yyval.node = 0;
                    scanner_clear_rename();
                  }
break;
case 87:
#line 2599 "parser.y"
{
                    String *val = yyvsp[0].str ? NewString(yyvsp[0].str) : NewString("1");
                    new_feature(yyvsp[-2].id, val, 0, 0, 0, 0, 0);
                    yyval.node = 0;
                    scanner_clear_rename();
                  }
break;
case 88:
#line 2605 "parser.y"
{
                    String *val = Len(yyvsp[-2].id) ? NewString(yyvsp[-2].id) : 0;
                    new_feature(yyvsp[-4].id, val, 0, 0, 0, 0, 0);
                    yyval.node = 0;
                    scanner_clear_rename();
                  }
break;
case 89:
#line 2611 "parser.y"
{
                    String *val = yyvsp[0].str ? NewString(yyvsp[0].str) : NewString("1");
                    new_feature(yyvsp[-3].id, val, yyvsp[-2].node, 0, 0, 0, 0);
                    yyval.node = 0;
                    scanner_clear_rename();
                  }
break;
case 90:
#line 2617 "parser.y"
{
                    String *val = Len(yyvsp[-3].id) ? NewString(yyvsp[-3].id) : 0;
                    new_feature(yyvsp[-5].id, val, yyvsp[-2].node, 0, 0, 0, 0);
                    yyval.node = 0;
                    scanner_clear_rename();
                  }
break;
case 91:
#line 2625 "parser.y"
{ yyval.str = yyvsp[0].str; }
break;
case 92:
#line 2626 "parser.y"
{ yyval.str = 0; }
break;
case 93:
#line 2627 "parser.y"
{ yyval.str = yyvsp[-2].pl; }
break;
case 94:
#line 2630 "parser.y"
{
		  yyval.node = NewHash();
		  Setattr(yyval.node,"name",yyvsp[-2].id);
		  Setattr(yyval.node,"value",yyvsp[0].id);
                }
break;
case 95:
#line 2635 "parser.y"
{
		  yyval.node = NewHash();
		  Setattr(yyval.node,"name",yyvsp[-3].id);
		  Setattr(yyval.node,"value",yyvsp[-1].id);
                  set_nextSibling(yyval.node,yyvsp[0].node);
                }
break;
case 96:
#line 2645 "parser.y"
{
                 Parm *val;
		 String *name;
		 SwigType *t;
		 if (Namespaceprefix) name = NewStringf("%s::%s", Namespaceprefix, yyvsp[-2].decl.id);
		 else name = NewString(yyvsp[-2].decl.id);
		 val = yyvsp[-4].pl;
		 if (yyvsp[-2].decl.parms) {
		   Setmeta(val,"parms",yyvsp[-2].decl.parms);
		 }
		 t = yyvsp[-2].decl.type;
		 if (!Len(t)) t = 0;
		 if (t) {
		   if (yyvsp[-1].dtype.qualifier) SwigType_push(t,yyvsp[-1].dtype.qualifier);
		   if (SwigType_isfunction(t)) {
		     SwigType *decl = SwigType_pop_function(t);
		     if (SwigType_ispointer(t)) {
		       String *nname = NewStringf("*%s",name);
		       Swig_feature_set(Swig_cparse_features(), nname, decl, "feature:varargs", val, 0);
		       Delete(nname);
		     } else {
		       Swig_feature_set(Swig_cparse_features(), name, decl, "feature:varargs", val, 0);
		     }
		     Delete(decl);
		   } else if (SwigType_ispointer(t)) {
		     String *nname = NewStringf("*%s",name);
		     Swig_feature_set(Swig_cparse_features(),nname,0,"feature:varargs",val, 0);
		     Delete(nname);
		   }
		 } else {
		   Swig_feature_set(Swig_cparse_features(),name,0,"feature:varargs",val, 0);
		 }
		 Delete(name);
		 yyval.node = 0;
              }
break;
case 97:
#line 2681 "parser.y"
{ yyval.pl = yyvsp[0].pl; }
break;
case 98:
#line 2682 "parser.y"
{ 
		  int i;
		  int n;
		  Parm *p;
		  n = atoi(Char(yyvsp[-2].dtype.val));
		  if (n <= 0) {
		    Swig_error(cparse_file, cparse_line,"Argument count in %%varargs must be positive.\n");
		    yyval.pl = 0;
		  } else {
		    String *name = Getattr(yyvsp[0].p, "name");
		    yyval.pl = Copy(yyvsp[0].p);
		    if (name)
		      Setattr(yyval.pl, "name", NewStringf("%s%d", name, n));
		    for (i = 1; i < n; i++) {
		      p = Copy(yyvsp[0].p);
		      name = Getattr(p, "name");
		      if (name)
		        Setattr(p, "name", NewStringf("%s%d", name, n-i));
		      set_nextSibling(p,yyval.pl);
		      Delete(yyval.pl);
		      yyval.pl = p;
		    }
		  }
                }
break;
case 99:
#line 2717 "parser.y"
{
		   yyval.node = 0;
		   if (yyvsp[-3].tmap.method) {
		     String *code = 0;
		     yyval.node = new_node("typemap");
		     Setattr(yyval.node,"method",yyvsp[-3].tmap.method);
		     if (yyvsp[-3].tmap.kwargs) {
		       ParmList *kw = yyvsp[-3].tmap.kwargs;
                       code = remove_block(kw, yyvsp[0].str);
		       Setattr(yyval.node,"kwargs", yyvsp[-3].tmap.kwargs);
		     }
		     code = code ? code : NewString(yyvsp[0].str);
		     Setattr(yyval.node,"code", code);
		     Delete(code);
		     appendChild(yyval.node,yyvsp[-1].p);
		   }
	       }
break;
case 100:
#line 2734 "parser.y"
{
		 yyval.node = 0;
		 if (yyvsp[-3].tmap.method) {
		   yyval.node = new_node("typemap");
		   Setattr(yyval.node,"method",yyvsp[-3].tmap.method);
		   appendChild(yyval.node,yyvsp[-1].p);
		 }
	       }
break;
case 101:
#line 2742 "parser.y"
{
		   yyval.node = 0;
		   if (yyvsp[-5].tmap.method) {
		     yyval.node = new_node("typemapcopy");
		     Setattr(yyval.node,"method",yyvsp[-5].tmap.method);
		     Setattr(yyval.node,"pattern", Getattr(yyvsp[-1].p,"pattern"));
		     appendChild(yyval.node,yyvsp[-3].p);
		   }
	       }
break;
case 102:
#line 2755 "parser.y"
{
		 Hash *p;
		 String *name;
		 p = nextSibling(yyvsp[0].node);
		 if (p && (!Getattr(p,"value"))) {
 		   /* this is the deprecated two argument typemap form */
 		   Swig_warning(WARN_DEPRECATED_TYPEMAP_LANG,cparse_file, cparse_line,
				"Specifying the language name in %%typemap is deprecated - use #ifdef SWIG<LANG> instead.\n");
		   /* two argument typemap form */
		   name = Getattr(yyvsp[0].node,"name");
		   if (!name || (Strcmp(name,typemap_lang))) {
		     yyval.tmap.method = 0;
		     yyval.tmap.kwargs = 0;
		   } else {
		     yyval.tmap.method = Getattr(p,"name");
		     yyval.tmap.kwargs = nextSibling(p);
		   }
		 } else {
		   /* one-argument typemap-form */
		   yyval.tmap.method = Getattr(yyvsp[0].node,"name");
		   yyval.tmap.kwargs = p;
		 }
                }
break;
case 103:
#line 2780 "parser.y"
{
                 yyval.p = yyvsp[-1].p;
		 set_nextSibling(yyval.p,yyvsp[0].p);
		}
break;
case 104:
#line 2786 "parser.y"
{
                 yyval.p = yyvsp[-1].p;
		 set_nextSibling(yyval.p,yyvsp[0].p);
                }
break;
case 105:
#line 2790 "parser.y"
{ yyval.p = 0;}
break;
case 106:
#line 2793 "parser.y"
{
                  Parm *parm;
		  SwigType_push(yyvsp[-1].type,yyvsp[0].decl.type);
		  yyval.p = new_node("typemapitem");
		  parm = NewParmWithoutFileLineInfo(yyvsp[-1].type,yyvsp[0].decl.id);
		  Setattr(yyval.p,"pattern",parm);
		  Setattr(yyval.p,"parms", yyvsp[0].decl.parms);
		  Delete(parm);
		  /*		  $$ = NewParmWithoutFileLineInfo($1,$2.id);
				  Setattr($$,"parms",$2.parms); */
                }
break;
case 107:
#line 2804 "parser.y"
{
                  yyval.p = new_node("typemapitem");
		  Setattr(yyval.p,"pattern",yyvsp[-1].pl);
		  /*		  Setattr($$,"multitype",$2); */
               }
break;
case 108:
#line 2809 "parser.y"
{
		 yyval.p = new_node("typemapitem");
		 Setattr(yyval.p,"pattern", yyvsp[-4].pl);
		 /*                 Setattr($$,"multitype",$2); */
		 Setattr(yyval.p,"parms",yyvsp[-1].pl);
               }
break;
case 109:
#line 2822 "parser.y"
{
                   yyval.node = new_node("types");
		   Setattr(yyval.node,"parms",yyvsp[-2].pl);
                   if (yyvsp[0].str)
		     Setattr(yyval.node,"convcode",NewString(yyvsp[0].str));
               }
break;
case 110:
#line 2834 "parser.y"
{
                  Parm *p, *tp;
		  Node *n;
		  Symtab *tscope = 0;
		  int     specialized = 0;

		  yyval.node = 0;

		  tscope = Swig_symbol_current();          /* Get the current scope */

		  /* If the class name is qualified, we need to create or lookup namespace entries */
		  if (!inclass) {
		    yyvsp[-4].str = resolve_create_node_scope(yyvsp[-4].str);
		  }

		  /*
		    We use the new namespace entry 'nscope' only to
		    emit the template node. The template parameters are
		    resolved in the current 'tscope'.

		    This is closer to the C++ (typedef) behavior.
		  */
		  n = Swig_cparse_template_locate(yyvsp[-4].str,yyvsp[-2].p,tscope);

		  /* Patch the argument types to respect namespaces */
		  p = yyvsp[-2].p;
		  while (p) {
		    SwigType *value = Getattr(p,"value");
		    if (!value) {
		      SwigType *ty = Getattr(p,"type");
		      if (ty) {
			SwigType *rty = 0;
			int reduce = template_reduce;
			if (reduce || !SwigType_ispointer(ty)) {
			  rty = Swig_symbol_typedef_reduce(ty,tscope);
			  if (!reduce) reduce = SwigType_ispointer(rty);
			}
			ty = reduce ? Swig_symbol_type_qualify(rty,tscope) : Swig_symbol_type_qualify(ty,tscope);
			Setattr(p,"type",ty);
			Delete(ty);
			Delete(rty);
		      }
		    } else {
		      value = Swig_symbol_type_qualify(value,tscope);
		      Setattr(p,"value",value);
		      Delete(value);
		    }

		    p = nextSibling(p);
		  }

		  /* Look for the template */
		  {
                    Node *nn = n;
                    Node *linklistend = 0;
                    while (nn) {
                      Node *templnode = 0;
                      if (Strcmp(nodeType(nn),"template") == 0) {
                        int nnisclass = (Strcmp(Getattr(nn,"templatetype"),"class") == 0); /* if not a templated class it is a templated function */
                        Parm *tparms = Getattr(nn,"templateparms");
                        if (!tparms) {
                          specialized = 1;
                        }
                        if (nnisclass && !specialized && ((ParmList_len(yyvsp[-2].p) > ParmList_len(tparms)))) {
                          Swig_error(cparse_file, cparse_line, "Too many template parameters. Maximum of %d.\n", ParmList_len(tparms));
                        } else if (nnisclass && !specialized && ((ParmList_len(yyvsp[-2].p) < ParmList_numrequired(tparms)))) {
                          Swig_error(cparse_file, cparse_line, "Not enough template parameters specified. %d required.\n", ParmList_numrequired(tparms));
                        } else if (!nnisclass && ((ParmList_len(yyvsp[-2].p) != ParmList_len(tparms)))) {
                          /* must be an overloaded templated method - ignore it as it is overloaded with a different number of template parameters */
                          nn = Getattr(nn,"sym:nextSibling"); /* repeat for overloaded templated functions */
                          continue;
                        } else {
			  String *tname = Copy(yyvsp[-4].str);
                          int def_supplied = 0;
                          /* Expand the template */
			  Node *templ = Swig_symbol_clookup(yyvsp[-4].str,0);
			  Parm *targs = templ ? Getattr(templ,"templateparms") : 0;

                          ParmList *temparms;
                          if (specialized) temparms = CopyParmList(yyvsp[-2].p);
                          else temparms = CopyParmList(tparms);

                          /* Create typedef's and arguments */
                          p = yyvsp[-2].p;
                          tp = temparms;
                          if (!p && ParmList_len(p) != ParmList_len(temparms)) {
                            /* we have no template parameters supplied in %template for a template that has default args*/
                            p = tp;
                            def_supplied = 1;
                          }

                          while (p) {
                            String *value = Getattr(p,"value");
                            if (def_supplied) {
                              Setattr(p,"default","1");
                            }
                            if (value) {
                              Setattr(tp,"value",value);
                            } else {
                              SwigType *ty = Getattr(p,"type");
                              if (ty) {
                                Setattr(tp,"type",ty);
                              }
                              Delattr(tp,"value");
                            }
			    /* fix default arg values */
			    if (targs) {
			      Parm *pi = temparms;
			      Parm *ti = targs;
			      String *tv = Getattr(tp,"value");
			      if (!tv) tv = Getattr(tp,"type");
			      while(pi != tp && ti && pi) {
				String *name = Getattr(ti,"name");
				String *value = Getattr(pi,"value");
				if (!value) value = Getattr(pi,"type");
				Replaceid(tv, name, value);
				pi = nextSibling(pi);
				ti = nextSibling(ti);
			      }
			    }
                            p = nextSibling(p);
                            tp = nextSibling(tp);
                            if (!p && tp) {
                              p = tp;
                              def_supplied = 1;
                            }
                          }

                          templnode = copy_node(nn);
                          /* We need to set the node name based on name used to instantiate */
                          Setattr(templnode,"name",tname);
			  Delete(tname);
                          if (!specialized) {
                            Delattr(templnode,"sym:typename");
                          } else {
                            Setattr(templnode,"sym:typename","1");
                          }
                          if (yyvsp[-6].id && !inclass) {
			    /*
			       Comment this out for 1.3.28. We need to
			       re-enable it later but first we need to
			       move %ignore from using %rename to use
			       %feature(ignore).

			       String *symname = Swig_name_make(templnode,0,$3,0,0);
			    */
			    String *symname = yyvsp[-6].id;
                            Swig_cparse_template_expand(templnode,symname,temparms,tscope);
                            Setattr(templnode,"sym:name",symname);
                          } else {
                            static int cnt = 0;
                            String *nname = NewStringf("__dummy_%d__", cnt++);
                            Swig_cparse_template_expand(templnode,nname,temparms,tscope);
                            Setattr(templnode,"sym:name",nname);
			    Delete(nname);
                            Setattr(templnode,"feature:onlychildren", "typemap,typemapitem,typemapcopy,typedef,types,fragment");

			    if (yyvsp[-6].id) {
			      Swig_warning(WARN_PARSE_NESTED_TEMPLATE, cparse_file, cparse_line, "Named nested template instantiations not supported. Processing as if no name was given to %%template().\n");
			    }
                          }
                          Delattr(templnode,"templatetype");
                          Setattr(templnode,"template",nn);
                          Setfile(templnode,cparse_file);
                          Setline(templnode,cparse_line);
                          Delete(temparms);

                          add_symbols_copy(templnode);

                          if (Strcmp(nodeType(templnode),"class") == 0) {

                            /* Identify pure abstract methods */
                            Setattr(templnode,"abstract", pure_abstract(firstChild(templnode)));

                            /* Set up inheritance in symbol table */
                            {
                              Symtab  *csyms;
                              List *baselist = Getattr(templnode,"baselist");
                              csyms = Swig_symbol_current();
                              Swig_symbol_setscope(Getattr(templnode,"symtab"));
                              if (baselist) {
                                List *bases = make_inherit_list(Getattr(templnode,"name"),baselist);
                                if (bases) {
                                  Iterator s;
                                  for (s = First(bases); s.item; s = Next(s)) {
                                    Symtab *st = Getattr(s.item,"symtab");
                                    if (st) {
				      Setfile(st,Getfile(s.item));
				      Setline(st,Getline(s.item));
                                      Swig_symbol_inherit(st);
                                    }
                                  }
				  Delete(bases);
                                }
                              }
                              Swig_symbol_setscope(csyms);
                            }

                            /* Merge in %extend methods for this class */

			    /* !!! This may be broken.  We may have to add the
			       %extend methods at the beginning of the class */

                            if (extendhash) {
                              String *stmp = 0;
                              String *clsname;
                              Node *am;
                              if (Namespaceprefix) {
                                clsname = stmp = NewStringf("%s::%s", Namespaceprefix, Getattr(templnode,"name"));
                              } else {
                                clsname = Getattr(templnode,"name");
                              }
                              am = Getattr(extendhash,clsname);
                              if (am) {
                                Symtab *st = Swig_symbol_current();
                                Swig_symbol_setscope(Getattr(templnode,"symtab"));
                                /*			    Printf(stdout,"%s: %s %p %p\n", Getattr(templnode,"name"), clsname, Swig_symbol_current(), Getattr(templnode,"symtab")); */
                                merge_extensions(templnode,am);
                                Swig_symbol_setscope(st);
				append_previous_extension(templnode,am);
                                Delattr(extendhash,clsname);
                              }
			      if (stmp) Delete(stmp);
                            }
                            /* Add to classes hash */
                            if (!classes) classes = NewHash();

                            {
                              if (Namespaceprefix) {
                                String *temp = NewStringf("%s::%s", Namespaceprefix, Getattr(templnode,"name"));
                                Setattr(classes,temp,templnode);
				Delete(temp);
                              } else {
				String *qs = Swig_symbol_qualifiedscopename(templnode);
                                Setattr(classes, qs,templnode);
				Delete(qs);
                              }
                            }
                          }
                        }

                        /* all the overloaded templated functions are added into a linked list */
                        if (nscope_inner) {
                          /* non-global namespace */
                          if (templnode) {
                            appendChild(nscope_inner,templnode);
			    Delete(templnode);
                            if (nscope) yyval.node = nscope;
                          }
                        } else {
                          /* global namespace */
                          if (!linklistend) {
                            yyval.node = templnode;
                          } else {
                            set_nextSibling(linklistend,templnode);
			    Delete(templnode);
                          }
                          linklistend = templnode;
                        }
                      }
                      nn = Getattr(nn,"sym:nextSibling"); /* repeat for overloaded templated functions. If a templated class there will never be a sibling. */
                    }
		  }
	          Swig_symbol_setscope(tscope);
		  Delete(Namespaceprefix);
		  Namespaceprefix = Swig_symbol_qualifiedscopename(0);
                }
break;
case 111:
#line 3108 "parser.y"
{
		  Swig_warning(0,cparse_file, cparse_line,"%s\n", yyvsp[0].id);
		  yyval.node = 0;
               }
break;
case 112:
#line 3118 "parser.y"
{
                    yyval.node = yyvsp[0].node; 
                    if (yyval.node) {
   		      add_symbols(yyval.node);
                      default_arguments(yyval.node);
   	            }
                }
break;
case 113:
#line 3125 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 114:
#line 3126 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 115:
#line 3130 "parser.y"
{
		  if (Strcmp(yyvsp[-1].id,"C") == 0) {
		    cparse_externc = 1;
		  }
		}
break;
case 116:
#line 3134 "parser.y"
{
		  cparse_externc = 0;
		  if (Strcmp(yyvsp[-4].id,"C") == 0) {
		    Node *n = firstChild(yyvsp[-1].node);
		    yyval.node = new_node("extern");
		    Setattr(yyval.node,"name",yyvsp[-4].id);
		    appendChild(yyval.node,n);
		    while (n) {
		      SwigType *decl = Getattr(n,"decl");
		      if (SwigType_isfunction(decl) && Strcmp(Getattr(n, "storage"), "typedef") != 0) {
			Setattr(n,"storage","externc");
		      }
		      n = nextSibling(n);
		    }
		  } else {
		     Swig_warning(WARN_PARSE_UNDEFINED_EXTERN,cparse_file, cparse_line,"Unrecognized extern type \"%s\".\n", yyvsp[-4].id);
		    yyval.node = new_node("extern");
		    Setattr(yyval.node,"name",yyvsp[-4].id);
		    appendChild(yyval.node,firstChild(yyvsp[-1].node));
		  }
                }
break;
case 117:
#line 3161 "parser.y"
{
              yyval.node = new_node("cdecl");
	      if (yyvsp[-1].dtype.qualifier) SwigType_push(yyvsp[-2].decl.type,yyvsp[-1].dtype.qualifier);
	      Setattr(yyval.node,"type",yyvsp[-3].type);
	      Setattr(yyval.node,"storage",yyvsp[-4].id);
	      Setattr(yyval.node,"name",yyvsp[-2].decl.id);
	      Setattr(yyval.node,"decl",yyvsp[-2].decl.type);
	      Setattr(yyval.node,"parms",yyvsp[-2].decl.parms);
	      Setattr(yyval.node,"value",yyvsp[-1].dtype.val);
	      Setattr(yyval.node,"throws",yyvsp[-1].dtype.throws);
	      Setattr(yyval.node,"throw",yyvsp[-1].dtype.throwf);
	      if (!yyvsp[0].node) {
		if (Len(scanner_ccode)) {
		  String *code = Copy(scanner_ccode);
		  Setattr(yyval.node,"code",code);
		  Delete(code);
		}
	      } else {
		Node *n = yyvsp[0].node;
		/* Inherit attributes */
		while (n) {
		  String *type = Copy(yyvsp[-3].type);
		  Setattr(n,"type",type);
		  Setattr(n,"storage",yyvsp[-4].id);
		  n = nextSibling(n);
		  Delete(type);
		}
	      }
	      if (yyvsp[-1].dtype.bitfield) {
		Setattr(yyval.node,"bitfield", yyvsp[-1].dtype.bitfield);
	      }

	      /* Look for "::" declarations (ignored) */
	      if (Strstr(yyvsp[-2].decl.id,"::")) {
                /* This is a special case. If the scope name of the declaration exactly
                   matches that of the declaration, then we will allow it. Otherwise, delete. */
                String *p = Swig_scopename_prefix(yyvsp[-2].decl.id);
		if (p) {
		  if ((Namespaceprefix && Strcmp(p,Namespaceprefix) == 0) ||
		      (inclass && Strcmp(p,Classprefix) == 0)) {
		    String *lstr = Swig_scopename_last(yyvsp[-2].decl.id);
		    Setattr(yyval.node,"name",lstr);
		    Delete(lstr);
		    set_nextSibling(yyval.node,yyvsp[0].node);
		  } else {
		    Delete(yyval.node);
		    yyval.node = yyvsp[0].node;
		  }
		  Delete(p);
		} else {
		  Delete(yyval.node);
		  yyval.node = yyvsp[0].node;
		}
	      } else {
		set_nextSibling(yyval.node,yyvsp[0].node);
	      }
           }
break;
case 118:
#line 3222 "parser.y"
{ 
                   yyval.node = 0;
                   Clear(scanner_ccode); 
               }
break;
case 119:
#line 3226 "parser.y"
{
		 yyval.node = new_node("cdecl");
		 if (yyvsp[-1].dtype.qualifier) SwigType_push(yyvsp[-2].decl.type,yyvsp[-1].dtype.qualifier);
		 Setattr(yyval.node,"name",yyvsp[-2].decl.id);
		 Setattr(yyval.node,"decl",yyvsp[-2].decl.type);
		 Setattr(yyval.node,"parms",yyvsp[-2].decl.parms);
		 Setattr(yyval.node,"value",yyvsp[-1].dtype.val);
		 Setattr(yyval.node,"throws",yyvsp[-1].dtype.throws);
		 Setattr(yyval.node,"throw",yyvsp[-1].dtype.throwf);
		 if (yyvsp[-1].dtype.bitfield) {
		   Setattr(yyval.node,"bitfield", yyvsp[-1].dtype.bitfield);
		 }
		 if (!yyvsp[0].node) {
		   if (Len(scanner_ccode)) {
		     String *code = Copy(scanner_ccode);
		     Setattr(yyval.node,"code",code);
		     Delete(code);
		   }
		 } else {
		   set_nextSibling(yyval.node,yyvsp[0].node);
		 }
	       }
break;
case 120:
#line 3248 "parser.y"
{ 
                   skip_balanced('{','}');
                   yyval.node = 0;
               }
break;
case 121:
#line 3254 "parser.y"
{ 
                   yyval.dtype = yyvsp[0].dtype; 
                   yyval.dtype.qualifier = 0;
		   yyval.dtype.throws = 0;
		   yyval.dtype.throwf = 0;
              }
break;
case 122:
#line 3260 "parser.y"
{ 
                   yyval.dtype = yyvsp[0].dtype; 
		   yyval.dtype.qualifier = yyvsp[-1].str;
		   yyval.dtype.throws = 0;
		   yyval.dtype.throwf = 0;
	      }
break;
case 123:
#line 3266 "parser.y"
{ 
		   yyval.dtype = yyvsp[0].dtype; 
                   yyval.dtype.qualifier = 0;
		   yyval.dtype.throws = yyvsp[-2].pl;
		   yyval.dtype.throwf = NewString("1");
              }
break;
case 124:
#line 3272 "parser.y"
{ 
                   yyval.dtype = yyvsp[0].dtype; 
                   yyval.dtype.qualifier = yyvsp[-5].str;
		   yyval.dtype.throws = yyvsp[-2].pl;
		   yyval.dtype.throwf = NewString("1");
              }
break;
case 125:
#line 3285 "parser.y"
{
		   SwigType *ty = 0;
		   yyval.node = new_node("enumforward");
		   ty = NewStringf("enum %s", yyvsp[-1].id);
		   Setattr(yyval.node,"name",yyvsp[-1].id);
		   Setattr(yyval.node,"type",ty);
		   Setattr(yyval.node,"sym:weak", "1");
		   add_symbols(yyval.node);
	      }
break;
case 126:
#line 3300 "parser.y"
{
		  SwigType *ty = 0;
                  yyval.node = new_node("enum");
		  ty = NewStringf("enum %s", yyvsp[-4].id);
		  Setattr(yyval.node,"name",yyvsp[-4].id);
		  Setattr(yyval.node,"type",ty);
		  appendChild(yyval.node,yyvsp[-2].node);
		  add_symbols(yyval.node);       /* Add to tag space */
		  add_symbols(yyvsp[-2].node);       /* Add enum values to id space */
               }
break;
case 127:
#line 3310 "parser.y"
{
		 Node *n;
		 SwigType *ty = 0;
		 String   *unnamed = 0;
		 int       unnamedinstance = 0;

		 yyval.node = new_node("enum");
		 if (yyvsp[-6].id) {
		   Setattr(yyval.node,"name",yyvsp[-6].id);
		   ty = NewStringf("enum %s", yyvsp[-6].id);
		 } else if (yyvsp[-2].decl.id) {
		   unnamed = make_unnamed();
		   ty = NewStringf("enum %s", unnamed);
		   Setattr(yyval.node,"unnamed",unnamed);
                   /* name is not set for unnamed enum instances, e.g. enum { foo } Instance; */
		   if (yyvsp[-8].id && Cmp(yyvsp[-8].id,"typedef") == 0) {
		     Setattr(yyval.node,"name",yyvsp[-2].decl.id);
                   } else {
                     unnamedinstance = 1;
                   }
		   Setattr(yyval.node,"storage",yyvsp[-8].id);
		 }
		 if (yyvsp[-2].decl.id && Cmp(yyvsp[-8].id,"typedef") == 0) {
		   Setattr(yyval.node,"tdname",yyvsp[-2].decl.id);
                   Setattr(yyval.node,"allows_typedef","1");
                 }
		 appendChild(yyval.node,yyvsp[-4].node);
		 n = new_node("cdecl");
		 Setattr(n,"type",ty);
		 Setattr(n,"name",yyvsp[-2].decl.id);
		 Setattr(n,"storage",yyvsp[-8].id);
		 Setattr(n,"decl",yyvsp[-2].decl.type);
		 Setattr(n,"parms",yyvsp[-2].decl.parms);
		 Setattr(n,"unnamed",unnamed);

                 if (unnamedinstance) {
		   SwigType *cty = NewString("enum ");
		   Setattr(yyval.node,"type",cty);
		   SetFlag(yyval.node,"unnamedinstance");
		   SetFlag(n,"unnamedinstance");
		   Delete(cty);
                 }
		 if (yyvsp[0].node) {
		   Node *p = yyvsp[0].node;
		   set_nextSibling(n,p);
		   while (p) {
		     SwigType *cty = Copy(ty);
		     Setattr(p,"type",cty);
		     Setattr(p,"unnamed",unnamed);
		     Setattr(p,"storage",yyvsp[-8].id);
		     Delete(cty);
		     p = nextSibling(p);
		   }
		 } else {
		   if (Len(scanner_ccode)) {
		     String *code = Copy(scanner_ccode);
		     Setattr(n,"code",code);
		     Delete(code);
		   }
		 }

                 /* Ensure that typedef enum ABC {foo} XYZ; uses XYZ for sym:name, like structs.
                  * Note that class_rename/yyrename are bit of a mess so used this simple approach to change the name. */
                 if (yyvsp[-2].decl.id && yyvsp[-6].id && Cmp(yyvsp[-8].id,"typedef") == 0) {
		   String *name = NewString(yyvsp[-2].decl.id);
                   Setattr(yyval.node, "parser:makename", name);
		   Delete(name);
                 }

		 add_symbols(yyval.node);       /* Add enum to tag space */
		 set_nextSibling(yyval.node,n);
		 Delete(n);
		 add_symbols(yyvsp[-4].node);       /* Add enum values to id space */
	         add_symbols(n);
		 Delete(unnamed);
	       }
break;
case 128:
#line 3388 "parser.y"
{
                   /* This is a sick hack.  If the ctor_end has parameters,
                      and the parms parameter only has 1 parameter, this
                      could be a declaration of the form:

                         type (id)(parms)

			 Otherwise it's an error. */
                    int err = 0;
                    yyval.node = 0;

		    if ((ParmList_len(yyvsp[-2].pl) == 1) && (!Swig_scopename_check(yyvsp[-4].type))) {
		      SwigType *ty = Getattr(yyvsp[-2].pl,"type");
		      String *name = Getattr(yyvsp[-2].pl,"name");
		      err = 1;
		      if (!name) {
			yyval.node = new_node("cdecl");
			Setattr(yyval.node,"type",yyvsp[-4].type);
			Setattr(yyval.node,"storage",yyvsp[-5].id);
			Setattr(yyval.node,"name",ty);

			if (yyvsp[0].decl.have_parms) {
			  SwigType *decl = NewStringEmpty();
			  SwigType_add_function(decl,yyvsp[0].decl.parms);
			  Setattr(yyval.node,"decl",decl);
			  Setattr(yyval.node,"parms",yyvsp[0].decl.parms);
			  if (Len(scanner_ccode)) {
			    String *code = Copy(scanner_ccode);
			    Setattr(yyval.node,"code",code);
			    Delete(code);
			  }
			}
			if (yyvsp[0].decl.defarg) {
			  Setattr(yyval.node,"value",yyvsp[0].decl.defarg);
			}
			Setattr(yyval.node,"throws",yyvsp[0].decl.throws);
			Setattr(yyval.node,"throw",yyvsp[0].decl.throwf);
			err = 0;
		      }
		    }
		    if (err) {
		      Swig_error(cparse_file,cparse_line,"Syntax error in input(2).\n");
		      exit(1);
		    }
                }
break;
case 129:
#line 3439 "parser.y"
{  yyval.node = yyvsp[0].node; }
break;
case 130:
#line 3440 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 131:
#line 3441 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 132:
#line 3442 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 133:
#line 3443 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 134:
#line 3444 "parser.y"
{ yyval.node = 0; }
break;
case 135:
#line 3449 "parser.y"
{
                 if (nested_template == 0) {
                   String *prefix;
                   List *bases = 0;
		   Node *scope = 0;
		   yyval.node = new_node("class");
		   Setline(yyval.node,cparse_start_line);
		   Setattr(yyval.node,"kind",yyvsp[-3].id);
		   if (yyvsp[-1].bases) {
		     Setattr(yyval.node,"baselist", Getattr(yyvsp[-1].bases,"public"));
		     Setattr(yyval.node,"protectedbaselist", Getattr(yyvsp[-1].bases,"protected"));
		     Setattr(yyval.node,"privatebaselist", Getattr(yyvsp[-1].bases,"private"));
		   }
		   Setattr(yyval.node,"allows_typedef","1");

		   /* preserve the current scope */
		   prev_symtab = Swig_symbol_current();
		  
		   /* If the class name is qualified.  We need to create or lookup namespace/scope entries */
		   scope = resolve_create_node_scope(yyvsp[-2].str);
		   Setfile(scope,cparse_file);
		   Setline(scope,cparse_line);
		   yyvsp[-2].str = scope;
		   
		   /* support for old nested classes "pseudo" support, such as:

		         %rename(Ala__Ola) Ala::Ola;
			class Ala::Ola {
			public:
			    Ola() {}
		         };

		      this should disappear when a proper implementation is added.
		   */
		   if (nscope_inner && Strcmp(nodeType(nscope_inner),"namespace") != 0) {
		     if (Namespaceprefix) {
		       String *name = NewStringf("%s::%s", Namespaceprefix, yyvsp[-2].str);		       
		       yyvsp[-2].str = name;
		       Namespaceprefix = 0;
		       nscope_inner = 0;
		     }
		   }
		   Setattr(yyval.node,"name",yyvsp[-2].str);

		   Delete(class_rename);
                   class_rename = make_name(yyval.node,yyvsp[-2].str,0);
		   Classprefix = NewString(yyvsp[-2].str);
		   /* Deal with inheritance  */
		   if (yyvsp[-1].bases) {
		     bases = make_inherit_list(yyvsp[-2].str,Getattr(yyvsp[-1].bases,"public"));
		   }
		   prefix = SwigType_istemplate_templateprefix(yyvsp[-2].str);
		   if (prefix) {
		     String *fbase, *tbase;
		     if (Namespaceprefix) {
		       fbase = NewStringf("%s::%s", Namespaceprefix,yyvsp[-2].str);
		       tbase = NewStringf("%s::%s", Namespaceprefix, prefix);
		     } else {
		       fbase = Copy(yyvsp[-2].str);
		       tbase = Copy(prefix);
		     }
		     Swig_name_inherit(tbase,fbase);
		     Delete(fbase);
		     Delete(tbase);
		   }
                   if (strcmp(yyvsp[-3].id,"class") == 0) {
		     cplus_mode = CPLUS_PRIVATE;
		   } else {
		     cplus_mode = CPLUS_PUBLIC;
		   }
		   Swig_symbol_newscope();
		   Swig_symbol_setscopename(yyvsp[-2].str);
		   if (bases) {
		     Iterator s;
		     for (s = First(bases); s.item; s = Next(s)) {
		       Symtab *st = Getattr(s.item,"symtab");
		       if (st) {
			 Setfile(st,Getfile(s.item));
			 Setline(st,Getline(s.item));
			 Swig_symbol_inherit(st); 
		       }
		     }
		     Delete(bases);
		   }
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		   cparse_start_line = cparse_line;

		   /* If there are active template parameters, we need to make sure they are
                      placed in the class symbol table so we can catch shadows */

		   if (template_parameters) {
		     Parm *tp = template_parameters;
		     while(tp) {
		       String *tpname = Copy(Getattr(tp,"name"));
		       Node *tn = new_node("templateparm");
		       Setattr(tn,"name",tpname);
		       Swig_symbol_cadd(tpname,tn);
		       tp = nextSibling(tp);
		       Delete(tpname);
		     }
		   }
		   if (class_level >= max_class_levels) {
		       if (!max_class_levels) {
			   max_class_levels = 16;
		       } else {
			   max_class_levels *= 2;
		       }
		       class_decl = (Node**) realloc(class_decl, sizeof(Node*) * max_class_levels);
		       if (!class_decl) {
			   Swig_error(cparse_file, cparse_line, "realloc() failed\n");
		       }
		   }
		   class_decl[class_level++] = yyval.node;
		   Delete(prefix);
		   inclass = 1;
		 }
               }
break;
case 136:
#line 3566 "parser.y"
{
	         (void) yyvsp[-3].node;
		 if (nested_template == 0) {
		   Node *p;
		   SwigType *ty;
		   Symtab *cscope = prev_symtab;
		   Node *am = 0;
		   String *scpname = 0;
		   yyval.node = class_decl[--class_level];
		   inclass = 0;
		   
		   /* Check for pure-abstract class */
		   Setattr(yyval.node,"abstract", pure_abstract(yyvsp[-2].node));
		   
		   /* This bit of code merges in a previously defined %extend directive (if any) */
		   
		   if (extendhash) {
		     String *clsname = Swig_symbol_qualifiedscopename(0);
		     am = Getattr(extendhash,clsname);
		     if (am) {
		       merge_extensions(yyval.node,am);
		       Delattr(extendhash,clsname);
		     }
		     Delete(clsname);
		   }
		   if (!classes) classes = NewHash();
		   scpname = Swig_symbol_qualifiedscopename(0);
		   Setattr(classes,scpname,yyval.node);
		   Delete(scpname);

		   appendChild(yyval.node,yyvsp[-2].node);
		   
		   if (am) append_previous_extension(yyval.node,am);

		   p = yyvsp[0].node;
		   if (p) {
		     set_nextSibling(yyval.node,p);
		   }
		   
		   if (cparse_cplusplus && !cparse_externc) {
		     ty = NewString(yyvsp[-6].str);
		   } else {
		     ty = NewStringf("%s %s", yyvsp[-7].id,yyvsp[-6].str);
		   }
		   while (p) {
		     Setattr(p,"storage",yyvsp[-8].id);
		     Setattr(p,"type",ty);
		     p = nextSibling(p);
		   }
		   /* Dump nested classes */
		   {
		     String *name = yyvsp[-6].str;
		     if (yyvsp[0].node) {
		       SwigType *decltype = Getattr(yyvsp[0].node,"decl");
		       if (Cmp(yyvsp[-8].id,"typedef") == 0) {
			 if (!decltype || !Len(decltype)) {
			   String *cname;
			   String *tdscopename;
			   String *class_scope = Swig_symbol_qualifiedscopename(cscope);
			   name = Getattr(yyvsp[0].node,"name");
			   cname = Copy(name);
			   Setattr(yyval.node,"tdname",cname);
			   tdscopename = class_scope ? NewStringf("%s::%s", class_scope, name) : Copy(name);

			   /* Use typedef name as class name */
			   if (class_rename && (Strcmp(class_rename,yyvsp[-6].str) == 0)) {
			     Delete(class_rename);
			     class_rename = NewString(name);
			   }
			   if (!Getattr(classes,tdscopename)) {
			     Setattr(classes,tdscopename,yyval.node);
			   }
			   Setattr(yyval.node,"decl",decltype);
			   Delete(class_scope);
			   Delete(cname);
			   Delete(tdscopename);
			 }
		       }
		     }
		     appendChild(yyval.node,dump_nested(Char(name)));
		   }

		   if (cplus_mode != CPLUS_PUBLIC) {
		   /* we 'open' the class at the end, to allow %template
		      to add new members */
		     Node *pa = new_node("access");
		     Setattr(pa,"kind","public");
		     cplus_mode = CPLUS_PUBLIC;
		     appendChild(yyval.node,pa);
		     Delete(pa);
		   }

		   Setattr(yyval.node,"symtab",Swig_symbol_popscope());

		   Classprefix = 0;
		   if (nscope_inner) {
		     /* this is tricky */
		     /* we add the declaration in the original namespace */
		     appendChild(nscope_inner,yyval.node);
		     Swig_symbol_setscope(Getattr(nscope_inner,"symtab"));
		     Delete(Namespaceprefix);
		     Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		     add_symbols(yyval.node);
		     if (nscope) yyval.node = nscope;
		     /* but the variable definition in the current scope */
		     Swig_symbol_setscope(cscope);
		     Delete(Namespaceprefix);
		     Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		     add_symbols(yyvsp[0].node);
		   } else {
		     Delete(yyrename);
		     yyrename = Copy(class_rename);
		     Delete(Namespaceprefix);
		     Namespaceprefix = Swig_symbol_qualifiedscopename(0);

		     add_symbols(yyval.node);
		     add_symbols(yyvsp[0].node);
		   }
		   Swig_symbol_setscope(cscope);
		   Delete(Namespaceprefix);
		   Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		 } else {
		    yyval.node = new_node("class");
		    Setattr(yyval.node,"kind",yyvsp[-7].id);
		    Setattr(yyval.node,"name",NewString(yyvsp[-6].str));
		    SetFlag(yyval.node,"nestedtemplateclass");
		 }
	       }
break;
case 137:
#line 3697 "parser.y"
{
	       String *unnamed;
	       unnamed = make_unnamed();
	       yyval.node = new_node("class");
	       Setline(yyval.node,cparse_start_line);
	       Setattr(yyval.node,"kind",yyvsp[-1].id);
	       Setattr(yyval.node,"storage",yyvsp[-2].id);
	       Setattr(yyval.node,"unnamed",unnamed);
	       Setattr(yyval.node,"allows_typedef","1");
	       Delete(class_rename);
	       class_rename = make_name(yyval.node,0,0);
	       if (strcmp(yyvsp[-1].id,"class") == 0) {
		 cplus_mode = CPLUS_PRIVATE;
	       } else {
		 cplus_mode = CPLUS_PUBLIC;
	       }
	       Swig_symbol_newscope();
	       cparse_start_line = cparse_line;
	       if (class_level >= max_class_levels) {
		   if (!max_class_levels) {
		       max_class_levels = 16;
		   } else {
		       max_class_levels *= 2;
		   }
		   class_decl = (Node**) realloc(class_decl, sizeof(Node*) * max_class_levels);
		   if (!class_decl) {
		       Swig_error(cparse_file, cparse_line, "realloc() failed\n");
		   }
	       }
	       class_decl[class_level++] = yyval.node;
	       inclass = 1;
	       Classprefix = NewStringEmpty();
	       Delete(Namespaceprefix);
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
             }
break;
case 138:
#line 3731 "parser.y"
{
	       String *unnamed;
	       Node *n;
	       (void) yyvsp[-5].node;
	       Classprefix = 0;
	       yyval.node = class_decl[--class_level];
	       inclass = 0;
	       unnamed = Getattr(yyval.node,"unnamed");

	       /* Check for pure-abstract class */
	       Setattr(yyval.node,"abstract", pure_abstract(yyvsp[-4].node));

	       n = new_node("cdecl");
	       Setattr(n,"name",yyvsp[-2].decl.id);
	       Setattr(n,"unnamed",unnamed);
	       Setattr(n,"type",unnamed);
	       Setattr(n,"decl",yyvsp[-2].decl.type);
	       Setattr(n,"parms",yyvsp[-2].decl.parms);
	       Setattr(n,"storage",yyvsp[-8].id);
	       if (yyvsp[0].node) {
		 Node *p = yyvsp[0].node;
		 set_nextSibling(n,p);
		 while (p) {
		   String *type = Copy(unnamed);
		   Setattr(p,"name",yyvsp[-2].decl.id);
		   Setattr(p,"unnamed",unnamed);
		   Setattr(p,"type",type);
		   Delete(type);
		   Setattr(p,"storage",yyvsp[-8].id);
		   p = nextSibling(p);
		 }
	       }
	       set_nextSibling(yyval.node,n);
	       Delete(n);
	       {
		 /* If a proper typedef name was given, we'll use it to set the scope name */
		 String *name = 0;
		 if (yyvsp[-8].id && (strcmp(yyvsp[-8].id,"typedef") == 0)) {
		   if (!Len(yyvsp[-2].decl.type)) {	
		     String *scpname = 0;
		     name = yyvsp[-2].decl.id;
		     Setattr(yyval.node,"tdname",name);
		     Setattr(yyval.node,"name",name);
		     Swig_symbol_setscopename(name);

		     /* If a proper name was given, we use that as the typedef, not unnamed */
		     Clear(unnamed);
		     Append(unnamed, name);
		     
		     n = nextSibling(n);
		     set_nextSibling(yyval.node,n);

		     /* Check for previous extensions */
		     if (extendhash) {
		       String *clsname = Swig_symbol_qualifiedscopename(0);
		       Node *am = Getattr(extendhash,clsname);
		       if (am) {
			 /* Merge the extension into the symbol table */
			 merge_extensions(yyval.node,am);
			 append_previous_extension(yyval.node,am);
			 Delattr(extendhash,clsname);
		       }
		       Delete(clsname);
		     }
		     if (!classes) classes = NewHash();
		     scpname = Swig_symbol_qualifiedscopename(0);
		     Setattr(classes,scpname,yyval.node);
		     Delete(scpname);
		   } else {
		     Swig_symbol_setscopename("<unnamed>");
		   }
		 }
		 appendChild(yyval.node,yyvsp[-4].node);
		 appendChild(yyval.node,dump_nested(Char(name)));
	       }
	       /* Pop the scope */
	       Setattr(yyval.node,"symtab",Swig_symbol_popscope());
	       if (class_rename) {
		 Delete(yyrename);
		 yyrename = NewString(class_rename);
	       }
	       Delete(Namespaceprefix);
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	       add_symbols(yyval.node);
	       add_symbols(n);
	       Delete(unnamed);
              }
break;
case 139:
#line 3820 "parser.y"
{ yyval.node = 0; }
break;
case 140:
#line 3821 "parser.y"
{
                        yyval.node = new_node("cdecl");
                        Setattr(yyval.node,"name",yyvsp[-2].decl.id);
                        Setattr(yyval.node,"decl",yyvsp[-2].decl.type);
                        Setattr(yyval.node,"parms",yyvsp[-2].decl.parms);
			set_nextSibling(yyval.node,yyvsp[0].node);
                    }
break;
case 141:
#line 3833 "parser.y"
{
              if (yyvsp[-3].id && (Strcmp(yyvsp[-3].id,"friend") == 0)) {
		/* Ignore */
                yyval.node = 0; 
	      } else {
		yyval.node = new_node("classforward");
		Setfile(yyval.node,cparse_file);
		Setline(yyval.node,cparse_line);
		Setattr(yyval.node,"kind",yyvsp[-2].id);
		Setattr(yyval.node,"name",yyvsp[-1].str);
		Setattr(yyval.node,"sym:weak", "1");
		add_symbols(yyval.node);
	      }
             }
break;
case 142:
#line 3853 "parser.y"
{ 
		    template_parameters = yyvsp[-1].tparms; 
		    if (inclass)
		      nested_template++;

		  }
break;
case 143:
#line 3858 "parser.y"
{

		    /* Don't ignore templated functions declared within a class, unless the templated function is within a nested class */
		    if (nested_template <= 1) {
		      int is_nested_template_class = yyvsp[0].node && GetFlag(yyvsp[0].node, "nestedtemplateclass");
		      if (is_nested_template_class) {
			yyval.node = 0;
			/* Nested template classes would probably better be ignored like ordinary nested classes using cpp_nested, but that introduces shift/reduce conflicts */
			if (cplus_mode == CPLUS_PUBLIC) {
			  /* Treat the nested class/struct/union as a forward declaration until a proper nested class solution is implemented */
			  String *kind = Getattr(yyvsp[0].node, "kind");
			  String *name = Getattr(yyvsp[0].node, "name");
			  yyval.node = new_node("template");
			  Setattr(yyval.node,"kind",kind);
			  Setattr(yyval.node,"name",name);
			  Setattr(yyval.node,"sym:weak", "1");
			  Setattr(yyval.node,"templatetype","classforward");
			  Setattr(yyval.node,"templateparms", yyvsp[-3].tparms);
			  add_symbols(yyval.node);

			  if (GetFlag(yyval.node, "feature:nestedworkaround")) {
			    Swig_symbol_remove(yyval.node);
			    yyval.node = 0;
			  } else {
			    SWIG_WARN_NODE_BEGIN(yyval.node);
			    Swig_warning(WARN_PARSE_NAMED_NESTED_CLASS, cparse_file, cparse_line, "Nested template %s not currently supported (%s ignored).\n", kind, name);
			    SWIG_WARN_NODE_END(yyval.node);
			  }
			}
			Delete(yyvsp[0].node);
		      } else {
			String *tname = 0;
			int     error = 0;

			/* check if we get a namespace node with a class declaration, and retrieve the class */
			Symtab *cscope = Swig_symbol_current();
			Symtab *sti = 0;
			Node *ntop = yyvsp[0].node;
			Node *ni = ntop;
			SwigType *ntype = ni ? nodeType(ni) : 0;
			while (ni && Strcmp(ntype,"namespace") == 0) {
			  sti = Getattr(ni,"symtab");
			  ni = firstChild(ni);
			  ntype = nodeType(ni);
			}
			if (sti) {
			  Swig_symbol_setscope(sti);
			  Delete(Namespaceprefix);
			  Namespaceprefix = Swig_symbol_qualifiedscopename(0);
			  yyvsp[0].node = ni;
			}

			yyval.node = yyvsp[0].node;
			if (yyval.node) tname = Getattr(yyval.node,"name");
			
			/* Check if the class is a template specialization */
			if ((yyval.node) && (Strchr(tname,'<')) && (!is_operator(tname))) {
			  /* If a specialization.  Check if defined. */
			  Node *tempn = 0;
			  {
			    String *tbase = SwigType_templateprefix(tname);
			    tempn = Swig_symbol_clookup_local(tbase,0);
			    if (!tempn || (Strcmp(nodeType(tempn),"template") != 0)) {
			      SWIG_WARN_NODE_BEGIN(tempn);
			      Swig_warning(WARN_PARSE_TEMPLATE_SP_UNDEF, Getfile(yyval.node),Getline(yyval.node),"Specialization of non-template '%s'.\n", tbase);
			      SWIG_WARN_NODE_END(tempn);
			      tempn = 0;
			      error = 1;
			    }
			    Delete(tbase);
			  }
			  Setattr(yyval.node,"specialization","1");
			  Setattr(yyval.node,"templatetype",nodeType(yyval.node));
			  set_nodeType(yyval.node,"template");
			  /* Template partial specialization */
			  if (tempn && (yyvsp[-3].tparms) && (yyvsp[0].node)) {
			    List   *tlist;
			    String *targs = SwigType_templateargs(tname);
			    tlist = SwigType_parmlist(targs);
			    /*			  Printf(stdout,"targs = '%s' %s\n", targs, tlist); */
			    if (!Getattr(yyval.node,"sym:weak")) {
			      Setattr(yyval.node,"sym:typename","1");
			    }
			    
			    if (Len(tlist) != ParmList_len(Getattr(tempn,"templateparms"))) {
			      Swig_error(Getfile(yyval.node),Getline(yyval.node),"Inconsistent argument count in template partial specialization. %d %d\n", Len(tlist), ParmList_len(Getattr(tempn,"templateparms")));
			      
			    } else {

			    /* This code builds the argument list for the partial template
			       specialization.  This is a little hairy, but the idea is as
			       follows:

			       $3 contains a list of arguments supplied for the template.
			       For example template<class T>.

			       tlist is a list of the specialization arguments--which may be
			       different.  For example class<int,T>.

			       tp is a copy of the arguments in the original template definition.
       
			       The patching algorithm walks through the list of supplied
			       arguments ($3), finds the position in the specialization arguments
			       (tlist), and then patches the name in the argument list of the
			       original template.
			    */

			    {
			      String *pn;
			      Parm *p, *p1;
			      int i, nargs;
			      Parm *tp = CopyParmList(Getattr(tempn,"templateparms"));
			      nargs = Len(tlist);
			      p = yyvsp[-3].tparms;
			      while (p) {
				for (i = 0; i < nargs; i++){
				  pn = Getattr(p,"name");
				  if (Strcmp(pn,SwigType_base(Getitem(tlist,i))) == 0) {
				    int j;
				    Parm *p1 = tp;
				    for (j = 0; j < i; j++) {
				      p1 = nextSibling(p1);
				    }
				    Setattr(p1,"name",pn);
				    Setattr(p1,"partialarg","1");
				  }
				}
				p = nextSibling(p);
			      }
			      p1 = tp;
			      i = 0;
			      while (p1) {
				if (!Getattr(p1,"partialarg")) {
				  Delattr(p1,"name");
				  Setattr(p1,"type", Getitem(tlist,i));
				} 
				i++;
				p1 = nextSibling(p1);
			      }
			      Setattr(yyval.node,"templateparms",tp);
			      Delete(tp);
			    }
  #if 0
			    /* Patch the parameter list */
			    if (tempn) {
			      Parm *p,*p1;
			      ParmList *tp = CopyParmList(Getattr(tempn,"templateparms"));
			      p = yyvsp[-3].tparms;
			      p1 = tp;
			      while (p && p1) {
				String *pn = Getattr(p,"name");
				Printf(stdout,"pn = '%s'\n", pn);
				if (pn) Setattr(p1,"name",pn);
				else Delattr(p1,"name");
				pn = Getattr(p,"type");
				if (pn) Setattr(p1,"type",pn);
				p = nextSibling(p);
				p1 = nextSibling(p1);
			      }
			      Setattr(yyval.node,"templateparms",tp);
			      Delete(tp);
			    } else {
			      Setattr(yyval.node,"templateparms",yyvsp[-3].tparms);
			    }
  #endif
			    Delattr(yyval.node,"specialization");
			    Setattr(yyval.node,"partialspecialization","1");
			    /* Create a specialized name for matching */
			    {
			      Parm *p = yyvsp[-3].tparms;
			      String *fname = NewString(Getattr(yyval.node,"name"));
			      String *ffname = 0;
			      ParmList *partialparms = 0;

			      char   tmp[32];
			      int    i, ilen;
			      while (p) {
				String *n = Getattr(p,"name");
				if (!n) {
				  p = nextSibling(p);
				  continue;
				}
				ilen = Len(tlist);
				for (i = 0; i < ilen; i++) {
				  if (Strstr(Getitem(tlist,i),n)) {
				    sprintf(tmp,"$%d",i+1);
				    Replaceid(fname,n,tmp);
				  }
				}
				p = nextSibling(p);
			      }
			      /* Patch argument names with typedef */
			      {
				Iterator tt;
				Parm *parm_current = 0;
				List *tparms = SwigType_parmlist(fname);
				ffname = SwigType_templateprefix(fname);
				Append(ffname,"<(");
				for (tt = First(tparms); tt.item; ) {
				  SwigType *rtt = Swig_symbol_typedef_reduce(tt.item,0);
				  SwigType *ttr = Swig_symbol_type_qualify(rtt,0);

				  Parm *newp = NewParmWithoutFileLineInfo(ttr, 0);
				  if (partialparms)
				    set_nextSibling(parm_current, newp);
				  else
				    partialparms = newp;
				  parm_current = newp;

				  Append(ffname,ttr);
				  tt = Next(tt);
				  if (tt.item) Putc(',',ffname);
				  Delete(rtt);
				  Delete(ttr);
				}
				Delete(tparms);
				Append(ffname,")>");
			      }
			      {
				Node *new_partial = NewHash();
				String *partials = Getattr(tempn,"partials");
				if (!partials) {
				  partials = NewList();
				  Setattr(tempn,"partials",partials);
				  Delete(partials);
				}
				/*			      Printf(stdout,"partial: fname = '%s', '%s'\n", fname, Swig_symbol_typedef_reduce(fname,0)); */
				Setattr(new_partial, "partialparms", partialparms);
				Setattr(new_partial, "templcsymname", ffname);
				Append(partials, new_partial);
			      }
			      Setattr(yyval.node,"partialargs",ffname);
			      Swig_symbol_cadd(ffname,yyval.node);
			    }
			    }
			    Delete(tlist);
			    Delete(targs);
			  } else {
			    /* An explicit template specialization */
			    /* add default args from primary (unspecialized) template */
			    String *ty = Swig_symbol_template_deftype(tname,0);
			    String *fname = Swig_symbol_type_qualify(ty,0);
			    Swig_symbol_cadd(fname,yyval.node);
			    Delete(ty);
			    Delete(fname);
			  }
			}  else if (yyval.node) {
			  Setattr(yyval.node,"templatetype",nodeType(yyvsp[0].node));
			  set_nodeType(yyval.node,"template");
			  Setattr(yyval.node,"templateparms", yyvsp[-3].tparms);
			  if (!Getattr(yyval.node,"sym:weak")) {
			    Setattr(yyval.node,"sym:typename","1");
			  }
			  add_symbols(yyval.node);
			  default_arguments(yyval.node);
			  /* We also place a fully parameterized version in the symbol table */
			  {
			    Parm *p;
			    String *fname = NewStringf("%s<(", Getattr(yyval.node,"name"));
			    p = yyvsp[-3].tparms;
			    while (p) {
			      String *n = Getattr(p,"name");
			      if (!n) n = Getattr(p,"type");
			      Append(fname,n);
			      p = nextSibling(p);
			      if (p) Putc(',',fname);
			    }
			    Append(fname,")>");
			    Swig_symbol_cadd(fname,yyval.node);
			  }
			}
			yyval.node = ntop;
			Swig_symbol_setscope(cscope);
			Delete(Namespaceprefix);
			Namespaceprefix = Swig_symbol_qualifiedscopename(0);
			if (error) yyval.node = 0;
		      }
		    } else {
		      yyval.node = 0;
		    }
		    template_parameters = 0;
		    if (inclass)
		      nested_template--;
                  }
break;
case 144:
#line 4142 "parser.y"
{
		  Swig_warning(WARN_PARSE_EXPLICIT_TEMPLATE, cparse_file, cparse_line, "Explicit template instantiation ignored.\n");
                   yyval.node = 0; 
                }
break;
case 145:
#line 4148 "parser.y"
{
		  yyval.node = yyvsp[0].node;
                }
break;
case 146:
#line 4151 "parser.y"
{
                   yyval.node = yyvsp[0].node;
                }
break;
case 147:
#line 4154 "parser.y"
{
                   yyval.node = yyvsp[0].node;
                }
break;
case 148:
#line 4157 "parser.y"
{
		  yyval.node = 0;
                }
break;
case 149:
#line 4160 "parser.y"
{
                  yyval.node = yyvsp[0].node;
                }
break;
case 150:
#line 4163 "parser.y"
{
                  yyval.node = yyvsp[0].node;
                }
break;
case 151:
#line 4168 "parser.y"
{
		   /* Rip out the parameter names */
		  Parm *p = yyvsp[0].pl;
		  yyval.tparms = yyvsp[0].pl;

		  while (p) {
		    String *name = Getattr(p,"name");
		    if (!name) {
		      /* Hmmm. Maybe it's a 'class T' parameter */
		      char *type = Char(Getattr(p,"type"));
		      /* Template template parameter */
		      if (strncmp(type,"template<class> ",16) == 0) {
			type += 16;
		      }
		      if ((strncmp(type,"class ",6) == 0) || (strncmp(type,"typename ", 9) == 0)) {
			char *t = strchr(type,' ');
			Setattr(p,"name", t+1);
		      } else {
			/*
			 Swig_error(cparse_file, cparse_line, "Missing template parameter name\n");
			 $$.rparms = 0;
			 $$.parms = 0;
			 break; */
		      }
		    }
		    p = nextSibling(p);
		  }
                 }
break;
case 152:
#line 4198 "parser.y"
{
                      set_nextSibling(yyvsp[-1].p,yyvsp[0].pl);
                      yyval.pl = yyvsp[-1].p;
                   }
break;
case 153:
#line 4202 "parser.y"
{ yyval.pl = 0; }
break;
case 154:
#line 4205 "parser.y"
{
		    yyval.p = NewParmWithoutFileLineInfo(NewString(yyvsp[0].id), 0);
                  }
break;
case 155:
#line 4208 "parser.y"
{
                    yyval.p = yyvsp[0].p;
                  }
break;
case 156:
#line 4213 "parser.y"
{
                         set_nextSibling(yyvsp[-1].p,yyvsp[0].pl);
                         yyval.pl = yyvsp[-1].p;
                       }
break;
case 157:
#line 4217 "parser.y"
{ yyval.pl = 0; }
break;
case 158:
#line 4222 "parser.y"
{
                  String *uname = Swig_symbol_type_qualify(yyvsp[-1].str,0);
		  String *name = Swig_scopename_last(yyvsp[-1].str);
                  yyval.node = new_node("using");
		  Setattr(yyval.node,"uname",uname);
		  Setattr(yyval.node,"name", name);
		  Delete(uname);
		  Delete(name);
		  add_symbols(yyval.node);
             }
break;
case 159:
#line 4232 "parser.y"
{
	       Node *n = Swig_symbol_clookup(yyvsp[-1].str,0);
	       if (!n) {
		 Swig_error(cparse_file, cparse_line, "Nothing known about namespace '%s'\n", yyvsp[-1].str);
		 yyval.node = 0;
	       } else {

		 while (Strcmp(nodeType(n),"using") == 0) {
		   n = Getattr(n,"node");
		 }
		 if (n) {
		   if (Strcmp(nodeType(n),"namespace") == 0) {
		     Symtab *current = Swig_symbol_current();
		     Symtab *symtab = Getattr(n,"symtab");
		     yyval.node = new_node("using");
		     Setattr(yyval.node,"node",n);
		     Setattr(yyval.node,"namespace", yyvsp[-1].str);
		     if (current != symtab) {
		       Swig_symbol_inherit(symtab);
		     }
		   } else {
		     Swig_error(cparse_file, cparse_line, "'%s' is not a namespace.\n", yyvsp[-1].str);
		     yyval.node = 0;
		   }
		 } else {
		   yyval.node = 0;
		 }
	       }
             }
break;
case 160:
#line 4263 "parser.y"
{ 
                Hash *h;
                yyvsp[-2].node = Swig_symbol_current();
		h = Swig_symbol_clookup(yyvsp[-1].str,0);
		if (h && (yyvsp[-2].node == Getattr(h,"sym:symtab")) && (Strcmp(nodeType(h),"namespace") == 0)) {
		  if (Getattr(h,"alias")) {
		    h = Getattr(h,"namespace");
		    Swig_warning(WARN_PARSE_NAMESPACE_ALIAS, cparse_file, cparse_line, "Namespace alias '%s' not allowed here. Assuming '%s'\n",
				 yyvsp[-1].str, Getattr(h,"name"));
		    yyvsp[-1].str = Getattr(h,"name");
		  }
		  Swig_symbol_setscope(Getattr(h,"symtab"));
		} else {
		  Swig_symbol_newscope();
		  Swig_symbol_setscopename(yyvsp[-1].str);
		}
		Delete(Namespaceprefix);
		Namespaceprefix = Swig_symbol_qualifiedscopename(0);
             }
break;
case 161:
#line 4281 "parser.y"
{
                Node *n = yyvsp[-1].node;
		set_nodeType(n,"namespace");
		Setattr(n,"name",yyvsp[-4].str);
                Setattr(n,"symtab", Swig_symbol_popscope());
		Swig_symbol_setscope(yyvsp[-5].node);
		yyval.node = n;
		Delete(Namespaceprefix);
		Namespaceprefix = Swig_symbol_qualifiedscopename(0);
		add_symbols(yyval.node);
             }
break;
case 162:
#line 4292 "parser.y"
{
	       Hash *h;
	       yyvsp[-1].node = Swig_symbol_current();
	       h = Swig_symbol_clookup((char *)"    ",0);
	       if (h && (Strcmp(nodeType(h),"namespace") == 0)) {
		 Swig_symbol_setscope(Getattr(h,"symtab"));
	       } else {
		 Swig_symbol_newscope();
		 /* we don't use "__unnamed__", but a long 'empty' name */
		 Swig_symbol_setscopename("    ");
	       }
	       Namespaceprefix = 0;
             }
break;
case 163:
#line 4304 "parser.y"
{
	       yyval.node = yyvsp[-1].node;
	       set_nodeType(yyval.node,"namespace");
	       Setattr(yyval.node,"unnamed","1");
	       Setattr(yyval.node,"symtab", Swig_symbol_popscope());
	       Swig_symbol_setscope(yyvsp[-4].node);
	       Delete(Namespaceprefix);
	       Namespaceprefix = Swig_symbol_qualifiedscopename(0);
	       add_symbols(yyval.node);
             }
break;
case 164:
#line 4314 "parser.y"
{
	       /* Namespace alias */
	       Node *n;
	       yyval.node = new_node("namespace");
	       Setattr(yyval.node,"name",yyvsp[-3].id);
	       Setattr(yyval.node,"alias",yyvsp[-1].str);
	       n = Swig_symbol_clookup(yyvsp[-1].str,0);
	       if (!n) {
		 Swig_error(cparse_file, cparse_line, "Unknown namespace '%s'\n", yyvsp[-1].str);
		 yyval.node = 0;
	       } else {
		 if (Strcmp(nodeType(n),"namespace") != 0) {
		   Swig_error(cparse_file, cparse_line, "'%s' is not a namespace\n",yyvsp[-1].str);
		   yyval.node = 0;
		 } else {
		   while (Getattr(n,"alias")) {
		     n = Getattr(n,"namespace");
		   }
		   Setattr(yyval.node,"namespace",n);
		   add_symbols(yyval.node);
		   /* Set up a scope alias */
		   Swig_symbol_alias(yyvsp[-3].id,Getattr(n,"symtab"));
		 }
	       }
             }
break;
case 165:
#line 4341 "parser.y"
{
                   yyval.node = yyvsp[-1].node;
                   /* Insert cpp_member (including any siblings) to the front of the cpp_members linked list */
		   if (yyval.node) {
		     Node *p = yyval.node;
		     Node *pp =0;
		     while (p) {
		       pp = p;
		       p = nextSibling(p);
		     }
		     set_nextSibling(pp,yyvsp[0].node);
		   } else {
		     yyval.node = yyvsp[0].node;
		   }
             }
break;
case 166:
#line 4356 "parser.y"
{ 
                  if (cplus_mode != CPLUS_PUBLIC) {
		     Swig_error(cparse_file,cparse_line,"%%extend can only be used in a public section\n");
		  }
             }
break;
case 167:
#line 4360 "parser.y"
{
	       yyval.node = new_node("extend");
	       tag_nodes(yyvsp[-2].node,"feature:extend",(char*) "1");
	       appendChild(yyval.node,yyvsp[-2].node);
	       set_nextSibling(yyval.node,yyvsp[0].node);
	     }
break;
case 168:
#line 4366 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 169:
#line 4367 "parser.y"
{ yyval.node = 0;}
break;
case 170:
#line 4368 "parser.y"
{
	       int start_line = cparse_line;
	       skip_decl();
	       Swig_error(cparse_file,start_line,"Syntax error in input(3).\n");
	       exit(1);
	       }
break;
case 171:
#line 4373 "parser.y"
{ 
		 yyval.node = yyvsp[0].node;
   	     }
break;
case 172:
#line 4384 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 173:
#line 4385 "parser.y"
{ 
                 yyval.node = yyvsp[0].node; 
		 if (extendmode) {
		   String *symname;
		   symname= make_name(yyval.node,Getattr(yyval.node,"name"), Getattr(yyval.node,"decl"));
		   if (Strcmp(symname,Getattr(yyval.node,"name")) == 0) {
		     /* No renaming operation.  Set name to class name */
		     Delete(yyrename);
		     yyrename = NewString(Getattr(current_class,"sym:name"));
		   } else {
		     Delete(yyrename);
		     yyrename = symname;
		   }
		 }
		 add_symbols(yyval.node);
                 default_arguments(yyval.node);
             }
break;
case 174:
#line 4402 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 175:
#line 4403 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 176:
#line 4404 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 177:
#line 4405 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 178:
#line 4406 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 179:
#line 4407 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 180:
#line 4408 "parser.y"
{ yyval.node = 0; }
break;
case 181:
#line 4409 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 182:
#line 4410 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 183:
#line 4411 "parser.y"
{ yyval.node = 0; }
break;
case 184:
#line 4412 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 185:
#line 4413 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 186:
#line 4414 "parser.y"
{ yyval.node = 0; }
break;
case 187:
#line 4415 "parser.y"
{yyval.node = yyvsp[0].node; }
break;
case 188:
#line 4416 "parser.y"
{yyval.node = yyvsp[0].node; }
break;
case 189:
#line 4417 "parser.y"
{ yyval.node = 0; }
break;
case 190:
#line 4426 "parser.y"
{
              if (Classprefix) {
		 SwigType *decl = NewStringEmpty();
		 yyval.node = new_node("constructor");
		 Setattr(yyval.node,"storage",yyvsp[-5].id);
		 Setattr(yyval.node,"name",yyvsp[-4].type);
		 Setattr(yyval.node,"parms",yyvsp[-2].pl);
		 SwigType_add_function(decl,yyvsp[-2].pl);
		 Setattr(yyval.node,"decl",decl);
		 Setattr(yyval.node,"throws",yyvsp[0].decl.throws);
		 Setattr(yyval.node,"throw",yyvsp[0].decl.throwf);
		 if (Len(scanner_ccode)) {
		   String *code = Copy(scanner_ccode);
		   Setattr(yyval.node,"code",code);
		   Delete(code);
		 }
		 SetFlag(yyval.node,"feature:new");
	      } else {
		yyval.node = 0;
              }
              }
break;
case 191:
#line 4451 "parser.y"
{
               String *name = NewStringf("%s",yyvsp[-4].str);
	       if (*(Char(name)) != '~') Insert(name,0,"~");
               yyval.node = new_node("destructor");
	       Setattr(yyval.node,"name",name);
	       Delete(name);
	       if (Len(scanner_ccode)) {
		 String *code = Copy(scanner_ccode);
		 Setattr(yyval.node,"code",code);
		 Delete(code);
	       }
	       {
		 String *decl = NewStringEmpty();
		 SwigType_add_function(decl,yyvsp[-2].pl);
		 Setattr(yyval.node,"decl",decl);
		 Delete(decl);
	       }
	       Setattr(yyval.node,"throws",yyvsp[0].dtype.throws);
	       Setattr(yyval.node,"throw",yyvsp[0].dtype.throwf);
	       add_symbols(yyval.node);
	      }
break;
case 192:
#line 4475 "parser.y"
{
		String *name;
		char *c = 0;
		yyval.node = new_node("destructor");
	       /* Check for template names.  If the class is a template
		  and the constructor is missing the template part, we
		  add it */
	        if (Classprefix) {
                  c = strchr(Char(Classprefix),'<');
                  if (c && !Strchr(yyvsp[-4].str,'<')) {
                    yyvsp[-4].str = NewStringf("%s%s",yyvsp[-4].str,c);
                  }
		}
		Setattr(yyval.node,"storage","virtual");
	        name = NewStringf("%s",yyvsp[-4].str);
		if (*(Char(name)) != '~') Insert(name,0,"~");
		Setattr(yyval.node,"name",name);
		Delete(name);
		Setattr(yyval.node,"throws",yyvsp[0].dtype.throws);
		Setattr(yyval.node,"throw",yyvsp[0].dtype.throwf);
		if (yyvsp[0].dtype.val) {
		  Setattr(yyval.node,"value","0");
		}
		if (Len(scanner_ccode)) {
		  String *code = Copy(scanner_ccode);
		  Setattr(yyval.node,"code",code);
		  Delete(code);
		}
		{
		  String *decl = NewStringEmpty();
		  SwigType_add_function(decl,yyvsp[-2].pl);
		  Setattr(yyval.node,"decl",decl);
		  Delete(decl);
		}

		add_symbols(yyval.node);
	      }
break;
case 193:
#line 4516 "parser.y"
{
                 yyval.node = new_node("cdecl");
                 Setattr(yyval.node,"type",yyvsp[-5].type);
		 Setattr(yyval.node,"name",yyvsp[-6].str);
		 Setattr(yyval.node,"storage",yyvsp[-7].id);

		 SwigType_add_function(yyvsp[-4].type,yyvsp[-2].pl);
		 if (yyvsp[0].dtype.qualifier) {
		   SwigType_push(yyvsp[-4].type,yyvsp[0].dtype.qualifier);
		 }
		 Setattr(yyval.node,"decl",yyvsp[-4].type);
		 Setattr(yyval.node,"parms",yyvsp[-2].pl);
		 Setattr(yyval.node,"conversion_operator","1");
		 add_symbols(yyval.node);
              }
break;
case 194:
#line 4531 "parser.y"
{
		 SwigType *decl;
                 yyval.node = new_node("cdecl");
                 Setattr(yyval.node,"type",yyvsp[-5].type);
		 Setattr(yyval.node,"name",yyvsp[-6].str);
		 Setattr(yyval.node,"storage",yyvsp[-7].id);
		 decl = NewStringEmpty();
		 SwigType_add_reference(decl);
		 SwigType_add_function(decl,yyvsp[-2].pl);
		 if (yyvsp[0].dtype.qualifier) {
		   SwigType_push(decl,yyvsp[0].dtype.qualifier);
		 }
		 Setattr(yyval.node,"decl",decl);
		 Setattr(yyval.node,"parms",yyvsp[-2].pl);
		 Setattr(yyval.node,"conversion_operator","1");
		 add_symbols(yyval.node);
	       }
break;
case 195:
#line 4549 "parser.y"
{
		 SwigType *decl;
                 yyval.node = new_node("cdecl");
                 Setattr(yyval.node,"type",yyvsp[-6].type);
		 Setattr(yyval.node,"name",yyvsp[-7].str);
		 Setattr(yyval.node,"storage",yyvsp[-8].id);
		 decl = NewStringEmpty();
		 SwigType_add_pointer(decl);
		 SwigType_add_reference(decl);
		 SwigType_add_function(decl,yyvsp[-2].pl);
		 if (yyvsp[0].dtype.qualifier) {
		   SwigType_push(decl,yyvsp[0].dtype.qualifier);
		 }
		 Setattr(yyval.node,"decl",decl);
		 Setattr(yyval.node,"parms",yyvsp[-2].pl);
		 Setattr(yyval.node,"conversion_operator","1");
		 add_symbols(yyval.node);
	       }
break;
case 196:
#line 4568 "parser.y"
{
		String *t = NewStringEmpty();
		yyval.node = new_node("cdecl");
		Setattr(yyval.node,"type",yyvsp[-4].type);
		Setattr(yyval.node,"name",yyvsp[-5].str);
		 Setattr(yyval.node,"storage",yyvsp[-6].id);
		SwigType_add_function(t,yyvsp[-2].pl);
		if (yyvsp[0].dtype.qualifier) {
		  SwigType_push(t,yyvsp[0].dtype.qualifier);
		}
		Setattr(yyval.node,"decl",t);
		Setattr(yyval.node,"parms",yyvsp[-2].pl);
		Setattr(yyval.node,"conversion_operator","1");
		add_symbols(yyval.node);
              }
break;
case 197:
#line 4587 "parser.y"
{
                 skip_balanced('{','}');
                 yyval.node = 0;
               }
break;
case 198:
#line 4594 "parser.y"
{ 
                yyval.node = new_node("access");
		Setattr(yyval.node,"kind","public");
                cplus_mode = CPLUS_PUBLIC;
              }
break;
case 199:
#line 4601 "parser.y"
{ 
                yyval.node = new_node("access");
                Setattr(yyval.node,"kind","private");
		cplus_mode = CPLUS_PRIVATE;
	      }
break;
case 200:
#line 4609 "parser.y"
{ 
		yyval.node = new_node("access");
		Setattr(yyval.node,"kind","protected");
		cplus_mode = CPLUS_PROTECTED;
	      }
break;
case 201:
#line 4630 "parser.y"
{
		cparse_start_line = cparse_line;
		skip_balanced('{','}');
		yyval.str = NewString(scanner_ccode); /* copied as initializers overwrite scanner_ccode */
	      }
break;
case 202:
#line 4634 "parser.y"
{
	        yyval.node = 0;
		if (cplus_mode == CPLUS_PUBLIC) {
		  if (cparse_cplusplus) {
		    yyval.node = nested_forward_declaration(yyvsp[-6].id, yyvsp[-5].id, yyvsp[-4].str, yyvsp[-4].str, yyvsp[0].node);
		  } else if (yyvsp[0].node) {
		    nested_new_struct(yyvsp[-5].id, yyvsp[-1].str, yyvsp[0].node);
		  }
		}
		Delete(yyvsp[-1].str);
	      }
break;
case 203:
#line 4656 "parser.y"
{
		cparse_start_line = cparse_line;
		skip_balanced('{','}');
		yyval.str = NewString(scanner_ccode); /* copied as initializers overwrite scanner_ccode */
	      }
break;
case 204:
#line 4660 "parser.y"
{
	        yyval.node = 0;
		if (cplus_mode == CPLUS_PUBLIC) {
		  if (cparse_cplusplus) {
		    const char *name = yyvsp[0].node ? Getattr(yyvsp[0].node, "name") : 0;
		    yyval.node = nested_forward_declaration(yyvsp[-5].id, yyvsp[-4].id, 0, name, yyvsp[0].node);
		  } else {
		    if (yyvsp[0].node) {
		      nested_new_struct(yyvsp[-4].id, yyvsp[-1].str, yyvsp[0].node);
		    } else {
		      Swig_warning(WARN_PARSE_UNNAMED_NESTED_CLASS, cparse_file, cparse_line, "Nested %s not currently supported (ignored).\n", yyvsp[-4].id);
		    }
		  }
		}
		Delete(yyvsp[-1].str);
	      }
break;
case 205:
#line 4692 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 206:
#line 4695 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 207:
#line 4699 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 208:
#line 4702 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 209:
#line 4703 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 210:
#line 4704 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 211:
#line 4705 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 212:
#line 4706 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 213:
#line 4707 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 214:
#line 4708 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 215:
#line 4709 "parser.y"
{ yyval.node = yyvsp[0].node; }
break;
case 216:
#line 4712 "parser.y"
{
	            Clear(scanner_ccode);
		    yyval.dtype.throws = yyvsp[-1].dtype.throws;
		    yyval.dtype.throwf = yyvsp[-1].dtype.throwf;
               }
break;
case 217:
#line 4717 "parser.y"
{ 
		    skip_balanced('{','}'); 
		    yyval.dtype.throws = yyvsp[-1].dtype.throws;
		    yyval.dtype.throwf = yyvsp[-1].dtype.throwf;
	       }
break;
case 218:
#line 4724 "parser.y"
{ 
                     Clear(scanner_ccode);
                     yyval.dtype.val = 0;
                     yyval.dtype.qualifier = yyvsp[-1].dtype.qualifier;
                     yyval.dtype.bitfield = 0;
                     yyval.dtype.throws = yyvsp[-1].dtype.throws;
                     yyval.dtype.throwf = yyvsp[-1].dtype.throwf;
                }
break;
case 219:
#line 4732 "parser.y"
{ 
                     Clear(scanner_ccode);
                     yyval.dtype.val = yyvsp[-1].dtype.val;
                     yyval.dtype.qualifier = yyvsp[-3].dtype.qualifier;
                     yyval.dtype.bitfield = 0;
                     yyval.dtype.throws = yyvsp[-3].dtype.throws; 
                     yyval.dtype.throwf = yyvsp[-3].dtype.throwf; 
               }
break;
case 220:
#line 4740 "parser.y"
{ 
                     skip_balanced('{','}');
                     yyval.dtype.val = 0;
                     yyval.dtype.qualifier = yyvsp[-1].dtype.qualifier;
                     yyval.dtype.bitfield = 0;
                     yyval.dtype.throws = yyvsp[-1].dtype.throws; 
                     yyval.dtype.throwf = yyvsp[-1].dtype.throwf; 
               }
break;
case 221:
#line 4751 "parser.y"
{ }
break;
case 222:
#line 4757 "parser.y"
{ yyval.id = "extern"; }
break;
case 223:
#line 4758 "parser.y"
{ 
                   if (strcmp(yyvsp[0].id,"C") == 0) {
		     yyval.id = "externc";
		   } else {
		     Swig_warning(WARN_PARSE_UNDEFINED_EXTERN,cparse_file, cparse_line,"Unrecognized extern type \"%s\".\n", yyvsp[0].id);
		     yyval.id = 0;
		   }
               }
break;
case 224:
#line 4766 "parser.y"
{ yyval.id = "static"; }
break;
case 225:
#line 4767 "parser.y"
{ yyval.id = "typedef"; }
break;
case 226:
#line 4768 "parser.y"
{ yyval.id = "virtual"; }
break;
case 227:
#line 4769 "parser.y"
{ yyval.id = "friend"; }
break;
case 228:
#line 4770 "parser.y"
{ yyval.id = "explicit"; }
break;
case 229:
#line 4771 "parser.y"
{ yyval.id = 0; }
break;
case 230:
#line 4778 "parser.y"
{
                 Parm *p;
		 yyval.pl = yyvsp[0].pl;
		 p = yyvsp[0].pl;
                 while (p) {
		   Replace(Getattr(p,"type"),"typename ", "", DOH_REPLACE_ANY);
		   p = nextSibling(p);
                 }
               }
break;
case 231:
#line 4789 "parser.y"
{
                  set_nextSibling(yyvsp[-1].p,yyvsp[0].pl);
                  yyval.pl = yyvsp[-1].p;
		}
break;
case 232:
#line 4793 "parser.y"
{ yyval.pl = 0; }
break;
case 233:
#line 4796 "parser.y"
{
                 set_nextSibling(yyvsp[-1].p,yyvsp[0].pl);
		 yyval.pl = yyvsp[-1].p;
                }
break;
case 234:
#line 4800 "parser.y"
{ yyval.pl = 0; }
break;
case 235:
#line 4804 "parser.y"
{
                   SwigType_push(yyvsp[-1].type,yyvsp[0].decl.type);
		   yyval.p = NewParmWithoutFileLineInfo(yyvsp[-1].type,yyvsp[0].decl.id);
		   Setfile(yyval.p,cparse_file);
		   Setline(yyval.p,cparse_line);
		   if (yyvsp[0].decl.defarg) {
		     Setattr(yyval.p,"value",yyvsp[0].decl.defarg);
		   }
		}
break;
case 236:
#line 4814 "parser.y"
{
                  yyval.p = NewParmWithoutFileLineInfo(NewStringf("template<class> %s %s", yyvsp[-2].id,yyvsp[-1].str), 0);
		  Setfile(yyval.p,cparse_file);
		  Setline(yyval.p,cparse_line);
                  if (yyvsp[0].dtype.val) {
                    Setattr(yyval.p,"value",yyvsp[0].dtype.val);
                  }
                }
break;
case 237:
#line 4822 "parser.y"
{
		  SwigType *t = NewString("v(...)");
		  yyval.p = NewParmWithoutFileLineInfo(t, 0);
		  Setfile(yyval.p,cparse_file);
		  Setline(yyval.p,cparse_line);
		}
break;
case 238:
#line 4830 "parser.y"
{
                 Parm *p;
		 yyval.p = yyvsp[0].p;
		 p = yyvsp[0].p;
                 while (p) {
		   if (Getattr(p,"type")) {
		     Replace(Getattr(p,"type"),"typename ", "", DOH_REPLACE_ANY);
		   }
		   p = nextSibling(p);
                 }
               }
break;
case 239:
#line 4843 "parser.y"
{
                  set_nextSibling(yyvsp[-1].p,yyvsp[0].p);
                  yyval.p = yyvsp[-1].p;
		}
break;
case 240:
#line 4847 "parser.y"
{ yyval.p = 0; }
break;
case 241:
#line 4850 "parser.y"
{
                 set_nextSibling(yyvsp[-1].p,yyvsp[0].p);
		 yyval.p = yyvsp[-1].p;
                }
break;
case 242:
#line 4854 "parser.y"
{ yyval.p = 0; }
break;
case 243:
#line 4858 "parser.y"
{
		  yyval.p = yyvsp[0].p;
		  {
		    /* We need to make a possible adjustment for integer parameters. */
		    SwigType *type;
		    Node     *n = 0;

		    while (!n) {
		      type = Getattr(yyvsp[0].p,"type");
		      n = Swig_symbol_clookup(type,0);     /* See if we can find a node that matches the typename */
		      if ((n) && (Strcmp(nodeType(n),"cdecl") == 0)) {
			SwigType *decl = Getattr(n,"decl");
			if (!SwigType_isfunction(decl)) {
			  String *value = Getattr(n,"value");
			  if (value) {
			    String *v = Copy(value);
			    Setattr(yyvsp[0].p,"type",v);
			    Delete(v);
			    n = 0;
			  }
			}
		      } else {
			break;
		      }
		    }
		  }

               }
break;
case 244:
#line 4886 "parser.y"
{
                  yyval.p = NewParmWithoutFileLineInfo(0,0);
                  Setfile(yyval.p,cparse_file);
		  Setline(yyval.p,cparse_line);
		  Setattr(yyval.p,"value",yyvsp[0].dtype.val);
               }
break;
case 245:
#line 4894 "parser.y"
{ 
                  yyval.dtype = yyvsp[0].dtype; 
		  if (yyvsp[0].dtype.type == T_ERROR) {
		    Swig_warning(WARN_PARSE_BAD_DEFAULT,cparse_file, cparse_line, "Can't set default argument (ignored)\n");
		    yyval.dtype.val = 0;
		    yyval.dtype.rawval = 0;
		    yyval.dtype.bitfield = 0;
		    yyval.dtype.throws = 0;
		    yyval.dtype.throwf = 0;
		  }
               }
break;
case 246:
#line 4905 "parser.y"
{ 
		  yyval.dtype = yyvsp[-3].dtype;
		  if (yyvsp[-3].dtype.type == T_ERROR) {
		    Swig_warning(WARN_PARSE_BAD_DEFAULT,cparse_file, cparse_line, "Can't set default argument (ignored)\n");
		    yyval.dtype = yyvsp[-3].dtype;
		    yyval.dtype.val = 0;
		    yyval.dtype.rawval = 0;
		    yyval.dtype.bitfield = 0;
		    yyval.dtype.throws = 0;
		    yyval.dtype.throwf = 0;
		  } else {
		    yyval.dtype.val = NewStringf("%s[%s]",yyvsp[-3].dtype.val,yyvsp[-1].dtype.val); 
		  }		  
               }
break;
case 247:
#line 4919 "parser.y"
{
		 skip_balanced('{','}');
		 yyval.dtype.val = 0;
		 yyval.dtype.rawval = 0;
                 yyval.dtype.type = T_INT;
		 yyval.dtype.bitfield = 0;
		 yyval.dtype.throws = 0;
		 yyval.dtype.throwf = 0;
	       }
break;
case 248:
#line 4928 "parser.y"
{ 
		 yyval.dtype.val = 0;
		 yyval.dtype.rawval = 0;
		 yyval.dtype.type = 0;
		 yyval.dtype.bitfield = yyvsp[0].dtype.val;
		 yyval.dtype.throws = 0;
		 yyval.dtype.throwf = 0;
	       }
break;
case 249:
#line 4936 "parser.y"
{
                 yyval.dtype.val = 0;
                 yyval.dtype.rawval = 0;
                 yyval.dtype.type = T_INT;
		 yyval.dtype.bitfield = 0;
		 yyval.dtype.throws = 0;
		 yyval.dtype.throwf = 0;
               }
break;
case 250:
#line 4946 "parser.y"
{
                 yyval.decl = yyvsp[-1].decl;
		 yyval.decl.defarg = yyvsp[0].dtype.rawval ? yyvsp[0].dtype.rawval : yyvsp[0].dtype.val;
            }
break;
case 251:
#line 4950 "parser.y"
{
              yyval.decl = yyvsp[-1].decl;
	      yyval.decl.defarg = yyvsp[0].dtype.rawval ? yyvsp[0].dtype.rawval : yyvsp[0].dtype.val;
            }
break;
case 252:
#line 4954 "parser.y"
{
   	      yyval.decl.type = 0;
              yyval.decl.id = 0;
	      yyval.decl.defarg = yyvsp[0].dtype.rawval ? yyvsp[0].dtype.rawval : yyvsp[0].dtype.val;
            }
break;
case 253:
#line 4961 "parser.y"
{
                 yyval.decl = yyvsp[0].decl;
		 if (SwigType_isfunction(yyvsp[0].decl.type)) {
		   Delete(SwigType_pop_function(yyvsp[0].decl.type));
		 } else if (SwigType_isarray(yyvsp[0].decl.type)) {
		   SwigType *ta = SwigType_pop_arrays(yyvsp[0].decl.type);
		   if (SwigType_isfunction(yyvsp[0].decl.type)) {
		     Delete(SwigType_pop_function(yyvsp[0].decl.type));
		   } else {
		     yyval.decl.parms = 0;
		   }
		   SwigType_push(yyvsp[0].decl.type,ta);
		   Delete(ta);
		 } else {
		   yyval.decl.parms = 0;
		 }
            }
break;
case 254:
#line 4978 "parser.y"
{
              yyval.decl = yyvsp[0].decl;
	      if (SwigType_isfunction(yyvsp[0].decl.type)) {
		Delete(SwigType_pop_function(yyvsp[0].decl.type));
	      } else if (SwigType_isarray(yyvsp[0].decl.type)) {
		SwigType *ta = SwigType_pop_arrays(yyvsp[0].decl.type);
		if (SwigType_isfunction(yyvsp[0].decl.type)) {
		  Delete(SwigType_pop_function(yyvsp[0].decl.type));
		} else {
		  yyval.decl.parms = 0;
		}
		SwigType_push(yyvsp[0].decl.type,ta);
		Delete(ta);
	      } else {
		yyval.decl.parms = 0;
	      }
            }
break;
case 255:
#line 4995 "parser.y"
{
   	      yyval.decl.type = 0;
              yyval.decl.id = 0;
	      yyval.decl.parms = 0;
	      }
break;
case 256:
#line 5003 "parser.y"
{
              yyval.decl = yyvsp[0].decl;
	      if (yyval.decl.type) {
		SwigType_push(yyvsp[-1].type,yyval.decl.type);
		Delete(yyval.decl.type);
	      }
	      yyval.decl.type = yyvsp[-1].type;
           }
break;
case 257:
#line 5011 "parser.y"
{
              yyval.decl = yyvsp[0].decl;
	      SwigType_add_reference(yyvsp[-2].type);
              if (yyval.decl.type) {
		SwigType_push(yyvsp[-2].type,yyval.decl.type);
		Delete(yyval.decl.type);
	      }
	      yyval.decl.type = yyvsp[-2].type;
           }
break;
case 258:
#line 5020 "parser.y"
{
              yyval.decl = yyvsp[0].decl;
	      if (!yyval.decl.type) yyval.decl.type = NewStringEmpty();
           }
break;
case 259:
#line 5024 "parser.y"
{ 
	     yyval.decl = yyvsp[0].decl;
	     yyval.decl.type = NewStringEmpty();
	     SwigType_add_reference(yyval.decl.type);
	     if (yyvsp[0].decl.type) {
	       SwigType_push(yyval.decl.type,yyvsp[0].decl.type);
	       Delete(yyvsp[0].decl.type);
	     }
           }
break;
case 260:
#line 5033 "parser.y"
{ 
	     SwigType *t = NewStringEmpty();

	     yyval.decl = yyvsp[0].decl;
	     SwigType_add_memberpointer(t,yyvsp[-2].str);
	     if (yyval.decl.type) {
	       SwigType_push(t,yyval.decl.type);
	       Delete(yyval.decl.type);
	     }
	     yyval.decl.type = t;
	     }
break;
case 261:
#line 5044 "parser.y"
{ 
	     SwigType *t = NewStringEmpty();
	     yyval.decl = yyvsp[0].decl;
	     SwigType_add_memberpointer(t,yyvsp[-2].str);
	     SwigType_push(yyvsp[-3].type,t);
	     if (yyval.decl.type) {
	       SwigType_push(yyvsp[-3].type,yyval.decl.type);
	       Delete(yyval.decl.type);
	     }
	     yyval.decl.type = yyvsp[-3].type;
	     Delete(t);
	   }
break;
case 262:
#line 5056 "parser.y"
{ 
	     yyval.decl = yyvsp[0].decl;
	     SwigType_add_memberpointer(yyvsp[-4].type,yyvsp[-3].str);
	     SwigType_add_reference(yyvsp[-4].type);
	     if (yyval.decl.type) {
	       SwigType_push(yyvsp[-4].type,yyval.decl.type);
	       Delete(yyval.decl.type);
	     }
	     yyval.decl.type = yyvsp[-4].type;
	   }
break;
case 263:
#line 5066 "parser.y"
{ 
	     SwigType *t = NewStringEmpty();
	     yyval.decl = yyvsp[0].decl;
	     SwigType_add_memberpointer(t,yyvsp[-3].str);
	     SwigType_add_reference(t);
	     if (yyval.decl.type) {
	       SwigType_push(t,yyval.decl.type);
	       Delete(yyval.decl.type);
	     } 
	     yyval.decl.type = t;
	   }
break;
case 264:
#line 5079 "parser.y"
{
                /* Note: This is non-standard C.  Template declarator is allowed to follow an identifier */
                 yyval.decl.id = Char(yyvsp[0].str);
		 yyval.decl.type = 0;
		 yyval.decl.parms = 0;
		 yyval.decl.have_parms = 0;
                  }
break;
case 265:
#line 5086 "parser.y"
{
                  yyval.decl.id = Char(NewStringf("~%s",yyvsp[0].str));
                  yyval.decl.type = 0;
                  yyval.decl.parms = 0;
                  yyval.decl.have_parms = 0;
                  }
break;
case 266:
#line 5094 "parser.y"
{
                  yyval.decl.id = Char(yyvsp[-1].str);
                  yyval.decl.type = 0;
                  yyval.decl.parms = 0;
                  yyval.decl.have_parms = 0;
                  }
break;
case 267:
#line 5110 "parser.y"
{
		    yyval.decl = yyvsp[-1].decl;
		    if (yyval.decl.type) {
		      SwigType_push(yyvsp[-2].type,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = yyvsp[-2].type;
                  }
break;
case 268:
#line 5118 "parser.y"
{
		    SwigType *t;
		    yyval.decl = yyvsp[-1].decl;
		    t = NewStringEmpty();
		    SwigType_add_memberpointer(t,yyvsp[-3].str);
		    if (yyval.decl.type) {
		      SwigType_push(t,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = t;
		    }
break;
case 269:
#line 5129 "parser.y"
{ 
		    SwigType *t;
		    yyval.decl = yyvsp[-2].decl;
		    t = NewStringEmpty();
		    SwigType_add_array(t,(char*)"");
		    if (yyval.decl.type) {
		      SwigType_push(t,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = t;
                  }
break;
case 270:
#line 5140 "parser.y"
{ 
		    SwigType *t;
		    yyval.decl = yyvsp[-3].decl;
		    t = NewStringEmpty();
		    SwigType_add_array(t,yyvsp[-1].dtype.val);
		    if (yyval.decl.type) {
		      SwigType_push(t,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = t;
                  }
break;
case 271:
#line 5151 "parser.y"
{
		    SwigType *t;
                    yyval.decl = yyvsp[-3].decl;
		    t = NewStringEmpty();
		    SwigType_add_function(t,yyvsp[-1].pl);
		    if (!yyval.decl.have_parms) {
		      yyval.decl.parms = yyvsp[-1].pl;
		      yyval.decl.have_parms = 1;
		    }
		    if (!yyval.decl.type) {
		      yyval.decl.type = t;
		    } else {
		      SwigType_push(t, yyval.decl.type);
		      Delete(yyval.decl.type);
		      yyval.decl.type = t;
		    }
		  }
break;
case 272:
#line 5170 "parser.y"
{
                /* Note: This is non-standard C.  Template declarator is allowed to follow an identifier */
                 yyval.decl.id = Char(yyvsp[0].str);
		 yyval.decl.type = 0;
		 yyval.decl.parms = 0;
		 yyval.decl.have_parms = 0;
                  }
break;
case 273:
#line 5178 "parser.y"
{
                  yyval.decl.id = Char(NewStringf("~%s",yyvsp[0].str));
                  yyval.decl.type = 0;
                  yyval.decl.parms = 0;
                  yyval.decl.have_parms = 0;
                  }
break;
case 274:
#line 5195 "parser.y"
{
		    yyval.decl = yyvsp[-1].decl;
		    if (yyval.decl.type) {
		      SwigType_push(yyvsp[-2].type,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = yyvsp[-2].type;
                  }
break;
case 275:
#line 5203 "parser.y"
{
                    yyval.decl = yyvsp[-1].decl;
		    if (!yyval.decl.type) {
		      yyval.decl.type = NewStringEmpty();
		    }
		    SwigType_add_reference(yyval.decl.type);
                  }
break;
case 276:
#line 5210 "parser.y"
{
		    SwigType *t;
		    yyval.decl = yyvsp[-1].decl;
		    t = NewStringEmpty();
		    SwigType_add_memberpointer(t,yyvsp[-3].str);
		    if (yyval.decl.type) {
		      SwigType_push(t,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = t;
		    }
break;
case 277:
#line 5221 "parser.y"
{ 
		    SwigType *t;
		    yyval.decl = yyvsp[-2].decl;
		    t = NewStringEmpty();
		    SwigType_add_array(t,(char*)"");
		    if (yyval.decl.type) {
		      SwigType_push(t,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = t;
                  }
break;
case 278:
#line 5232 "parser.y"
{ 
		    SwigType *t;
		    yyval.decl = yyvsp[-3].decl;
		    t = NewStringEmpty();
		    SwigType_add_array(t,yyvsp[-1].dtype.val);
		    if (yyval.decl.type) {
		      SwigType_push(t,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = t;
                  }
break;
case 279:
#line 5243 "parser.y"
{
		    SwigType *t;
                    yyval.decl = yyvsp[-3].decl;
		    t = NewStringEmpty();
		    SwigType_add_function(t,yyvsp[-1].pl);
		    if (!yyval.decl.have_parms) {
		      yyval.decl.parms = yyvsp[-1].pl;
		      yyval.decl.have_parms = 1;
		    }
		    if (!yyval.decl.type) {
		      yyval.decl.type = t;
		    } else {
		      SwigType_push(t, yyval.decl.type);
		      Delete(yyval.decl.type);
		      yyval.decl.type = t;
		    }
		  }
break;
case 280:
#line 5262 "parser.y"
{
		    yyval.decl.type = yyvsp[0].type;
                    yyval.decl.id = 0;
		    yyval.decl.parms = 0;
		    yyval.decl.have_parms = 0;
                  }
break;
case 281:
#line 5268 "parser.y"
{ 
                     yyval.decl = yyvsp[0].decl;
                     SwigType_push(yyvsp[-1].type,yyvsp[0].decl.type);
		     yyval.decl.type = yyvsp[-1].type;
		     Delete(yyvsp[0].decl.type);
                  }
break;
case 282:
#line 5274 "parser.y"
{
		    yyval.decl.type = yyvsp[-1].type;
		    SwigType_add_reference(yyval.decl.type);
		    yyval.decl.id = 0;
		    yyval.decl.parms = 0;
		    yyval.decl.have_parms = 0;
		  }
break;
case 283:
#line 5281 "parser.y"
{
		    yyval.decl = yyvsp[0].decl;
		    SwigType_add_reference(yyvsp[-2].type);
		    if (yyval.decl.type) {
		      SwigType_push(yyvsp[-2].type,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = yyvsp[-2].type;
                  }
break;
case 284:
#line 5290 "parser.y"
{
		    yyval.decl = yyvsp[0].decl;
                  }
break;
case 285:
#line 5293 "parser.y"
{
		    yyval.decl = yyvsp[0].decl;
		    yyval.decl.type = NewStringEmpty();
		    SwigType_add_reference(yyval.decl.type);
		    if (yyvsp[0].decl.type) {
		      SwigType_push(yyval.decl.type,yyvsp[0].decl.type);
		      Delete(yyvsp[0].decl.type);
		    }
                  }
break;
case 286:
#line 5302 "parser.y"
{ 
                    yyval.decl.id = 0;
                    yyval.decl.parms = 0;
		    yyval.decl.have_parms = 0;
                    yyval.decl.type = NewStringEmpty();
		    SwigType_add_reference(yyval.decl.type);
                  }
break;
case 287:
#line 5309 "parser.y"
{ 
		    yyval.decl.type = NewStringEmpty();
                    SwigType_add_memberpointer(yyval.decl.type,yyvsp[-1].str);
                    yyval.decl.id = 0;
                    yyval.decl.parms = 0;
		    yyval.decl.have_parms = 0;
      	          }
break;
case 288:
#line 5316 "parser.y"
{ 
		    SwigType *t = NewStringEmpty();
                    yyval.decl.type = yyvsp[-2].type;
		    yyval.decl.id = 0;
		    yyval.decl.parms = 0;
		    yyval.decl.have_parms = 0;
		    SwigType_add_memberpointer(t,yyvsp[-1].str);
		    SwigType_push(yyval.decl.type,t);
		    Delete(t);
                  }
break;
case 289:
#line 5326 "parser.y"
{ 
		    yyval.decl = yyvsp[0].decl;
		    SwigType_add_memberpointer(yyvsp[-3].type,yyvsp[-2].str);
		    if (yyval.decl.type) {
		      SwigType_push(yyvsp[-3].type,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = yyvsp[-3].type;
                  }
break;
case 290:
#line 5337 "parser.y"
{ 
		    SwigType *t;
		    yyval.decl = yyvsp[-2].decl;
		    t = NewStringEmpty();
		    SwigType_add_array(t,(char*)"");
		    if (yyval.decl.type) {
		      SwigType_push(t,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = t;
                  }
break;
case 291:
#line 5348 "parser.y"
{ 
		    SwigType *t;
		    yyval.decl = yyvsp[-3].decl;
		    t = NewStringEmpty();
		    SwigType_add_array(t,yyvsp[-1].dtype.val);
		    if (yyval.decl.type) {
		      SwigType_push(t,yyval.decl.type);
		      Delete(yyval.decl.type);
		    }
		    yyval.decl.type = t;
                  }
break;
case 292:
#line 5359 "parser.y"
{ 
		    yyval.decl.type = NewStringEmpty();
		    yyval.decl.id = 0;
		    yyval.decl.parms = 0;
		    yyval.decl.have_parms = 0;
		    SwigType_add_array(yyval.decl.type,(char*)"");
                  }
break;
case 293:
#line 5366 "parser.y"
{ 
		    yyval.decl.type = NewStringEmpty();
		    yyval.decl.id = 0;
		    yyval.decl.parms = 0;
		    yyval.decl.have_parms = 0;
		    SwigType_add_array(yyval.decl.type,yyvsp[-1].dtype.val);
		  }
break;
case 294:
#line 5373 "parser.y"
{
                    yyval.decl = yyvsp[-1].decl;
		  }
break;
case 295:
#line 5376 "parser.y"
{
		    SwigType *t;
                    yyval.decl = yyvsp[-3].decl;
		    t = NewStringEmpty();
                    SwigType_add_function(t,yyvsp[-1].pl);
		    if (!yyval.decl.type) {
		      yyval.decl.type = t;
		    } else {
		      SwigType_push(t,yyval.decl.type);
		      Delete(yyval.decl.type);
		      yyval.decl.type = t;
		    }
		    if (!yyval.decl.have_parms) {
		      yyval.decl.parms = yyvsp[-1].pl;
		      yyval.decl.have_parms = 1;
		    }
		  }
break;
case 296:
#line 5393 "parser.y"
{
                    yyval.decl.type = NewStringEmpty();
                    SwigType_add_function(yyval.decl.type,yyvsp[-1].pl);
		    yyval.decl.parms = yyvsp[-1].pl;
		    yyval.decl.have_parms = 1;
		    yyval.decl.id = 0;
                  }
break;
case 297:
#line 5403 "parser.y"
{ 
             yyval.type = NewStringEmpty();
             SwigType_add_pointer(yyval.type);
	     SwigType_push(yyval.type,yyvsp[-1].str);
	     SwigType_push(yyval.type,yyvsp[0].type);
	     Delete(yyvsp[0].type);
           }
break;
case 298:
#line 5410 "parser.y"
{
	     yyval.type = NewStringEmpty();
	     SwigType_add_pointer(yyval.type);
	     SwigType_push(yyval.type,yyvsp[0].type);
	     Delete(yyvsp[0].type);
	   }
break;
case 299:
#line 5416 "parser.y"
{ 
	     yyval.type = NewStringEmpty();
	     SwigType_add_pointer(yyval.type);
	     SwigType_push(yyval.type,yyvsp[0].str);
           }
break;
case 300:
#line 5421 "parser.y"
{
	     yyval.type = NewStringEmpty();
	     SwigType_add_pointer(yyval.type);
           }
break;
case 301:
#line 5427 "parser.y"
{
	          yyval.str = NewStringEmpty();
	          if (yyvsp[0].id) SwigType_add_qualifier(yyval.str,yyvsp[0].id);
               }
break;
case 302:
#line 5431 "parser.y"
{
		  yyval.str = yyvsp[0].str;
	          if (yyvsp[-1].id) SwigType_add_qualifier(yyval.str,yyvsp[-1].id);
               }
break;
case 303:
#line 5437 "parser.y"
{ yyval.id = "const"; }
break;
case 304:
#line 5438 "parser.y"
{ yyval.id = "volatile"; }
break;
case 305:
#line 5439 "parser.y"
{ yyval.id = 0; }
break;
case 306:
#line 5445 "parser.y"
{
                   yyval.type = yyvsp[0].type;
                   Replace(yyval.type,"typename ","", DOH_REPLACE_ANY);
                }
break;
case 307:
#line 5451 "parser.y"
{
                   yyval.type = yyvsp[0].type;
	           SwigType_push(yyval.type,yyvsp[-1].str);
               }
break;
case 308:
#line 5455 "parser.y"
{ yyval.type = yyvsp[0].type; }
break;
case 309:
#line 5456 "parser.y"
{
		  yyval.type = yyvsp[-1].type;
	          SwigType_push(yyval.type,yyvsp[0].str);
	       }
break;
case 310:
#line 5460 "parser.y"
{
		  yyval.type = yyvsp[-1].type;
	          SwigType_push(yyval.type,yyvsp[0].str);
	          SwigType_push(yyval.type,yyvsp[-2].str);
	       }
break;
case 311:
#line 5467 "parser.y"
{ yyval.type = yyvsp[0].type;
                  /* Printf(stdout,"primitive = '%s'\n", $$);*/
               }
break;
case 312:
#line 5470 "parser.y"
{ yyval.type = yyvsp[0].type; }
break;
case 313:
#line 5471 "parser.y"
{ yyval.type = yyvsp[0].type; }
break;
case 314:
#line 5472 "parser.y"
{ yyval.type = NewStringf("%s%s",yyvsp[-1].type,yyvsp[0].id); }
break;
case 315:
#line 5473 "parser.y"
{ yyval.type = NewStringf("enum %s", yyvsp[0].str); }
break;
case 316:
#line 5474 "parser.y"
{ yyval.type = yyvsp[0].type; }
break;
case 317:
#line 5476 "parser.y"
{
		  yyval.type = yyvsp[0].str;
               }
break;
case 318:
#line 5479 "parser.y"
{ 
		 yyval.type = NewStringf("%s %s", yyvsp[-1].id, yyvsp[0].str);
               }
break;
case 319:
#line 5484 "parser.y"
{
		 if (!yyvsp[0].ptype.type) yyvsp[0].ptype.type = NewString("int");
		 if (yyvsp[0].ptype.us) {
		   yyval.type = NewStringf("%s %s", yyvsp[0].ptype.us, yyvsp[0].ptype.type);
		   Delete(yyvsp[0].ptype.us);
                   Delete(yyvsp[0].ptype.type);
		 } else {
                   yyval.type = yyvsp[0].ptype.type;
		 }
		 if (Cmp(yyval.type,"signed int") == 0) {
		   Delete(yyval.type);
		   yyval.type = NewString("int");
                 } else if (Cmp(yyval.type,"signed long") == 0) {
		   Delete(yyval.type);
                   yyval.type = NewString("long");
                 } else if (Cmp(yyval.type,"signed short") == 0) {
		   Delete(yyval.type);
		   yyval.type = NewString("short");
		 } else if (Cmp(yyval.type,"signed long long") == 0) {
		   Delete(yyval.type);
		   yyval.type = NewString("long long");
		 }
               }
break;
case 320:
#line 5509 "parser.y"
{ 
                 yyval.ptype = yyvsp[0].ptype;
               }
break;
case 321:
#line 5512 "parser.y"
{
                    if (yyvsp[-1].ptype.us && yyvsp[0].ptype.us) {
		      Swig_error(cparse_file, cparse_line, "Extra %s specifier.\n", yyvsp[0].ptype.us);
		    }
                    yyval.ptype = yyvsp[0].ptype;
                    if (yyvsp[-1].ptype.us) yyval.ptype.us = yyvsp[-1].ptype.us;
		    if (yyvsp[-1].ptype.type) {
		      if (!yyvsp[0].ptype.type) yyval.ptype.type = yyvsp[-1].ptype.type;
		      else {
			int err = 0;
			if ((Cmp(yyvsp[-1].ptype.type,"long") == 0)) {
			  if ((Cmp(yyvsp[0].ptype.type,"long") == 0) || (Strncmp(yyvsp[0].ptype.type,"double",6) == 0)) {
			    yyval.ptype.type = NewStringf("long %s", yyvsp[0].ptype.type);
			  } else if (Cmp(yyvsp[0].ptype.type,"int") == 0) {
			    yyval.ptype.type = yyvsp[-1].ptype.type;
			  } else {
			    err = 1;
			  }
			} else if ((Cmp(yyvsp[-1].ptype.type,"short")) == 0) {
			  if (Cmp(yyvsp[0].ptype.type,"int") == 0) {
			    yyval.ptype.type = yyvsp[-1].ptype.type;
			  } else {
			    err = 1;
			  }
			} else if (Cmp(yyvsp[-1].ptype.type,"int") == 0) {
			  yyval.ptype.type = yyvsp[0].ptype.type;
			} else if (Cmp(yyvsp[-1].ptype.type,"double") == 0) {
			  if (Cmp(yyvsp[0].ptype.type,"long") == 0) {
			    yyval.ptype.type = NewString("long double");
			  } else if (Cmp(yyvsp[0].ptype.type,"complex") == 0) {
			    yyval.ptype.type = NewString("double complex");
			  } else {
			    err = 1;
			  }
			} else if (Cmp(yyvsp[-1].ptype.type,"float") == 0) {
			  if (Cmp(yyvsp[0].ptype.type,"complex") == 0) {
			    yyval.ptype.type = NewString("float complex");
			  } else {
			    err = 1;
			  }
			} else if (Cmp(yyvsp[-1].ptype.type,"complex") == 0) {
			  yyval.ptype.type = NewStringf("%s complex", yyvsp[0].ptype.type);
			} else {
			  err = 1;
			}
			if (err) {
			  Swig_error(cparse_file, cparse_line, "Extra %s specifier.\n", yyvsp[-1].ptype.type);
			}
		      }
		    }
               }
break;
case 322:
#line 5566 "parser.y"
{ 
		    yyval.ptype.type = NewString("int");
                    yyval.ptype.us = 0;
               }
break;
case 323:
#line 5570 "parser.y"
{ 
                    yyval.ptype.type = NewString("short");
                    yyval.ptype.us = 0;
                }
break;
case 324:
#line 5574 "parser.y"
{ 
                    yyval.ptype.type = NewString("long");
                    yyval.ptype.us = 0;
                }
break;
case 325:
#line 5578 "parser.y"
{ 
                    yyval.ptype.type = NewString("char");
                    yyval.ptype.us = 0;
                }
break;
case 326:
#line 5582 "parser.y"
{ 
                    yyval.ptype.type = NewString("wchar_t");
                    yyval.ptype.us = 0;
                }
break;
case 327:
#line 5586 "parser.y"
{ 
                    yyval.ptype.type = NewString("float");
                    yyval.ptype.us = 0;
                }
break;
case 328:
#line 5590 "parser.y"
{ 
                    yyval.ptype.type = NewString("double");
                    yyval.ptype.us = 0;
                }
break;
case 329:
#line 5594 "parser.y"
{ 
                    yyval.ptype.us = NewString("signed");
                    yyval.ptype.type = 0;
                }
break;
case 330:
#line 5598 "parser.y"
{ 
                    yyval.ptype.us = NewString("unsigned");
                    yyval.ptype.type = 0;
                }
break;
case 331:
#line 5602 "parser.y"
{ 
                    yyval.ptype.type = NewString("complex");
                    yyval.ptype.us = 0;
                }
break;
case 332:
#line 5606 "parser.y"
{ 
                    yyval.ptype.type = NewString("__int8");
                    yyval.ptype.us = 0;
                }
break;
case 333:
#line 5610 "parser.y"
{ 
                    yyval.ptype.type = NewString("__int16");
                    yyval.ptype.us = 0;
                }
break;
case 334:
#line 5614 "parser.y"
{ 
                    yyval.ptype.type = NewString("__int32");
                    yyval.ptype.us = 0;
                }
break;
case 335:
#line 5618 "parser.y"
{ 
                    yyval.ptype.type = NewString("__int64");
                    yyval.ptype.us = 0;
                }
break;
case 336:
#line 5624 "parser.y"
{ /* scanner_check_typedef(); */ }
break;
case 337:
#line 5624 "parser.y"
{
                   yyval.dtype = yyvsp[0].dtype;
		   if (yyval.dtype.type == T_STRING) {
		     yyval.dtype.rawval = NewStringf("\"%(escape)s\"",yyval.dtype.val);
		   } else if (yyval.dtype.type != T_CHAR) {
		     yyval.dtype.rawval = 0;
		   }
		   yyval.dtype.bitfield = 0;
		   yyval.dtype.throws = 0;
		   yyval.dtype.throwf = 0;
		   scanner_ignore_typedef();
                }
break;
case 338:
#line 5650 "parser.y"
{ yyval.id = yyvsp[0].id; }
break;
case 339:
#line 5651 "parser.y"
{ yyval.id = (char *) 0;}
break;
case 340:
#line 5654 "parser.y"
{ 

                  /* Ignore if there is a trailing comma in the enum list */
                  if (yyvsp[0].node) {
                    Node *leftSibling = Getattr(yyvsp[-2].node,"_last");
                    if (!leftSibling) {
                      leftSibling=yyvsp[-2].node;
                    }
                    set_nextSibling(leftSibling,yyvsp[0].node);
                    Setattr(yyvsp[-2].node,"_last",yyvsp[0].node);
                  }
		  yyval.node = yyvsp[-2].node;
               }
break;
case 341:
#line 5667 "parser.y"
{ 
                   yyval.node = yyvsp[0].node; 
                   if (yyvsp[0].node) {
                     Setattr(yyvsp[0].node,"_last",yyvsp[0].node);
                   }
               }
break;
case 342:
#line 5675 "parser.y"
{
		   SwigType *type = NewSwigType(T_INT);
		   yyval.node = new_node("enumitem");
		   Setattr(yyval.node,"name",yyvsp[0].id);
		   Setattr(yyval.node,"type",type);
		   SetFlag(yyval.node,"feature:immutable");
		   Delete(type);
		 }
break;
case 343:
#line 5683 "parser.y"
{
		   SwigType *type = NewSwigType(yyvsp[0].dtype.type == T_BOOL ? T_BOOL : (yyvsp[0].dtype.type == T_CHAR ? T_CHAR : T_INT));
		   yyval.node = new_node("enumitem");
		   Setattr(yyval.node,"name",yyvsp[-2].id);
		   Setattr(yyval.node,"type",type);
		   SetFlag(yyval.node,"feature:immutable");
		   Setattr(yyval.node,"enumvalue", yyvsp[0].dtype.val);
		   Setattr(yyval.node,"value",yyvsp[-2].id);
		   Delete(type);
                 }
break;
case 344:
#line 5693 "parser.y"
{ yyval.node = 0; }
break;
case 345:
#line 5696 "parser.y"
{
                   yyval.dtype = yyvsp[0].dtype;
		   if ((yyval.dtype.type != T_INT) && (yyval.dtype.type != T_UINT) &&
		       (yyval.dtype.type != T_LONG) && (yyval.dtype.type != T_ULONG) &&
		       (yyval.dtype.type != T_LONGLONG) && (yyval.dtype.type != T_ULONGLONG) &&
		       (yyval.dtype.type != T_SHORT) && (yyval.dtype.type != T_USHORT) &&
		       (yyval.dtype.type != T_SCHAR) && (yyval.dtype.type != T_UCHAR) &&
		       (yyval.dtype.type != T_CHAR) && (yyval.dtype.type != T_BOOL)) {
		     Swig_error(cparse_file,cparse_line,"Type error. Expecting an integral type\n");
		   }
                }
break;
case 346:
#line 5711 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 347:
#line 5712 "parser.y"
{
		 Node *n;
		 yyval.dtype.val = yyvsp[0].type;
		 yyval.dtype.type = T_INT;
		 /* Check if value is in scope */
		 n = Swig_symbol_clookup(yyvsp[0].type,0);
		 if (n) {
                   /* A band-aid for enum values used in expressions. */
                   if (Strcmp(nodeType(n),"enumitem") == 0) {
                     String *q = Swig_symbol_qualified(n);
                     if (q) {
                       yyval.dtype.val = NewStringf("%s::%s", q, Getattr(n,"name"));
                       Delete(q);
                     }
                   }
		 }
               }
break;
case 348:
#line 5731 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 349:
#line 5732 "parser.y"
{
		    yyval.dtype.val = NewString(yyvsp[0].id);
                    yyval.dtype.type = T_STRING;
               }
break;
case 350:
#line 5736 "parser.y"
{
		  SwigType_push(yyvsp[-2].type,yyvsp[-1].decl.type);
		  yyval.dtype.val = NewStringf("sizeof(%s)",SwigType_str(yyvsp[-2].type,0));
		  yyval.dtype.type = T_ULONG;
               }
break;
case 351:
#line 5741 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 352:
#line 5742 "parser.y"
{
		  yyval.dtype.val = NewString(yyvsp[0].str);
		  if (Len(yyval.dtype.val)) {
		    yyval.dtype.rawval = NewStringf("'%(escape)s'", yyval.dtype.val);
		  } else {
		    yyval.dtype.rawval = NewString("'\\0'");
		  }
		  yyval.dtype.type = T_CHAR;
		  yyval.dtype.bitfield = 0;
		  yyval.dtype.throws = 0;
		  yyval.dtype.throwf = 0;
	       }
break;
case 353:
#line 5756 "parser.y"
{
   	            yyval.dtype.val = NewStringf("(%s)",yyvsp[-1].dtype.val);
		    yyval.dtype.type = yyvsp[-1].dtype.type;
   	       }
break;
case 354:
#line 5763 "parser.y"
{
                 yyval.dtype = yyvsp[0].dtype;
		 if (yyvsp[0].dtype.type != T_STRING) {
		   switch (yyvsp[-2].dtype.type) {
		     case T_FLOAT:
		     case T_DOUBLE:
		     case T_LONGDOUBLE:
		     case T_FLTCPLX:
		     case T_DBLCPLX:
		       yyval.dtype.val = NewStringf("(%s)%s", yyvsp[-2].dtype.val, yyvsp[0].dtype.val); /* SwigType_str and decimal points don't mix! */
		       break;
		     default:
		       yyval.dtype.val = NewStringf("(%s) %s", SwigType_str(yyvsp[-2].dtype.val,0), yyvsp[0].dtype.val);
		       break;
		   }
		 }
 	       }
break;
case 355:
#line 5780 "parser.y"
{
                 yyval.dtype = yyvsp[0].dtype;
		 if (yyvsp[0].dtype.type != T_STRING) {
		   SwigType_push(yyvsp[-3].dtype.val,yyvsp[-2].type);
		   yyval.dtype.val = NewStringf("(%s) %s", SwigType_str(yyvsp[-3].dtype.val,0), yyvsp[0].dtype.val);
		 }
 	       }
break;
case 356:
#line 5787 "parser.y"
{
                 yyval.dtype = yyvsp[0].dtype;
		 if (yyvsp[0].dtype.type != T_STRING) {
		   SwigType_add_reference(yyvsp[-3].dtype.val);
		   yyval.dtype.val = NewStringf("(%s) %s", SwigType_str(yyvsp[-3].dtype.val,0), yyvsp[0].dtype.val);
		 }
 	       }
break;
case 357:
#line 5794 "parser.y"
{
                 yyval.dtype = yyvsp[0].dtype;
		 if (yyvsp[0].dtype.type != T_STRING) {
		   SwigType_push(yyvsp[-4].dtype.val,yyvsp[-3].type);
		   SwigType_add_reference(yyvsp[-4].dtype.val);
		   yyval.dtype.val = NewStringf("(%s) %s", SwigType_str(yyvsp[-4].dtype.val,0), yyvsp[0].dtype.val);
		 }
 	       }
break;
case 358:
#line 5802 "parser.y"
{
		 yyval.dtype = yyvsp[0].dtype;
                 yyval.dtype.val = NewStringf("&%s",yyvsp[0].dtype.val);
	       }
break;
case 359:
#line 5806 "parser.y"
{
		 yyval.dtype = yyvsp[0].dtype;
                 yyval.dtype.val = NewStringf("*%s",yyvsp[0].dtype.val);
	       }
break;
case 360:
#line 5812 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 361:
#line 5813 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 362:
#line 5814 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 363:
#line 5815 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 364:
#line 5816 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 365:
#line 5817 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 366:
#line 5818 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 367:
#line 5819 "parser.y"
{ yyval.dtype = yyvsp[0].dtype; }
break;
case 368:
#line 5822 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s+%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = promote(yyvsp[-2].dtype.type,yyvsp[0].dtype.type);
	       }
break;
case 369:
#line 5826 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s-%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = promote(yyvsp[-2].dtype.type,yyvsp[0].dtype.type);
	       }
break;
case 370:
#line 5830 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s*%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = promote(yyvsp[-2].dtype.type,yyvsp[0].dtype.type);
	       }
break;
case 371:
#line 5834 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s/%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = promote(yyvsp[-2].dtype.type,yyvsp[0].dtype.type);
	       }
break;
case 372:
#line 5838 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s%%%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = promote(yyvsp[-2].dtype.type,yyvsp[0].dtype.type);
	       }
break;
case 373:
#line 5842 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s&%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = promote(yyvsp[-2].dtype.type,yyvsp[0].dtype.type);
	       }
break;
case 374:
#line 5846 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s|%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = promote(yyvsp[-2].dtype.type,yyvsp[0].dtype.type);
	       }
break;
case 375:
#line 5850 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s^%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = promote(yyvsp[-2].dtype.type,yyvsp[0].dtype.type);
	       }
break;
case 376:
#line 5854 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s << %s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = promote_type(yyvsp[-2].dtype.type);
	       }
break;
case 377:
#line 5858 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s >> %s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = promote_type(yyvsp[-2].dtype.type);
	       }
break;
case 378:
#line 5862 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s&&%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
break;
case 379:
#line 5866 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s||%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
break;
case 380:
#line 5870 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s==%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
break;
case 381:
#line 5874 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s!=%s",yyvsp[-2].dtype.val,yyvsp[0].dtype.val);
		 yyval.dtype.type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
break;
case 382:
#line 5888 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s >= %s", yyvsp[-2].dtype.val, yyvsp[0].dtype.val);
		 yyval.dtype.type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
break;
case 383:
#line 5892 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s <= %s", yyvsp[-2].dtype.val, yyvsp[0].dtype.val);
		 yyval.dtype.type = cparse_cplusplus ? T_BOOL : T_INT;
	       }
break;
case 384:
#line 5896 "parser.y"
{
		 yyval.dtype.val = NewStringf("%s?%s:%s", yyvsp[-4].dtype.val, yyvsp[-2].dtype.val, yyvsp[0].dtype.val);
		 /* This may not be exactly right, but is probably good enough
		  * for the purposes of parsing constant expressions. */
		 yyval.dtype.type = promote(yyvsp[-2].dtype.type, yyvsp[0].dtype.type);
	       }
break;
case 385:
#line 5902 "parser.y"
{
		 yyval.dtype.val = NewStringf("-%s",yyvsp[0].dtype.val);
		 yyval.dtype.type = yyvsp[0].dtype.type;
	       }
break;
case 386:
#line 5906 "parser.y"
{
                 yyval.dtype.val = NewStringf("+%s",yyvsp[0].dtype.val);
		 yyval.dtype.type = yyvsp[0].dtype.type;
	       }
break;
case 387:
#line 5910 "parser.y"
{
		 yyval.dtype.val = NewStringf("~%s",yyvsp[0].dtype.val);
		 yyval.dtype.type = yyvsp[0].dtype.type;
	       }
break;
case 388:
#line 5914 "parser.y"
{
                 yyval.dtype.val = NewStringf("!%s",yyvsp[0].dtype.val);
		 yyval.dtype.type = T_INT;
	       }
break;
case 389:
#line 5918 "parser.y"
{
		 String *qty;
                 skip_balanced('(',')');
		 qty = Swig_symbol_type_qualify(yyvsp[-1].type,0);
		 if (SwigType_istemplate(qty)) {
		   String *nstr = SwigType_namestr(qty);
		   Delete(qty);
		   qty = nstr;
		 }
		 yyval.dtype.val = NewStringf("%s%s",qty,scanner_ccode);
		 Clear(scanner_ccode);
		 yyval.dtype.type = T_INT;
		 Delete(qty);
               }
break;
case 390:
#line 5934 "parser.y"
{
		 yyval.bases = yyvsp[0].bases;
               }
break;
case 391:
#line 5939 "parser.y"
{ inherit_list = 1; }
break;
case 392:
#line 5939 "parser.y"
{ yyval.bases = yyvsp[0].bases; inherit_list = 0; }
break;
case 393:
#line 5940 "parser.y"
{ yyval.bases = 0; }
break;
case 394:
#line 5943 "parser.y"
{
		   Hash *list = NewHash();
		   Node *base = yyvsp[0].node;
		   Node *name = Getattr(base,"name");
		   List *lpublic = NewList();
		   List *lprotected = NewList();
		   List *lprivate = NewList();
		   Setattr(list,"public",lpublic);
		   Setattr(list,"protected",lprotected);
		   Setattr(list,"private",lprivate);
		   Delete(lpublic);
		   Delete(lprotected);
		   Delete(lprivate);
		   Append(Getattr(list,Getattr(base,"access")),name);
	           yyval.bases = list;
               }
break;
case 395:
#line 5960 "parser.y"
{
		   Hash *list = yyvsp[-2].bases;
		   Node *base = yyvsp[0].node;
		   Node *name = Getattr(base,"name");
		   Append(Getattr(list,Getattr(base,"access")),name);
                   yyval.bases = list;
               }
break;
case 396:
#line 5969 "parser.y"
{
		 yyval.intvalue = cparse_line;
	       }
break;
case 397:
#line 5971 "parser.y"
{
		 yyval.node = NewHash();
		 Setfile(yyval.node,cparse_file);
		 Setline(yyval.node,yyvsp[-1].intvalue);
		 Setattr(yyval.node,"name",yyvsp[0].str);
		 Setfile(yyvsp[0].str,cparse_file);
		 Setline(yyvsp[0].str,yyvsp[-1].intvalue);
                 if (last_cpptype && (Strcmp(last_cpptype,"struct") != 0)) {
		   Setattr(yyval.node,"access","private");
		   Swig_warning(WARN_PARSE_NO_ACCESS, Getfile(yyval.node), Getline(yyval.node), "No access specifier given for base class '%s' (ignored).\n", SwigType_namestr(yyvsp[0].str));
                 } else {
		   Setattr(yyval.node,"access","public");
		 }
               }
break;
case 398:
#line 5985 "parser.y"
{
		 yyval.intvalue = cparse_line;
	       }
break;
case 399:
#line 5987 "parser.y"
{
		 yyval.node = NewHash();
		 Setfile(yyval.node,cparse_file);
		 Setline(yyval.node,yyvsp[-2].intvalue);
		 Setattr(yyval.node,"name",yyvsp[0].str);
		 Setfile(yyvsp[0].str,cparse_file);
		 Setline(yyvsp[0].str,yyvsp[-2].intvalue);
		 Setattr(yyval.node,"access",yyvsp[-3].id);
	         if (Strcmp(yyvsp[-3].id,"public") != 0) {
		   Swig_warning(WARN_PARSE_PRIVATE_INHERIT, Getfile(yyval.node), Getline(yyval.node), "%s inheritance from base '%s' (ignored).\n", yyvsp[-3].id, SwigType_namestr(yyvsp[0].str));
		 }
               }
break;
case 400:
#line 6001 "parser.y"
{ yyval.id = (char*)"public"; }
break;
case 401:
#line 6002 "parser.y"
{ yyval.id = (char*)"private"; }
break;
case 402:
#line 6003 "parser.y"
{ yyval.id = (char*)"protected"; }
break;
case 403:
#line 6007 "parser.y"
{ 
                   yyval.id = (char*)"class"; 
		   if (!inherit_list) last_cpptype = yyval.id;
               }
break;
case 404:
#line 6011 "parser.y"
{ 
                   yyval.id = (char *)"typename"; 
		   if (!inherit_list) last_cpptype = yyval.id;
               }
break;
case 405:
#line 6017 "parser.y"
{
                 yyval.id = yyvsp[0].id;
               }
break;
case 406:
#line 6020 "parser.y"
{ 
                   yyval.id = (char*)"struct"; 
		   if (!inherit_list) last_cpptype = yyval.id;
               }
break;
case 407:
#line 6024 "parser.y"
{
                   yyval.id = (char*)"union"; 
		   if (!inherit_list) last_cpptype = yyval.id;
               }
break;
case 410:
#line 6034 "parser.y"
{
                    yyval.dtype.qualifier = yyvsp[0].str;
                    yyval.dtype.throws = 0;
                    yyval.dtype.throwf = 0;
               }
break;
case 411:
#line 6039 "parser.y"
{
                    yyval.dtype.qualifier = 0;
                    yyval.dtype.throws = yyvsp[-1].pl;
                    yyval.dtype.throwf = NewString("1");
               }
break;
case 412:
#line 6044 "parser.y"
{
                    yyval.dtype.qualifier = yyvsp[-4].str;
                    yyval.dtype.throws = yyvsp[-1].pl;
                    yyval.dtype.throwf = NewString("1");
               }
break;
case 413:
#line 6049 "parser.y"
{ 
                    yyval.dtype.qualifier = 0; 
                    yyval.dtype.throws = 0;
                    yyval.dtype.throwf = 0;
               }
break;
case 414:
#line 6056 "parser.y"
{ 
                    Clear(scanner_ccode); 
                    yyval.decl.have_parms = 0; 
                    yyval.decl.defarg = 0; 
		    yyval.decl.throws = yyvsp[-2].dtype.throws;
		    yyval.decl.throwf = yyvsp[-2].dtype.throwf;
               }
break;
case 415:
#line 6063 "parser.y"
{ 
                    skip_balanced('{','}'); 
                    yyval.decl.have_parms = 0; 
                    yyval.decl.defarg = 0; 
                    yyval.decl.throws = yyvsp[-2].dtype.throws;
                    yyval.decl.throwf = yyvsp[-2].dtype.throwf;
               }
break;
case 416:
#line 6070 "parser.y"
{ 
                    Clear(scanner_ccode); 
                    yyval.decl.parms = yyvsp[-2].pl; 
                    yyval.decl.have_parms = 1; 
                    yyval.decl.defarg = 0; 
		    yyval.decl.throws = 0;
		    yyval.decl.throwf = 0;
               }
break;
case 417:
#line 6078 "parser.y"
{
                    skip_balanced('{','}'); 
                    yyval.decl.parms = yyvsp[-2].pl; 
                    yyval.decl.have_parms = 1; 
                    yyval.decl.defarg = 0; 
                    yyval.decl.throws = 0;
                    yyval.decl.throwf = 0;
               }
break;
case 418:
#line 6086 "parser.y"
{ 
                    yyval.decl.have_parms = 0; 
                    yyval.decl.defarg = yyvsp[-1].dtype.val; 
                    yyval.decl.throws = 0;
                    yyval.decl.throwf = 0;
               }
break;
case 423:
#line 6102 "parser.y"
{
	            skip_balanced('(',')');
                    Clear(scanner_ccode);
            	}
break;
case 424:
#line 6108 "parser.y"
{ 
                     String *s = NewStringEmpty();
                     SwigType_add_template(s,yyvsp[-1].p);
                     yyval.id = Char(s);
		     scanner_last_id(1);
                 }
break;
case 425:
#line 6114 "parser.y"
{ yyval.id = (char*)"";  }
break;
case 426:
#line 6117 "parser.y"
{ yyval.id = yyvsp[0].id; }
break;
case 427:
#line 6118 "parser.y"
{ yyval.id = yyvsp[0].id; }
break;
case 428:
#line 6121 "parser.y"
{ yyval.id = yyvsp[0].id; }
break;
case 429:
#line 6122 "parser.y"
{ yyval.id = 0; }
break;
case 430:
#line 6125 "parser.y"
{ 
                  yyval.str = 0;
		  if (!yyval.str) yyval.str = NewStringf("%s%s", yyvsp[-1].str,yyvsp[0].str);
      	          Delete(yyvsp[0].str);
               }
break;
case 431:
#line 6130 "parser.y"
{ 
		 yyval.str = NewStringf("::%s%s",yyvsp[-1].str,yyvsp[0].str);
                 Delete(yyvsp[0].str);
               }
break;
case 432:
#line 6134 "parser.y"
{
		 yyval.str = NewString(yyvsp[0].str);
   	       }
break;
case 433:
#line 6137 "parser.y"
{
		 yyval.str = NewStringf("::%s",yyvsp[0].str);
               }
break;
case 434:
#line 6140 "parser.y"
{
                 yyval.str = NewString(yyvsp[0].str);
	       }
break;
case 435:
#line 6143 "parser.y"
{
                 yyval.str = NewStringf("::%s",yyvsp[0].str);
               }
break;
case 436:
#line 6148 "parser.y"
{
                   yyval.str = NewStringf("::%s%s",yyvsp[-1].str,yyvsp[0].str);
		   Delete(yyvsp[0].str);
               }
break;
case 437:
#line 6152 "parser.y"
{
                   yyval.str = NewStringf("::%s",yyvsp[0].str);
               }
break;
case 438:
#line 6155 "parser.y"
{
                   yyval.str = NewStringf("::%s",yyvsp[0].str);
               }
break;
case 439:
#line 6162 "parser.y"
{
		 yyval.str = NewStringf("::~%s",yyvsp[0].str);
               }
break;
case 440:
#line 6168 "parser.y"
{
                  yyval.str = NewStringf("%s%s",yyvsp[-1].id,yyvsp[0].id);
		  /*		  if (Len($2)) {
		    scanner_last_id(1);
		    } */
              }
break;
case 441:
#line 6177 "parser.y"
{ 
                  yyval.str = 0;
		  if (!yyval.str) yyval.str = NewStringf("%s%s", yyvsp[-1].id,yyvsp[0].str);
      	          Delete(yyvsp[0].str);
               }
break;
case 442:
#line 6182 "parser.y"
{ 
		 yyval.str = NewStringf("::%s%s",yyvsp[-1].id,yyvsp[0].str);
                 Delete(yyvsp[0].str);
               }
break;
case 443:
#line 6186 "parser.y"
{
		 yyval.str = NewString(yyvsp[0].id);
   	       }
break;
case 444:
#line 6189 "parser.y"
{
		 yyval.str = NewStringf("::%s",yyvsp[0].id);
               }
break;
case 445:
#line 6192 "parser.y"
{
                 yyval.str = NewString(yyvsp[0].str);
	       }
break;
case 446:
#line 6195 "parser.y"
{
                 yyval.str = NewStringf("::%s",yyvsp[0].str);
               }
break;
case 447:
#line 6200 "parser.y"
{
                   yyval.str = NewStringf("::%s%s",yyvsp[-1].id,yyvsp[0].str);
		   Delete(yyvsp[0].str);
               }
break;
case 448:
#line 6204 "parser.y"
{
                   yyval.str = NewStringf("::%s",yyvsp[0].id);
               }
break;
case 449:
#line 6207 "parser.y"
{
                   yyval.str = NewStringf("::%s",yyvsp[0].str);
               }
break;
case 450:
#line 6210 "parser.y"
{
		 yyval.str = NewStringf("::~%s",yyvsp[0].id);
               }
break;
case 451:
#line 6216 "parser.y"
{ 
                   yyval.id = (char *) malloc(strlen(yyvsp[-1].id)+strlen(yyvsp[0].id)+1);
                   strcpy(yyval.id,yyvsp[-1].id);
                   strcat(yyval.id,yyvsp[0].id);
               }
break;
case 452:
#line 6221 "parser.y"
{ yyval.id = yyvsp[0].id;}
break;
case 453:
#line 6224 "parser.y"
{
		 yyval.str = NewString(yyvsp[0].id);
               }
break;
case 454:
#line 6227 "parser.y"
{
                  skip_balanced('{','}');
		  yyval.str = NewString(scanner_ccode);
               }
break;
case 455:
#line 6231 "parser.y"
{
		 yyval.str = yyvsp[0].str;
              }
break;
case 456:
#line 6236 "parser.y"
{
                  Hash *n;
                  yyval.node = NewHash();
                  n = yyvsp[-1].node;
                  while(n) {
                     String *name, *value;
                     name = Getattr(n,"name");
                     value = Getattr(n,"value");
		     if (!value) value = (String *) "1";
                     Setattr(yyval.node,name, value);
		     n = nextSibling(n);
		  }
               }
break;
case 457:
#line 6249 "parser.y"
{ yyval.node = 0; }
break;
case 458:
#line 6253 "parser.y"
{
		 yyval.node = NewHash();
		 Setattr(yyval.node,"name",yyvsp[-2].id);
		 Setattr(yyval.node,"value",yyvsp[0].id);
               }
break;
case 459:
#line 6258 "parser.y"
{
		 yyval.node = NewHash();
		 Setattr(yyval.node,"name",yyvsp[-4].id);
		 Setattr(yyval.node,"value",yyvsp[-2].id);
		 set_nextSibling(yyval.node,yyvsp[0].node);
               }
break;
case 460:
#line 6264 "parser.y"
{
                 yyval.node = NewHash();
                 Setattr(yyval.node,"name",yyvsp[0].id);
	       }
break;
case 461:
#line 6268 "parser.y"
{
                 yyval.node = NewHash();
                 Setattr(yyval.node,"name",yyvsp[-2].id);
                 set_nextSibling(yyval.node,yyvsp[0].node);
               }
break;
case 462:
#line 6273 "parser.y"
{
                 yyval.node = yyvsp[0].node;
		 Setattr(yyval.node,"name",yyvsp[-2].id);
               }
break;
case 463:
#line 6277 "parser.y"
{
                 yyval.node = yyvsp[-2].node;
		 Setattr(yyval.node,"name",yyvsp[-4].id);
		 set_nextSibling(yyval.node,yyvsp[0].node);
               }
break;
case 464:
#line 6284 "parser.y"
{
		 yyval.id = yyvsp[0].id;
               }
break;
case 465:
#line 6287 "parser.y"
{
                 yyval.id = Char(yyvsp[0].dtype.val);
               }
break;
#line 9675 "CParse/parser.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;

yyoverflow:
    yyerror("yacc stack overflow");

yyabort:
    return (1);

yyaccept:
    return (0);
}
