/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 90, 91, 92, 93, 94, 98, 99, 2000 Free Software Foundation, Inc.
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* Code for all builtin macros, initialisation of symbol table, and
   expansion of user defined macros.  */

#define COMPILING_M4
#include "m4.h"
#include "m4private.h"

extern FILE *popen ();

#include "m4regex.h"

/* Initialisation of builtin and predefined macros.  The table
   "builtin_tab" is both used for initialisation, and by the "builtin"
   builtin.  */

M4BUILTIN (m4___file__);
M4BUILTIN (m4___line__);
M4BUILTIN (m4_builtin);
M4BUILTIN (m4_changecom);
M4BUILTIN (m4_changequote);
M4BUILTIN (m4_changesyntax);
#ifdef ENABLE_CHANGEWORD
M4BUILTIN (m4_changeword);
#endif
M4BUILTIN (m4_debugmode);
M4BUILTIN (m4_debugfile);
M4BUILTIN (m4_decr);
M4BUILTIN (m4_define);
M4BUILTIN (m4_defn);
M4BUILTIN (m4_divert);
M4BUILTIN (m4_divnum);
M4BUILTIN (m4_dnl);
M4BUILTIN (m4_dumpdef);
M4BUILTIN (m4_errprint);
M4BUILTIN (m4_esyscmd);
M4BUILTIN (m4_eval);
M4BUILTIN (m4_format);
M4BUILTIN (m4_ifdef);
M4BUILTIN (m4_ifelse);
M4BUILTIN (m4_include);
M4BUILTIN (m4_incr);
M4BUILTIN (m4_index);
M4BUILTIN (m4_indir);
M4BUILTIN (m4_len);
#ifdef WITH_MODULES
M4BUILTIN (m4_loadmodule);
#endif /* WITH_MODULES */
M4BUILTIN (m4_m4exit);
M4BUILTIN (m4_m4wrap);
M4BUILTIN (m4_maketemp);
#ifdef WITH_GMP
M4BUILTIN (m4_mpeval);
#endif /* WITH_GMP */
M4BUILTIN (m4_patsubst);
M4BUILTIN (m4_popdef);
M4BUILTIN (m4_pushdef);
M4BUILTIN (m4_regexp);
M4BUILTIN (m4_shift);
M4BUILTIN (m4_sinclude);
M4BUILTIN (m4_substr);
M4BUILTIN (m4_symbols);
M4BUILTIN (m4_syncoutput);
M4BUILTIN (m4_syscmd);
M4BUILTIN (m4_sysval);
M4BUILTIN (m4_traceoff);
M4BUILTIN (m4_traceon);
M4BUILTIN (m4_translit);
M4BUILTIN (m4_undefine);
M4BUILTIN (m4_undivert);
#ifdef WITH_MODULES
M4BUILTIN (m4_unloadmodule);
#endif /* WITH_MODULES */

#undef M4BUILTIN

static builtin
builtin_tab[] =
{

  /* name		GNUext	macros	blind	function */

  { "__file__",		TRUE,	FALSE,	FALSE,	m4___file__ },
  { "__line__",		TRUE,	FALSE,	FALSE,	m4___line__ },
  { "builtin",		TRUE,	FALSE,	TRUE,	m4_builtin },
  { "changecom",	FALSE,	FALSE,	FALSE,	m4_changecom },
  { "changequote",	FALSE,	FALSE,	FALSE,	m4_changequote },
  { "changesyntax",	TRUE,	FALSE,	TRUE,	m4_changesyntax },
#ifdef ENABLE_CHANGEWORD
  { "changeword",	TRUE,	FALSE,	FALSE,	m4_changeword },
#endif
  { "debugmode",	TRUE,	FALSE,	FALSE,	m4_debugmode },
  { "debugfile",	TRUE,	FALSE,	FALSE,	m4_debugfile },
  { "decr",		FALSE,	FALSE,	TRUE,	m4_decr },
  { "define",		FALSE,	TRUE,	TRUE,	m4_define },
  { "defn",		FALSE,	FALSE,	TRUE,	m4_defn },
  { "divert",		FALSE,	FALSE,	FALSE,	m4_divert },
  { "divnum",		FALSE,	FALSE,	FALSE,	m4_divnum },
  { "dnl",		FALSE,	FALSE,	FALSE,	m4_dnl },
  { "dumpdef",		FALSE,	FALSE,	FALSE,	m4_dumpdef },
  { "errprint",		FALSE,	FALSE,	FALSE,	m4_errprint },
  { "esyscmd",		TRUE,	FALSE,	TRUE,	m4_esyscmd },
  { "eval",		FALSE,	FALSE,	TRUE,	m4_eval },
  { "format",		TRUE,	FALSE,	TRUE,	m4_format },
  { "ifdef",		FALSE,	FALSE,	TRUE,	m4_ifdef },
  { "ifelse",		FALSE,	FALSE,	TRUE,	m4_ifelse },
  { "include",		FALSE,	FALSE,	TRUE,	m4_include },
  { "incr",		FALSE,	FALSE,	TRUE,	m4_incr },
  { "index",		FALSE,	FALSE,	TRUE,	m4_index },
  { "indir",		TRUE,	FALSE,	FALSE,	m4_indir },
  { "len",		FALSE,	FALSE,	TRUE,	m4_len },
#ifdef WITH_MODULES
  { "loadmodule",	TRUE,	FALSE,	TRUE,	m4_loadmodule },
#endif /* WITH_MODULES */
  { "m4exit",		FALSE,	FALSE,	FALSE,	m4_m4exit },
  { "m4wrap",		FALSE,	FALSE,	FALSE,	m4_m4wrap },
  { "maketemp",		FALSE,	FALSE,	TRUE,	m4_maketemp },
#ifdef WITH_GMP
  { "mpeval",		TRUE,	FALSE,	TRUE,	m4_mpeval },
#endif /* WITH_GMP */
  { "patsubst",		TRUE,	FALSE,	TRUE,	m4_patsubst },
  { "popdef",		FALSE,	FALSE,	TRUE,	m4_popdef },
  { "pushdef",		FALSE,	TRUE,	TRUE,	m4_pushdef },
  { "regexp",		TRUE,	FALSE,	TRUE,	m4_regexp },
  { "shift",		FALSE,	FALSE,	FALSE,	m4_shift },
  { "sinclude",		FALSE,	FALSE,	TRUE,	m4_sinclude },
  { "substr",		FALSE,	FALSE,	TRUE,	m4_substr },
  { "symbols",		TRUE,	FALSE,	FALSE,	m4_symbols },
  { "syncoutput",	TRUE,   FALSE,  TRUE,   m4_syncoutput },
  { "syscmd",		FALSE,	FALSE,	TRUE,	m4_syscmd },
  { "sysval",		FALSE,	FALSE,	FALSE,	m4_sysval },
  { "traceoff",		FALSE,	FALSE,	FALSE,	m4_traceoff },
  { "traceon",		FALSE,	FALSE,	FALSE,	m4_traceon },
  { "translit",		FALSE,	FALSE,	TRUE,	m4_translit },
  { "undefine",		FALSE,	FALSE,	TRUE,	m4_undefine },
  { "undivert",		FALSE,	FALSE,	FALSE,	m4_undivert },
#ifdef WITH_MODULES
  { "unloadmodule",	TRUE,	FALSE,	TRUE,	m4_unloadmodule },
#endif /* WITH_MODULES */

  { 0,			FALSE,	FALSE,	FALSE,	0 },
};

static predefined const
predefined_tab[] =
{
  { "unix",	"__unix__",	"" },
  { NULL,	"__gnu__",	"" },
  { NULL,	"__m4_version__", VERSION },

#ifdef ENABLE_CHANGEWORD
  { NULL,	"__m4_changeword__", "" },
#endif /* ENABLE_CHANGEWORD */
#ifdef WITH_GMP
  { NULL,	"__m4_gmp__", "" },
#endif /* WITH_GMP */
#ifdef WITH_MODULES
  { NULL,	"__m4_modules__", "" },
#endif /* WITH_MODULES */

  { NULL,	NULL,		NULL },
};


/*------------------------------------------------------------------.
| If dynamic modules are enabled, more builtin tables can be active |
| at a time.  This implements a list of tables of builtins.	    |
`------------------------------------------------------------------*/

struct builtin_table
{
  struct builtin_table *next;
  builtin *table;
};

typedef struct builtin_table builtin_table;

static builtin_table *builtin_tables = NULL;

static void
push_builtin_table (builtin *table)
{
  builtin_table *bt;

  bt = (builtin_table *)xmalloc(sizeof(struct builtin_table));
  bt->next = builtin_tables;
  bt->table = table;
  builtin_tables = bt;
}

static int
drop_builtin_table (builtin *table)
{
  builtin_table *bt, *prev = NULL;

  /* Find the list node which holds TABLE. */
  for (bt = builtin_tables; bt != NULL; bt = bt->next)
    {
      if (bt->table == table)
	break;
      prev = bt;
    }

  if (bt == NULL)		/* No match. */
    return -1;

  if (prev == NULL)		/* Dropping head node. */
    builtin_tables = bt->next;
  else				/* Dropping non-head node. */
    prev->next = bt->next;

  xfree(bt);

  return 0;
}

/*----------------------------------------.
| Find the builtin, which lives on ADDR.  |
`----------------------------------------*/

const builtin *
find_builtin_by_addr (builtin_func *func)
{
  const builtin_table *bt;
  const builtin *bp;

  for (bt = builtin_tables; bt != NULL; bt = bt->next)
    for (bp = bt->table; bp->name != NULL; bp++)
      if (bp->func == func)
	return bp;
  return NULL;
}

/*-----------------------------------.
| Find the builtin, which has NAME.  |
`-----------------------------------*/

const builtin *
find_builtin_by_name (const char *name)
{
  const builtin_table *bt;
  const builtin *bp;

  /* This is necessary to load frozen files */
  if (builtin_tables == NULL)
    push_builtin_table (builtin_tab);

  for (bt = builtin_tables; bt != NULL; bt = bt->next)
    for (bp = bt->table; bp->name != NULL; bp++)
      if (strcmp (bp->name, name) == 0)
	return bp;
  return NULL;
}


/*-------------------------------------------------------------------------.
| Install a builtin macro with name NAME, bound to the C function given in |
| BP.  MODE is SYMBOL_INSERT or SYMBOL_PUSHDEF.  TRACED defines whether	   |
| NAME is to be traced.							   |
`-------------------------------------------------------------------------*/

void
define_builtin (const char *name, const builtin *bp, symbol_lookup mode,
		boolean traced)
{
  symbol *sym;

  sym = lookup_symbol (name, mode);
  SYMBOL_TYPE (sym) = TOKEN_FUNC;
  SYMBOL_MACRO_ARGS (sym) = bp->groks_macro_args;
  SYMBOL_BLIND_NO_ARGS (sym) = bp->blind_if_no_args;
  SYMBOL_FUNC (sym) = bp->func;
  SYMBOL_TRACED (sym) = traced;
}

/*------------------------------.
| Install a new builtin_table.  |
`------------------------------*/

void
install_builtin_table (builtin *table)
{
  const builtin *bp;
  char *string;

  push_builtin_table(table);

  for (bp = table; bp->name != NULL; bp++)
    if (!no_gnu_extensions || !bp->gnu_extension)
      {
	if (prefix_all_builtins)
	  {
	    string = (char *) xmalloc (strlen (bp->name) + 4);
	    strcpy (string, "m4_");
	    strcat (string, bp->name);
	    define_builtin (string, bp, SYMBOL_INSERT, FALSE);
	    free (string);
	  }
	else
	  define_builtin (bp->name, bp, SYMBOL_INSERT, FALSE);
      }
}

int
remove_builtin_table (builtin *table)
{
  const builtin *bp;
  char *string;

  if (drop_builtin_table(table) < 0)
    return -1;

  for (bp = table; bp->name != NULL; bp++)
    if (prefix_all_builtins)
      {
	string = (char*) xmalloc (strlen (bp->name) + 4);
	strcpy (string, "m4_");
	strcat (string, bp->name);
	lookup_symbol (string, SYMBOL_POPDEF);
	free(string);
      }
    else
	lookup_symbol (bp->name, SYMBOL_POPDEF);
      
  return 0;
}

/*-------------------------------------------------------------------------.
| Define a predefined or user-defined macro, with name NAME, and expansion |
| TEXT.  MODE destinguishes between the "define" and the "pushdef" case.   |
| It is also used from main ().						   |
`-------------------------------------------------------------------------*/

void
define_user_macro (const char *name, const char *text, symbol_lookup mode)
{
  symbol *s;

  s = lookup_symbol (name, mode);
  if (SYMBOL_TYPE (s) == TOKEN_TEXT)
    xfree (SYMBOL_TEXT (s));

  SYMBOL_TYPE (s) = TOKEN_TEXT;
  SYMBOL_TEXT (s) = xstrdup (text);
}

/*-----------------------------------------------.
| Initialise all builtin and predefined macros.	 |
`-----------------------------------------------*/

void
builtin_init (void)
{
  const predefined *pp;

  install_builtin_table (builtin_tab);

  for (pp = &predefined_tab[0]; pp->func != NULL; pp++)
    if (no_gnu_extensions)
      {
	if (pp->unix_name != NULL)
	  define_user_macro (pp->unix_name, pp->func, SYMBOL_INSERT);
      }
    else
      {
	if (pp->gnu_name != NULL)
	  define_user_macro (pp->gnu_name, pp->func, SYMBOL_INSERT);
      }
}

/*----------------------------------------------------------------------.
| Print ARGC arguments from the table ARGV to obstack OBS, separated by |
| SEP, and quoted by the current quotes, if QUOTED is TRUE.	        |
`----------------------------------------------------------------------*/

static void
dump_args (struct obstack *obs, int argc, token_data **argv,
	   const char *sep, boolean quoted)
{
  int i;
  size_t len = strlen (sep);

  for (i = 1; i < argc; i++)
    {
      if (i > 1)
	obstack_grow (obs, sep, len);

      m4_shipout_string(obs, TOKEN_DATA_TEXT (argv[i]), 0, quoted);
    }
}

/* The rest of this file is code for builtins and expansion of user
   defined macros.  All the functions for builtins have a prototype as:
   
	void m4_MACRONAME (struct obstack *obs, int argc, char *argv[]);
   
   The function are expected to leave their expansion on the obstack OBS,
   as an unfinished object.  ARGV is a table of ARGC pointers to the
   individual arguments to the macro.  Please note that in general
   argv[argc] != NULL.  */

/* The first section are macros for definining, undefining, examining,
   changing, ... other macros.  */

/*-------------------------------------------------------------------------.
| The function define_macro is common for the builtins "define",	   |
| "undefine", "pushdef" and "popdef".  ARGC and ARGV is as for the caller, |
| and MODE argument determines how the macro name is entered into the	   |
| symbol table.								   |
`-------------------------------------------------------------------------*/

static void
define_macro (int argc, token_data **argv, symbol_lookup mode)
{
  const builtin *bp;

  if (m4_bad_argc (argv[0], argc, 2, 3))
    return;

  if (TOKEN_DATA_TYPE (argv[1]) != TOKEN_TEXT)
    return;

  if (argc == 2)
    {
      define_user_macro (M4ARG (1), "", mode);
      return;
    }

  switch (TOKEN_DATA_TYPE (argv[2]))
    {
    case TOKEN_TEXT:
      define_user_macro (M4ARG (1), M4ARG (2), mode);
      break;

    case TOKEN_FUNC:
      bp = find_builtin_by_addr (TOKEN_DATA_FUNC (argv[2]));
      if (bp == NULL)
	return;
      else
	define_builtin (M4ARG (1), bp, mode, TOKEN_DATA_FUNC_TRACED (argv[2]));
      break;

    default:
      M4ERROR ((warning_status, 0,
		_("INTERNAL ERROR: Bad token data type in define_macro ()")));
      abort ();
    }
  return;
}

static void
m4_define (struct obstack *obs, int argc, token_data **argv)
{
  define_macro (argc, argv, SYMBOL_INSERT);
}

static void
m4_undefine (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;
  lookup_symbol (M4ARG (1), SYMBOL_DELETE);
}

static void
m4_pushdef (struct obstack *obs, int argc, token_data **argv)
{
  define_macro (argc, argv,  SYMBOL_PUSHDEF);
}

static void
m4_popdef (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;
  lookup_symbol (M4ARG (1), SYMBOL_POPDEF);
}


#ifdef WITH_MODULES

/*-------------------------------------.
| Loading external module at runtime.  |
`-------------------------------------*/

static void
m4_loadmodule (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  module_load (M4ARG(1), obs);
}  

static void
m4_unloadmodule (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  module_unload (M4ARG(1), obs);
}  

#endif /* WITH_MODULES */


/*---------------------.
| Conditionals of m4.  |
`---------------------*/

static void
m4_ifdef (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;
  const char *result;

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;
  s = lookup_symbol (M4ARG (1), SYMBOL_LOOKUP);

  if (s != NULL)
    result = M4ARG (2);
  else if (argc == 4)
    result = M4ARG (3);
  else
    result = NULL;

  if (result != NULL)
    obstack_grow (obs, result, strlen (result));
}

static void
m4_ifelse (struct obstack *obs, int argc, token_data **argv)
{
  const char *result;
  token_data *argv0;

  if (argc == 2)
    return;

  if (m4_bad_argc (argv[0], argc, 4, -1))
    return;
  else
    /* Diagnose excess arguments if 5, 8, 11, etc., actual arguments.  */
    m4_bad_argc (argv[0], (argc + 2) % 3, -1, 1);

  argv0 = argv[0];
  argv++;
  argc--;

  result = NULL;
  while (result == NULL)

    if (strcmp (M4ARG (0), M4ARG (1)) == 0)
      result = M4ARG (2);

    else
      switch (argc)
	{
	case 3:
	  return;

	case 4:
	case 5:
	  result = M4ARG (3);
	  break;

	default:
	  argc -= 3;
	  argv += 3;
	}

  obstack_grow (obs, result, strlen (result));
}

/*---------------------------------------------------------------------.
| The function dump_symbol () is for use by "dumpdef".  It builds up a |
| table of all defined, un-shadowed, symbols.			       |
`---------------------------------------------------------------------*/

/* The structure dump_symbol_data is used to pass the information needed
   from call to call to dump_symbol.  */

struct dump_symbol_data
{
  struct obstack *obs;		/* obstack for table */
  symbol **base;		/* base of table */
  int size;			/* size of table */
};

static void
dump_symbol (symbol *sym, struct dump_symbol_data *data)
{
  if (!SYMBOL_SHADOWED (sym) && SYMBOL_TYPE (sym) != TOKEN_VOID)
    {
      obstack_blank (data->obs, sizeof (symbol *));
      data->base = (symbol **) obstack_base (data->obs);
      data->base[data->size++] = sym;
    }
}

/*------------------------------------------------------------------------.
| qsort comparison routine, for sorting the table made in m4_dumpdef ().  |
`------------------------------------------------------------------------*/

static int
dumpdef_cmp (const voidstar s1, const voidstar s2)
{
  return strcmp (SYMBOL_NAME (* (symbol *const *) s1),
		 SYMBOL_NAME (* (symbol *const *) s2));
}

/*------------------------------------------------------------------------.
| If there are no arguments, build a sorted list of all defined,          |
| un-shadowed, symbols, otherwise, only the specified symbols.            |
`------------------------------------------------------------------------*/

static void
dump_symbols (struct dump_symbol_data *data, int argc, token_data **argv,
	      boolean complain)
{
  data->base = (symbol **) obstack_base (data->obs);
  data->size = 0;

  if (argc == 1)
    {
      hack_all_symbols (dump_symbol, (char *) data);
    }
  else
    {
      int i;
      symbol *s;

      for (i = 1; i < argc; i++)
	{
	  s = lookup_symbol (TOKEN_DATA_TEXT (argv[i]), SYMBOL_LOOKUP);
	  if (s != NULL && SYMBOL_TYPE (s) != TOKEN_VOID)
	    dump_symbol (s, data);
	  else if (complain)
	    M4ERROR ((warning_status, 0,
		      _("Undefined name %s"), TOKEN_DATA_TEXT (argv[i])));
	}
    }

  obstack_finish (data->obs);
  qsort ((char *) data->base, data->size, sizeof (symbol *), dumpdef_cmp);
}

/*-------------------------------------------------------------------------.
| Implementation of "dumpdef" itself.  It builds up a table of pointers to |
| symbols, sorts it and prints the sorted table.			   |
`-------------------------------------------------------------------------*/

static void
m4_dumpdef (struct obstack *obs, int argc, token_data **argv)
{
  struct dump_symbol_data data;
  const builtin *bp;

  data.obs = obs;
  dump_symbols (&data, argc, argv, TRUE);

  for (; data.size > 0; --data.size, data.base++)
    {
      DEBUG_PRINT1 ("%s:\t", SYMBOL_NAME (data.base[0]));

      switch (SYMBOL_TYPE (data.base[0]))
	{
	case TOKEN_TEXT:
	  if (debug_level & DEBUG_TRACE_QUOTE)
	    DEBUG_PRINT3 ("%s%s%s\n",
			  lquote.string, SYMBOL_TEXT (data.base[0]), rquote.string);
	  else
	    DEBUG_PRINT1 ("%s\n", SYMBOL_TEXT (data.base[0]));
	  break;

	case TOKEN_FUNC:
	  bp = find_builtin_by_addr (SYMBOL_FUNC (data.base[0]));
	  if (bp == NULL)
	    {
	      M4ERROR ((warning_status, 0, _("\
INTERNAL ERROR: Builtin not found in builtin table!")));
	      abort ();
	    }
	  DEBUG_PRINT1 ("<%s>\n", bp->name);
	  break;

	default:
	  M4ERROR ((warning_status, 0, _("\
INTERNAL ERROR: Bad token data type in m4_dumpdef ()")));
	  abort ();
	  break;
	}
    }
}

/*-------------------------------------------------------------------------.
| Implementation of "symbols" itself.  It builds up a table of pointers to |
| symbols, sorts it and ships out the symbols name.			   |
`-------------------------------------------------------------------------*/

static void
m4_symbols (struct obstack *obs, int argc, token_data **argv)
{
  struct dump_symbol_data data;
  struct obstack data_obs;

  obstack_init (&data_obs);
  data.obs = &data_obs;
  dump_symbols (&data, argc, argv, FALSE);

  for (; data.size > 0; --data.size, data.base++)
    {
      m4_shipout_string (obs, SYMBOL_NAME (data.base[0]), 0, TRUE);
      if (data.size > 1)
	obstack_1grow (obs, ',');
    }
  obstack_free (&data_obs, NULL);
}

/*---------------------------------------------------------------------.
| The builtin "builtin" allows calls to builtin macros, even if their  |
| definition has been overridden or shadowed.  It is thus possible to  |
| redefine builtins, and still access their original definition.  This |
| macro is not available in compatibility mode.			       |
`---------------------------------------------------------------------*/

static void
m4_builtin (struct obstack *obs, int argc, token_data **argv)
{
  const builtin *bp;
  const char *name = M4ARG (1);

  if (m4_bad_argc (argv[0], argc, 2, -1))
    return;

  bp = find_builtin_by_name (name);
  if (bp == NULL)
    M4ERROR ((warning_status, 0,
	      _("Undefined name %s"), name));
  else
    (*bp->func) (obs, argc - 1, argv + 1);
}

/*------------------------------------------------------------------------.
| The builtin "indir" allows indirect calls to macros, even if their name |
| is not a proper macro name.  It is thus possible to define macros with  |
| ill-formed names for internal use in larger macro packages.  This macro |
| is not available in compatibility mode.				  |
`------------------------------------------------------------------------*/

static void
m4_indir (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;
  const char *name = M4ARG (1);

  if (m4_bad_argc (argv[0], argc, 1, -1))
    return;

  s = lookup_symbol (name, SYMBOL_LOOKUP);
  if (s == NULL)
    M4ERROR ((warning_status, 0,
	      _("Undefined name `%s'"), name));
  else
    call_macro (s, argc - 1, argv + 1, obs);
}

/*-------------------------------------------------------------------------.
| The macro "defn" returns the quoted definition of the macro named by the |
| first argument.  If the macro is builtin, it will push a special	   |
| macro-definition token on the input stack.				   |
`-------------------------------------------------------------------------*/

static void
m4_defn (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  s = lookup_symbol (M4ARG (1), SYMBOL_LOOKUP);
  if (s == NULL)
    return;

  switch (SYMBOL_TYPE (s))
    {
    case TOKEN_TEXT:
      m4_shipout_string(obs, SYMBOL_TEXT (s), 0, TRUE);
      break;

    case TOKEN_FUNC:
      push_macro (SYMBOL_FUNC (s), SYMBOL_TRACED (s));
      break;

    case TOKEN_VOID:
      break;

    default:
      M4ERROR ((warning_status, 0,
		_("INTERNAL ERROR: Bad symbol type in m4_defn ()")));
      abort ();
    }
}

/*------------------------------------------------------------------------.
| This contains macro which implements syncoutput() which takes one arg   |
|   1, on, yes - turn on sync lines                                       |
|   0, off, no - turn off sync lines                                      |
|   everything else is silently ignored                                   |
|                                                                         |
`------------------------------------------------------------------------*/

static void
m4_syncoutput (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  if (TOKEN_DATA_TYPE (argv[1]) != TOKEN_TEXT)
    return;

  if (TOKEN_DATA_TEXT(argv[1])[0] == '0'
      || TOKEN_DATA_TEXT(argv[1])[0] == 'n'
      || (TOKEN_DATA_TEXT(argv[1])[0] == 'o'
	  && TOKEN_DATA_TEXT(argv[1])[1] == 'f'))
    sync_output = 0;
  else if (TOKEN_DATA_TEXT(argv[1])[0] == '1'
	   || TOKEN_DATA_TEXT(argv[1])[0] == 'y'
	   || (TOKEN_DATA_TEXT(argv[1])[0] == 'o'
	       && TOKEN_DATA_TEXT(argv[1])[1] == 'n'))
    sync_output = 1;
}

/*------------------------------------------------------------------------.
| This section contains macros to handle the builtins "syscmd", "esyscmd" |
| and "sysval".  "esyscmd" is GNU specific.				  |
`------------------------------------------------------------------------*/

/* Exit code from last "syscmd" command.  */
static int sysval;

static void
m4_syscmd (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  debug_flush_files ();
  sysval = system (M4ARG (1));
}

static void
m4_esyscmd (struct obstack *obs, int argc, token_data **argv)
{
  FILE *pin;
  int ch;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  debug_flush_files ();
  pin = popen (M4ARG (1), "r");
  if (pin == NULL)
    {
      M4ERROR ((warning_status, errno,
		_("Cannot open pipe to command `%s'"), M4ARG (1)));
      sysval = 0xff << 8;
    }
  else
    {
      while ((ch = getc (pin)) != EOF)
	obstack_1grow (obs, (char) ch);
      sysval = pclose (pin);
    }
}

static void
m4_sysval (struct obstack *obs, int argc, token_data **argv)
{
  m4_shipout_int (obs, (sysval >> 8) & 0xff);
}

/*-------------------------------------------------------------------------.
| This section contains the top level code for the "eval" builtin.  The	   |
| actual work is done in the function evaluate (), which lives in eval.c.  |
`-------------------------------------------------------------------------*/

typedef boolean (*eval_func) M4_PARAMS((struct obstack *obs, const char *expr, 
					const int radix, int min));

static void
do_eval (struct obstack *obs, int argc, token_data **argv, eval_func func)
{
  int radix = 10;
  int min = 1;

  if (m4_bad_argc (argv[0], argc, 2, 4))
    return;

  if (argc >= 3 && !m4_numeric_arg (argv[0], M4ARG (2), &radix))
    return;

  if (radix <= 1 || radix > 36)
    {
      M4ERROR ((warning_status, 0,
		_("Radix in eval out of range (radix = %d)"), radix));
      return;
    }

  if (argc >= 4 && !m4_numeric_arg (argv[0], M4ARG (3), &min))
    return;
  if  (min <= 0)
    {
      M4ERROR ((warning_status, 0,
		_("Negative width to eval")));
      return;
    }

  if ((*func) (obs, M4ARG (1), radix, min))
    return;
}

static void
m4_eval (struct obstack *obs, int argc, token_data **argv)
{
  do_eval(obs, argc, argv, evaluate);
}

#ifdef WITH_GMP
static void
m4_mpeval (struct obstack *obs, int argc, token_data **argv)
{
  do_eval(obs, argc, argv, mp_evaluate);
}
#endif /* WITH_GMP */

static void
m4_incr (struct obstack *obs, int argc, token_data **argv)
{
  int value;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  if (!m4_numeric_arg (argv[0], M4ARG (1), &value))
    return;

  m4_shipout_int (obs, value + 1);
}

static void
m4_decr (struct obstack *obs, int argc, token_data **argv)
{
  int value;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  if (!m4_numeric_arg (argv[0], M4ARG (1), &value))
    return;

  m4_shipout_int (obs, value - 1);
}

/* This section contains the macros "divert", "undivert" and "divnum" for
   handling diversion.  The utility functions used lives in output.c.  */

/*-----------------------------------------------------------------------.
| Divert further output to the diversion given by ARGV[1].  Out of range |
| means discard further output.						 |
`-----------------------------------------------------------------------*/

static void
m4_divert (struct obstack *obs, int argc, token_data **argv)
{
  int i = 0;

  if (m4_bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc == 2 && !m4_numeric_arg (argv[0], M4ARG (1), &i))
    return;

  make_diversion (i);
}

/*-----------------------------------------------------.
| Expand to the current diversion number, -1 if none.  |
`-----------------------------------------------------*/

static void
m4_divnum (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;
  m4_shipout_int (obs, current_diversion);
}

/*-----------------------------------------------------------------------.
| Bring back the diversion given by the argument list.  If none is	 |
| specified, bring back all diversions.  GNU specific is the option of	 |
| undiverting named files, by passing a non-numeric argument to undivert |
| ().									 |
`-----------------------------------------------------------------------*/

static void
m4_undivert (struct obstack *obs, int argc, token_data **argv)
{
  int i, file;
  FILE *fp;

  if (argc == 1)
    undivert_all ();
  else
    for (i = 1; i < argc; i++)
      {
	if (sscanf (M4ARG (i), "%d", &file) == 1)
	  insert_diversion (file);
	else if (no_gnu_extensions)
	  M4ERROR ((warning_status, 0,
		    _("Non-numeric argument to %s"),
		    TOKEN_DATA_TEXT (argv[0])));
	else
	  {
	    fp = path_search (M4ARG (i), (char **)NULL);
	    if (fp != NULL)
	      {
		insert_file (fp);
		fclose (fp);
	      }
	    else
	      M4ERROR ((warning_status, errno,
			_("Cannot undivert %s"), M4ARG (i)));
	  }
      }
}

/*-------------------------------------------------------------------.
| This section contains various macros, which does not fall into any |
| specific group.  These are "dnl", "shift", "changequote",	     |
| "changecom", "changesyntax" and "changeword".			     |
`-------------------------------------------------------------------*/

/*------------------------------------------------------------------------.
| Delete all subsequent whitespace from input.  The function skip_line () |
| lives in input.c.							  |
`------------------------------------------------------------------------*/

static void
m4_dnl (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  skip_line ();
}

/*-------------------------------------------------------------------------.
| Shift all argument one to the left, discarding the first argument.  Each |
| output argument is quoted with the current quotes.			   |
`-------------------------------------------------------------------------*/

static void
m4_shift (struct obstack *obs, int argc, token_data **argv)
{
  dump_args (obs, argc - 1, argv + 1, ",", TRUE);
}

/*--------------------------------------------------------------------------.
| Change the current quotes.  The function set_quotes () lives in input.c.  |
`--------------------------------------------------------------------------*/

static void
m4_changequote (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 1, 3))
    return;

  set_quotes ((argc >= 2) ? TOKEN_DATA_TEXT (argv[1]) : NULL,
	     (argc >= 3) ? TOKEN_DATA_TEXT (argv[2]) : NULL);
}

/*--------------------------------------------------------------------.
| Change the current comment delimiters.  The function set_comment () |
| lives in input.c.						      |
`--------------------------------------------------------------------*/

static void
m4_changecom (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 1, 3))
    return;

  if (argc == 1)
    set_comment ("", "");	/* disable comments */
  else
    set_comment (TOKEN_DATA_TEXT (argv[1]),
		(argc >= 3) ? TOKEN_DATA_TEXT (argv[2]) : NULL);
}

/*-------------------------------------------------------------------.
| Change the current input syntax.  The function set_syntax () lives |
| in input.c.  For compability reasons, this function is not called, |
| if not followed by an SYNTAX_OPEN.  Also, any changes to comment   |
| delimiters and quotes made here will be overridden by a call to    |
| `changecom' or `changequote'.					     |
`-------------------------------------------------------------------*/

/* expand_ranges () from m4_translit () are used here. */
static const char *expand_ranges M4_PARAMS((const char *s, struct obstack *obs));

static void
m4_changesyntax (struct obstack *obs, int argc, token_data **argv)
{
  int i;
  int code;

  if (m4_bad_argc (argv[0], argc, 1, -1))
    return;

  for (i = 1; i < argc; i++)
    {
      switch (*TOKEN_DATA_TEXT (argv[i])) {
      case 'I': case 'i':
	code = SYNTAX_IGNORE;
	break;
      case 'O': case 'o':
	code = SYNTAX_OTHER;
	break;
      case 'S': case 's':
	code = SYNTAX_SPACE;
	break;
      case 'W': case 'w':
	code = SYNTAX_ALPHA;
	break;
      case 'D': case 'd':
	code = SYNTAX_NUM;
	break;

      case '(':
	code = SYNTAX_OPEN;
	break;
      case ')':
	code = SYNTAX_CLOSE;
	break;
      case ',':
	code = SYNTAX_COMMA;
	break;
#if 0				/* not yet used */
      case '$':
	code = SYNTAX_DOLLAR;
	break;
#endif

      case 'L': case 'l':
	code = SYNTAX_LQUOTE;
	break;
      case 'R': case 'r':
	code = SYNTAX_RQUOTE;
	break;
      case 'B': case 'b':
	code = SYNTAX_BCOMM;
	break;
      case 'E': case 'e':
	code = SYNTAX_ECOMM;
	break;

      case 'A': case 'a':
	code = SYNTAX_ACTIVE;
	break;

      case '@':
	code = SYNTAX_ESCAPE;
	break;

      case '\0':
	code = -1;
	break;
      default:
	M4ERROR ((warning_status, 0,
		  _("Undefined syntax code %c"), *TOKEN_DATA_TEXT (argv[i])));
	code = -1;
      }

      if (code >= 0)
	  set_syntax (code, 
		      expand_ranges (TOKEN_DATA_TEXT (argv[i])+1, obs));
    }
}

#ifdef ENABLE_CHANGEWORD

/*-----------------------------------------------------------------------.
| Change the regular expression used for breaking the input into words.	 |
| The function set_word_regexp () lives in input.c.			 |
`-----------------------------------------------------------------------*/

static void
m4_changeword (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  set_word_regexp (TOKEN_DATA_TEXT (argv[1]));
}

#endif /* ENABLE_CHANGEWORD */

/* This section contains macros for inclusion of other files -- "include"
   and "sinclude".  This differs from bringing back diversions, in that
   the input is scanned before being copied to the output.  */

/*-------------------------------------------------------------------------.
| Generic include function.  Include the file given by the first argument, |
| if it exists.  Complain about inaccesible files iff SILENT is FALSE.	   |
`-------------------------------------------------------------------------*/

static void
include (int argc, token_data **argv, boolean silent)
{
  FILE *fp;
  char *name = NULL;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  fp = path_search (M4ARG (1), &name);
  if (fp == NULL)
    {
      if (!silent)
	M4ERROR ((warning_status, errno,
		  _("Cannot open %s"), M4ARG (1)));
      return;
    }

  push_file (fp, name);
  xfree (name);
}

/*------------------------------------------------.
| Include a file, complaining in case of errors.  |
`------------------------------------------------*/

static void
m4_include (struct obstack *obs, int argc, token_data **argv)
{
  include (argc, argv, FALSE);
}

/*----------------------------------.
| Include a file, ignoring errors.  |
`----------------------------------*/

static void
m4_sinclude (struct obstack *obs, int argc, token_data **argv)
{
  include (argc, argv, TRUE);
}

/* More miscellaneous builtins -- "maketemp", "errprint", "__file__" and
   "__line__".  The last two are GNU specific.  */

/*------------------------------------------------------------------.
| Use the first argument as at template for a temporary file name.  |
`------------------------------------------------------------------*/

static void
m4_maketemp (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;
  mktemp (M4ARG (1));
  m4_shipout_string (obs, M4ARG (1), 0, FALSE);
}

/*----------------------------------------.
| Print all arguments on standard error.  |
`----------------------------------------*/

static void
m4_errprint (struct obstack *obs, int argc, token_data **argv)
{
  dump_args (obs, argc, argv, " ", FALSE);
  obstack_1grow (obs, '\0');
  fprintf (stderr, "%s", (char *) obstack_finish (obs));
  fflush (stderr);
}

static void
m4___file__ (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_shipout_string (obs, current_file, 0, TRUE);
}

static void
m4___line__ (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;
  m4_shipout_int (obs, current_line);
}

/* This section contains various macros for exiting, saving input until
   EOF is seen, and tracing macro calls.  That is: "m4exit", "m4wrap",
   "traceon" and "traceoff".  */

/*-------------------------------------------------------------------------.
| Exit immediately, with exitcode specified by the first argument, 0 if no |
| arguments are present.						   |
`-------------------------------------------------------------------------*/

static void
m4_m4exit (struct obstack *obs, int argc, token_data **argv)
{
  int exit_code = 0;

  if (m4_bad_argc (argv[0], argc, 1, 2))
    return;
  if (argc == 2  && !m4_numeric_arg (argv[0], M4ARG (1), &exit_code))
    exit_code = 0;

#ifdef WITH_MODULES
  module_unload_all();
#endif /* WITH_MODULES */

  exit (exit_code);
}

/*-------------------------------------------------------------------------.
| Save the argument text until EOF has been seen, allowing for user	   |
| specified cleanup action.  GNU version saves all arguments, the standard |
| version only the first.						   |
`-------------------------------------------------------------------------*/

static void
m4_m4wrap (struct obstack *obs, int argc, token_data **argv)
{
  if (no_gnu_extensions)
    m4_shipout_string (obs, M4ARG (1), 0, FALSE);
  else
    dump_args (obs, argc, argv, " ", FALSE);
  obstack_1grow (obs, '\0');
  push_wrapup (obstack_finish (obs));
}

/* Enable tracing of all specified macros, or all, if none is specified.
   Tracing is disabled by default, when a macro is defined.  This can be
   overridden by the "t" debug flag.  */

/*-----------------------------------------------------------------------.
| Set_trace () is used by "traceon" and "traceoff" to enable and disable |
| tracing of a macro.  It disables tracing if DATA is NULL, otherwise it |
| enable tracing.							 |
`-----------------------------------------------------------------------*/

static void
set_trace (symbol *sym, const char *data)
{
  SYMBOL_TRACED (sym) = (boolean) (data != NULL);
}

static void
m4_traceon (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;
  int i;

  if (argc == 1)
    hack_all_symbols (set_trace, (char *) obs);
  else
    for (i = 1; i < argc; i++)
      {
	s = lookup_symbol (TOKEN_DATA_TEXT (argv[i]), SYMBOL_LOOKUP);
	if (s != NULL)
	  set_trace (s, (char *) obs);
	else
	  M4ERROR ((warning_status, 0,
		    _("Undefined name %s"), TOKEN_DATA_TEXT (argv[i])));
      }
}

/*------------------------------------------------------------------------.
| Disable tracing of all specified macros, or all, if none is specified.  |
`------------------------------------------------------------------------*/

static void
m4_traceoff (struct obstack *obs, int argc, token_data **argv)
{
  symbol *s;
  int i;

  if (argc == 1)
    hack_all_symbols (set_trace, NULL);
  else
    for (i = 1; i < argc; i++)
      {
	s = lookup_symbol (TOKEN_DATA_TEXT (argv[i]), SYMBOL_LOOKUP);
	if (s != NULL)
	  set_trace (s, NULL);
	else
	  M4ERROR ((warning_status, 0,
		    _("Undefined name %s"), TOKEN_DATA_TEXT (argv[i])));
      }
}

/*----------------------------------------------------------------------.
| On-the-fly control of the format of the tracing output.  It takes one |
| argument, which is a character string like given to the -d option, or |
| none in which case the debug_level is zeroed.			        |
`----------------------------------------------------------------------*/

static void
m4_debugmode (struct obstack *obs, int argc, token_data **argv)
{
  int new_debug_level;
  int change_flag;

  if (m4_bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc == 1)
    debug_level = 0;
  else
    {
      if (M4ARG (1)[0] == '+' || M4ARG (1)[0] == '-')
	{
	  change_flag = M4ARG (1)[0];
	  new_debug_level = debug_decode (M4ARG (1) + 1);
	}
      else
	{
	  change_flag = 0;
	  new_debug_level = debug_decode (M4ARG (1));
	}

      if (new_debug_level < 0)
	M4ERROR ((warning_status, 0,
		  _("Debugmode: bad debug flags: `%s'"), M4ARG (1)));
      else
	{
	  switch (change_flag)
	    {
	    case 0:
	      debug_level = new_debug_level;
	      break;

	    case '+':
	      debug_level |= new_debug_level;
	      break;

	    case '-':
	      debug_level &= ~new_debug_level;
	      break;
	    }
	}
    }
}

/*-------------------------------------------------------------------------.
| Specify the destination of the debugging output.  With one argument, the |
| argument is taken as a file name, with no arguments, revert to stderr.   |
`-------------------------------------------------------------------------*/

static void
m4_debugfile (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc == 1)
    debug_set_output (NULL);
  else if (!debug_set_output (M4ARG (1)))
    M4ERROR ((warning_status, errno,
	      _("Cannot set error file: %s"), M4ARG (1)));
}

/* This section contains text processing macros: "len", "index",
   "substr", "translit", "format", "regexp" and "patsubst".  The last
   three are GNU specific.  */

/*---------------------------------------------.
| Expand to the length of the first argument.  |
`---------------------------------------------*/

static void
m4_len (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;
  m4_shipout_int (obs, strlen (M4ARG (1)));
}

/*-------------------------------------------------------------------------.
| The macro expands to the first index of the second argument in the first |
| argument.								   |
`-------------------------------------------------------------------------*/

static void
m4_index (struct obstack *obs, int argc, token_data **argv)
{
  const char *cp, *last;
  int l1, l2, retval;

  if (m4_bad_argc (argv[0], argc, 3, 3))
    return;

  l1 = strlen (M4ARG (1));
  l2 = strlen (M4ARG (2));

  last = M4ARG (1) + l1 - l2;

  for (cp = M4ARG (1); cp <= last; cp++)
    {
      if (strncmp (cp, M4ARG (2), l2) == 0)
	break;
    }
  retval = (cp <= last) ? cp - M4ARG (1) : -1;

  m4_shipout_int (obs, retval);
}

/*-------------------------------------------------------------------------.
| The macro "substr" extracts substrings from the first argument, starting |
| from the index given by the second argument, extending for a length	   |
| given by the third argument.  If the third argument is missing, the	   |
| substring extends to the end of the first argument.			   |
`-------------------------------------------------------------------------*/

static void
m4_substr (struct obstack *obs, int argc, token_data **argv)
{
  int start, length, avail;

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;

  length = avail = strlen (M4ARG (1));
  if (!m4_numeric_arg (argv[0], M4ARG (2), &start))
    return;

  if (argc == 4 && !m4_numeric_arg (argv[0], M4ARG (3), &length))
    return;

  if (start < 0 || length <= 0 || start >= avail)
    return;

  if (start + length > avail)
    length = avail - start;
  obstack_grow (obs, M4ARG (1) + start, length);
}

/*------------------------------------------------------------------------.
| For "translit", ranges are allowed in the second and third argument.	  |
| They are expanded in the following function, and the expanded strings,  |
| without any ranges left, are used to translate the characters of the	  |
| first argument.  A single - (dash) can be included in the strings by	  |
| being the first or the last character in the string.  If the first	  |
| character in a range is after the first in the character set, the range |
| is made backwards, thus 9-0 is the string 9876543210.			  |
`------------------------------------------------------------------------*/

static const char *
expand_ranges (const char *s, struct obstack *obs)
{
  char from;
  char to;

  for (from = '\0'; *s != '\0'; from = *s++)
    {
      if (*s == '-' && from != '\0')
	{
	  to = *++s;
	  if (to == '\0')
	    {
              /* trailing dash */
              obstack_1grow (obs, '-');
              break;
	    }
	  else if (from <= to)
	    {
	      while (from++ < to)
		obstack_1grow (obs, from);
	    }
	  else
	    {
	      while (--from >= to)
		obstack_1grow (obs, from);
	    }
	}
      else
	obstack_1grow (obs, *s);
    }
  obstack_1grow (obs, '\0');
  return obstack_finish (obs);
}

/*----------------------------------------------------------------------.
| The macro "translit" translates all characters in the first argument, |
| which are present in the second argument, into the corresponding      |
| character from the third argument.  If the third argument is shorter  |
| than the second, the extra characters in the second argument, are     |
| deleted from the first (pueh).				        |
`----------------------------------------------------------------------*/

static void
m4_translit (struct obstack *obs, int argc, token_data **argv)
{
  register const char *data, *tmp;
  const char *from, *to;
  int tolen;

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;

  from = M4ARG (2);
  if (strchr (from, '-') != NULL)
    {
      from = expand_ranges (from, obs);
      if (from == NULL)
	return;
    }

  if (argc == 4)
    {
      to = M4ARG (3);
      if (strchr (to, '-') != NULL)
	{
	  to = expand_ranges (to, obs);
	  if (to == NULL)
	    return;
	}
    }
  else
    to = "";

  tolen = strlen (to);

  for (data = M4ARG (1); *data; data++)
    {
      tmp = strchr (from, *data);
      if (tmp == NULL)
	{
	  obstack_1grow (obs, *data);
	}
      else
	{
	  if (tmp - from < tolen)
	    obstack_1grow (obs, *(to + (tmp - from)));
	}
    }
}

/*----------------------------------------------------------------------.
| Frontend for printf like formatting.  The function format () lives in |
| the file format.c.						        |
`----------------------------------------------------------------------*/

static void
m4_format (struct obstack *obs, int argc, token_data **argv)
{
  format (obs, argc - 1, argv + 1);
}

/*-------------------------------------------------------------------------.
| Function to perform substitution by regular expressions.  Used by the	   |
| builtins regexp and patsubst.  The changed text is placed on the	   |
| obstack.  The substitution is REPL, with \& substituted by this part of  |
| VICTIM matched by the last whole regular expression, taken from REGS[0], |
| and \N substituted by the text matched by the Nth parenthesized	   |
| sub-expression, taken from REGS[N].					   |
`-------------------------------------------------------------------------*/

static int substitute_warned = 0;

static void
substitute (struct obstack *obs, const char *victim, const char *repl,
	    struct re_registers *regs)
{
  register unsigned int ch;

  for (;;)
    {
      while ((ch = *repl++) != '\\')
	{
	  if (ch == '\0')
	    return;
	  obstack_1grow (obs, ch);
	}

      switch ((ch = *repl++))
	{
	case '0':
	  if (!substitute_warned)
	    {
	      M4ERROR ((warning_status, 0, _("\
WARNING: \\0 will disappear, use \\& instead in replacements")));
	      substitute_warned = 1;
	    }
	  /* Fall through.  */

	case '&':
	  obstack_grow (obs, victim + regs->start[0],
			regs->end[0] - regs->start[0]);
	  break;

	case '1': case '2': case '3': case '4': case '5': case '6':
	case '7': case '8': case '9': 
	  ch -= '0';
	  if (regs->end[ch] > 0)
	    obstack_grow (obs, victim + regs->start[ch],
			  regs->end[ch] - regs->start[ch]);
	  break;

	default:
	  obstack_1grow (obs, ch);
	  break;
	}
    }
}

/*--------------------------------------------------------------------------.
| Regular expression version of index.  Given two arguments, expand to the  |
| index of the first match of the second argument (a regexp) in the first.  |
| Expand to -1 if here is no match.  Given a third argument, is changes	    |
| the expansion to this argument.					    |
`--------------------------------------------------------------------------*/

static void
m4_regexp (struct obstack *obs, int argc, token_data **argv)
{
  const char *victim;		/* first argument */
  const char *regexp;		/* regular expression */
  const char *repl;		/* replacement string */

  struct re_pattern_buffer buf;	/* compiled regular expression */
  struct re_registers regs;	/* for subexpression matches */
  const char *msg;		/* error message from re_compile_pattern */
  int startpos;			/* start position of match */
  int length;			/* length of first argument */

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;

  victim = TOKEN_DATA_TEXT (argv[1]);
  regexp = TOKEN_DATA_TEXT (argv[2]);

  buf.buffer = NULL;
  buf.allocated = 0;
  buf.fastmap = NULL;
  buf.translate = NULL;
  msg = re_compile_pattern (regexp, strlen (regexp), &buf);

  if (msg != NULL)
    {
      M4ERROR ((warning_status, 0,
		_("Bad regular expression `%s': %s"), regexp, msg));
      return;
    }

  length = strlen (victim);
  startpos = re_search (&buf, victim, length, 0, length, &regs);
  xfree (buf.buffer);

  if (startpos  == -2)
    {
      M4ERROR ((warning_status, 0,
		_("Error matching regular expression `%s'"), regexp));
      return;
    }

  if (argc == 3)
    m4_shipout_int (obs, startpos);
  else if (startpos >= 0)
    {
      repl = TOKEN_DATA_TEXT (argv[3]);
      substitute (obs, victim, repl, &regs);
    }

  return;
}

/*--------------------------------------------------------------------------.
| Substitute all matches of a regexp occuring in a string.  Each match of   |
| the second argument (a regexp) in the first argument is changed to the    |
| third argument, with \& substituted by the matched text, and \N	    |
| substituted by the text matched by the Nth parenthesized sub-expression.  |
`--------------------------------------------------------------------------*/

static void
m4_patsubst (struct obstack *obs, int argc, token_data **argv)
{
  const char *victim;		/* first argument */
  const char *regexp;		/* regular expression */

  struct re_pattern_buffer buf;	/* compiled regular expression */
  struct re_registers regs;	/* for subexpression matches */
  const char *msg;		/* error message from re_compile_pattern */
  int matchpos;			/* start position of match */
  int offset;			/* current match offset */
  int length;			/* length of first argument */

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;

  regexp = TOKEN_DATA_TEXT (argv[2]);

  buf.buffer = NULL;
  buf.allocated = 0;
  buf.fastmap = NULL;
  buf.translate = NULL;
  msg = re_compile_pattern (regexp, strlen (regexp), &buf);

  if (msg != NULL)
    {
      M4ERROR ((warning_status, 0,
		_("Bad regular expression `%s': %s"), regexp, msg));
      if (buf.buffer != NULL)
	xfree (buf.buffer);
      return;
    }

  victim = TOKEN_DATA_TEXT (argv[1]);
  length = strlen (victim);

  offset = 0;
  matchpos = 0;
  while (offset < length)
    {
      matchpos = re_search (&buf, victim, length,
			    offset, length - offset, &regs);
      if (matchpos < 0)
	{

	  /* Match failed -- either error or there is no match in the
	     rest of the string, in which case the rest of the string is
	     copied verbatim.  */

	  if (matchpos == -2)
	    M4ERROR ((warning_status, 0,
		      _("Error matching regular expression `%s'"), regexp));
	  else if (offset < length)
	    obstack_grow (obs, victim + offset, length - offset);
	  break;
	}

      /* Copy the part of the string that was skipped by re_search ().  */

      if (matchpos > offset)
	obstack_grow (obs, victim + offset, matchpos - offset);

      /* Handle the part of the string that was covered by the match.  */

      substitute (obs, victim, M4ARG (3), &regs);

      /* Update the offset to the end of the match.  If the regexp
	 matched a null string, advance offset one more, to avoid
	 infinite loops.  */

      offset = regs.end[0];
      if (regs.start[0] == regs.end[0])
	obstack_1grow (obs, victim[offset++]);
    }
  obstack_1grow (obs, '\0');

  xfree (buf.buffer);
  return;
}

/*-------------------------------------------------------------------------.
| This function handles all expansion of user defined and predefined	   |
| macros.  It is called with an obstack OBS, where the macros expansion	   |
| will be placed, as an unfinished object.  SYM points to the macro	   |
| definition, giving the expansion text.  ARGC and ARGV are the arguments, |
| as usual.								   |
`-------------------------------------------------------------------------*/

void
expand_user_macro (struct obstack *obs, symbol *sym,
		   int argc, token_data **argv)
{
  const unsigned char *text;
  int i;

  for (text = SYMBOL_TEXT (sym); *text != '\0';)
    {
      if (*text != '$')
	{
	  obstack_1grow (obs, *text);
	  text++;
	  continue;
	}
      text++;
      switch (*text)
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  if (no_gnu_extensions || !isdigit(text[1]))
	    {
	      i = *text++ - '0';
	    }
	  else
	    {
	      char *endp;
	      i = (int)strtol (text, &endp, 10);
	      text = endp;
	    }
	  if (i < argc)
	    m4_shipout_string (obs, TOKEN_DATA_TEXT (argv[i]), 0, FALSE);
	  break;

	case '#':		/* number of arguments */
	  m4_shipout_int (obs, argc - 1);
	  text++;
	  break;

	case '*':		/* all arguments */
	case '@':		/* ... same, but quoted */
	  dump_args (obs, argc, argv, ",", *text == '@');
	  text++;
	  break;

	default:
	  obstack_1grow (obs, '$');
	  break;
	}
    }
}
