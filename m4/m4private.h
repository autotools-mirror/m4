/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 1998-1999, 2004-2010, 2013 Free Software
   Foundation, Inc.

   This file is part of GNU M4.

   GNU M4 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU M4 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef M4PRIVATE_H
#define M4PRIVATE_H 1

#include <m4/m4module.h>

#include "cloexec.h"
#include "quotearg.h"
#include "xmemdup0.h"

typedef struct m4__search_path_info m4__search_path_info;
typedef struct m4__macro_arg_stacks m4__macro_arg_stacks;
typedef struct m4__symbol_chain m4__symbol_chain;

typedef enum {
  M4_SYMBOL_VOID,               /* Traced but undefined, u is invalid.  */
  M4_SYMBOL_TEXT,               /* Plain text, u.u_t is valid.  */
  M4_SYMBOL_FUNC,               /* Builtin function, u.func is valid.  */
  M4_SYMBOL_PLACEHOLDER,        /* Placeholder for unknown builtin from -R.  */
  M4_SYMBOL_COMP                /* Composite symbol, u.u_c.c is valid.  */
} m4__symbol_type;

#define BIT_TEST(flags, bit)    (((flags) & (bit)) == (bit))
#define BIT_SET(flags, bit)     ((flags) |= (bit))
#define BIT_RESET(flags, bit)   ((flags) &= ~(bit))

/* Gnulib's stdbool doesn't work with bool bitfields.  For nicer
   debugging, use bool when we know it works, but use the more
   portable unsigned int elsewhere.  */
#if __GNUC__ > 2
typedef bool bool_bitfield;
#else
typedef unsigned int bool_bitfield;
#endif /* !__GNUC__ */


/* --- CONTEXT MANAGEMENT --- */

struct m4 {
  m4_symbol_table *     symtab;
  m4_syntax_table *     syntax;
  m4_module *           modules;
  m4_hash *             namemap;

  const char *          current_file;   /* Current input file.  */
  int                   current_line;   /* Current input line.  */
  int                   output_line;    /* Current output line.  */

  FILE *        debug_file;             /* File for debugging output.  */
  m4_obstack    trace_messages;
  int           exit_status;            /* Cumulative exit status.  */
  int           current_diversion;      /* Current output diversion.  */

  /* Option flags  (set in src/main.c).  */
  size_t        nesting_limit;                  /* -L */
  int           debug_level;                    /* -d */
  size_t        max_debug_arg_length;           /* -l */
  int           regexp_syntax;                  /* -r */
  int           opt_flags;

  /* __PRIVATE__: */
  m4__search_path_info  *search_path;   /* The list of path directories. */
  m4__macro_arg_stacks  *arg_stacks;    /* Array of current argv refs.  */
  size_t                stacks_count;   /* Size of arg_stacks.  */
  size_t                expansion_level;/* Macro call nesting level.  */
};

#define M4_OPT_PREFIX_BUILTINS_BIT      (1 << 0) /* -P */
#define M4_OPT_SUPPRESS_WARN_BIT        (1 << 1) /* -Q */
#define M4_OPT_DISCARD_COMMENTS_BIT     (1 << 2) /* -c */
#define M4_OPT_INTERACTIVE_BIT          (1 << 3) /* -e */
#define M4_OPT_SYNCOUTPUT_BIT           (1 << 4) /* -s */
#define M4_OPT_POSIXLY_CORRECT_BIT      (1 << 5) /* -G/POSIXLY_CORRECT */
#define M4_OPT_FATAL_WARN_BIT           (1 << 6) /* -E once */
#define M4_OPT_WARN_EXIT_BIT            (1 << 7) /* -E twice */
#define M4_OPT_SAFER_BIT                (1 << 8) /* --safer */

/* Fast macro versions of accessor functions for public fields of m4,
   that also have an identically named function exported in m4module.h.  */
#ifdef NDEBUG
#  define m4_get_symbol_table(C)                ((C)->symtab)
#  define m4_set_symbol_table(C, V)             ((C)->symtab = (V))
#  define m4_get_syntax_table(C)                ((C)->syntax)
#  define m4_set_syntax_table(C, V)             ((C)->syntax = (V))
#  define m4_get_current_file(C)                ((C)->current_file)
#  define m4_set_current_file(C, V)             ((C)->current_file = (V))
#  define m4_get_current_line(C)                ((C)->current_line)
#  define m4_set_current_line(C, V)             ((C)->current_line = (V))
#  define m4_get_output_line(C)                 ((C)->output_line)
#  define m4_set_output_line(C, V)              ((C)->output_line = (V))
#  define m4_get_debug_file(C)                  ((C)->debug_file)
#  define m4_set_debug_file(C, V)               ((C)->debug_file = (V))
#  define m4_get_trace_messages(C)              ((C)->trace_messages)
#  define m4_set_trace_messages(C, V)           ((C)->trace_messages = (V))
#  define m4_get_exit_status(C)                 ((C)->exit_status)
#  define m4_set_exit_status(C, V)              ((C)->exit_status = (V))
#  define m4_get_current_diversion(C)           ((C)->current_diversion)
#  define m4_set_current_diversion(C, V)        ((C)->current_diversion = (V))
#  define m4_get_nesting_limit_opt(C)           ((C)->nesting_limit)
#  define m4_set_nesting_limit_opt(C, V)        ((C)->nesting_limit = (V))
#  define m4_get_debug_level_opt(C)             ((C)->debug_level)
#  define m4_set_debug_level_opt(C, V)          ((C)->debug_level = (V))
#  define m4_get_max_debug_arg_length_opt(C)    ((C)->max_debug_arg_length)
#  define m4_set_max_debug_arg_length_opt(C, V) ((C)->max_debug_arg_length=(V))
#  define m4_get_regexp_syntax_opt(C)           ((C)->regexp_syntax)
#  define m4_set_regexp_syntax_opt(C, V)        ((C)->regexp_syntax = (V))

#  define m4_get_prefix_builtins_opt(C)                                 \
                (BIT_TEST((C)->opt_flags, M4_OPT_PREFIX_BUILTINS_BIT))
#  define m4_get_suppress_warnings_opt(C)                               \
                (BIT_TEST((C)->opt_flags, M4_OPT_SUPPRESS_WARN_BIT))
#  define m4_get_discard_comments_opt(C)                                \
                (BIT_TEST((C)->opt_flags, M4_OPT_DISCARD_COMMENTS_BIT))
#  define m4_get_interactive_opt(C)                                     \
                (BIT_TEST((C)->opt_flags, M4_OPT_INTERACTIVE_BIT))
#  define m4_get_syncoutput_opt(C)                                      \
                (BIT_TEST((C)->opt_flags, M4_OPT_SYNCOUTPUT_BIT))
#  define m4_get_posixly_correct_opt(C)                                 \
                (BIT_TEST((C)->opt_flags, M4_OPT_POSIXLY_CORRECT_BIT))
#  define m4_get_fatal_warnings_opt(C)                                  \
                (BIT_TEST((C)->opt_flags, M4_OPT_FATAL_WARN_BIT))
#  define m4_get_warnings_exit_opt(C)                                   \
                (BIT_TEST((C)->opt_flags, M4_OPT_WARN_EXIT_BIT))
#  define m4_get_safer_opt(C)                                           \
                (BIT_TEST((C)->opt_flags, M4_OPT_SAFER_BIT))

/* No fast opt bit set macros, as they would need to evaluate their
   arguments more than once, which would subtly change their semantics.  */
#endif

/* Accessors for private fields of m4, which have no function version
   exported in m4module.h.  */
#define m4__get_search_path(C)                  ((C)->search_path)


/* --- BUILTIN MANAGEMENT --- */

/* Internal representation of loaded builtins.  */
struct m4__builtin
{
  /* Copied from module's BUILTIN_SYMBOL table, although builtin.flags
     can be used for additional bits beyond what is allowed for
     modules.  */
  m4_builtin builtin;
  m4_module *module;            /* Module that owns this builtin.  */
};
typedef struct m4__builtin m4__builtin;

extern void m4__set_symbol_value_builtin (m4_symbol_value *,
                                          const m4__builtin *);
extern void m4__builtin_print (m4_obstack *, const m4__builtin *, bool,
                               m4__symbol_chain **, const m4_string_pair *,
                               bool);


/* --- MODULE MANAGEMENT --- */

/* Representation of a loaded m4 module.  */
struct m4_module
{
  const char *name;             /* Name of the module.  */
  void *handle;                 /* System module handle.  */
  m4__builtin *builtins;        /* Sorted array of builtins.  */
  m4_macro *macros;		/* Unsorted array of macros.  */
  size_t builtins_len;          /* Number of builtins.  */
  m4_module *next;
};

extern m4_module *  m4__module_open (m4 *context, const char *name,
                                     m4_obstack *obs);
extern m4_module *  m4__module_find (m4 *context, const char *name);


/* --- SYMBOL TABLE MANAGEMENT --- */

struct m4_symbol
{
  bool traced;                  /* True if this symbol is traced.  */
  m4_symbol_value *value;       /* Linked list of pushdef'd values.  */
};

/* Type of a link in a symbol chain.  */
enum m4__symbol_chain_type
{
  M4__CHAIN_STR,        /* Link contains a string, u.u_s is valid.  */
  M4__CHAIN_FUNC,       /* Link contains builtin token, u.builtin is valid.  */
  M4__CHAIN_ARGV,       /* Link contains a $@ reference, u.u_a is valid.  */
  M4__CHAIN_LOC         /* Link contains m4wrap location, u.u_l is valid.  */
};

/* Composite symbols are built of a linked list of chain objects.  */
struct m4__symbol_chain
{
  m4__symbol_chain *next;               /* Pointer to next link of chain.  */
  enum m4__symbol_chain_type type;      /* Type of this link.  */
  unsigned int quote_age;               /* Quote_age of this link, or 0.  */
  union
  {
    struct
    {
      const char *str;          /* Pointer to text.  */
      size_t len;               /* Remaining length of str.  */
      size_t level;             /* Expansion level of content, or SIZE_MAX.  */
    } u_s;                      /* M4__CHAIN_STR.  */
    const m4__builtin *builtin; /* M4__CHAIN_FUNC.  */
    struct
    {
      m4_macro_args *argv;              /* Reference to earlier $@.  */
      size_t index;                     /* Argument index within argv.  */
      bool_bitfield flatten : 1;        /* True to treat builtins as text.  */
      bool_bitfield comma : 1;          /* True when `,' is next input.  */
      bool_bitfield skip_last : 1;      /* True if last argument omitted.  */
      bool_bitfield has_func : 1;       /* True if argv includes func.  */
      const m4_string_pair *quotes;     /* NULL for $*, quotes for $@.  */
    } u_a;                      /* M4__CHAIN_ARGV.  */
    struct
    {
      const char *file; /* File where subsequent links originate.  */
      int line;         /* Line where subsequent links originate.  */
    } u_l;                      /* M4__CHAIN_LOC.  */
  } u;
};

/* A symbol value is used both for values associated with a macro
   name, and for arguments to a macro invocation.  */
struct m4_symbol_value
{
  m4_symbol_value *     next;
  m4_module *           module;
  unsigned int          flags;

  m4_hash *             arg_signature;
  size_t                min_args;
  size_t                max_args;
  size_t                pending_expansions;

  m4__symbol_type       type;
  union
  {
    struct
    {
      size_t            len;    /* Length of string.  */
      const char *      text;   /* String contents.  */
      /* Quote age when this string was built, or zero to force a
         rescan of the string.  Ignored for 0 len.  */
      unsigned int      quote_age;
    } u_t;                      /* Valid when type is TEXT, PLACEHOLDER.  */
    const m4__builtin * builtin;/* Valid when type is FUNC.  */
    struct
    {
      m4__symbol_chain *chain;          /* First link of the chain.  */
      m4__symbol_chain *end;            /* Last link of the chain.  */
      bool_bitfield wrapper : 1;        /* True if this is a $@ ref.  */
      bool_bitfield has_func : 1;       /* True if chain includes func.  */
    } u_c;                      /* Valid when type is COMP.  */
  } u;
};

/* Structure describing all arguments to a macro, including the macro
   name at index 0.  */
struct m4_macro_args
{
  /* One more than the highest actual argument.  May be larger than
     arraylen since the array can refer to multiple arguments via a
     single $@ reference.  */
  size_t argc;
  /* False unless the macro expansion refers to $@; determines whether
     this object can be freed at end of macro expansion or must wait
     until all references have been rescanned.  */
  bool_bitfield inuse : 1;
  /* False if all arguments are just text or func, true if this argv
     refers to another one.  */
  bool_bitfield wrapper : 1;
  /* False if all arguments belong to this argv, true if some of them
     include references to another.  */
  bool_bitfield has_ref : 1;
  /* True to flatten builtins contained in references.  */
  bool_bitfield flatten : 1;
  /* True if any token contains builtins.  */
  bool_bitfield has_func : 1;
  /* The value of quote_age for all tokens, or 0 if quote_age changed
     during parsing or any token is potentially unsafe and requires a
     rescan.  */
  unsigned int quote_age;
  /* The context of the macro call during expansion, and NULL in a
     back-reference.  */
  m4_call_info *info;
  size_t level; /* Which obstack owns this argv.  */
  size_t arraylen; /* True length of allocated elements in array.  */
  /* Used as a variable-length array, storing information about each
     argument.  */
  m4_symbol_value *array[FLEXIBLE_ARRAY_MEMBER];
};

/* Internal structure for managing multiple argv references.  See
   macro.c for a much more detailed comment on usage.  */
struct m4__macro_arg_stacks
{
  size_t refcount;      /* Number of active $@ references at this level.  */
  size_t argcount;      /* Number of argv at this level.  */
  m4_obstack *args;     /* Content of arguments.  */
  m4_obstack *argv;     /* Argv pointers into args.  */
  void *args_base;      /* Location for clearing the args obstack.  */
  void *argv_base;      /* Location for clearing the argv obstack.  */
};

/* Opaque structure for managing call context information.  Contains
   the context used in tracing and error messages that was valid at
   the start of the macro expansion, even if argument collection
   changes global context in the meantime.  */
struct m4_call_info
{
  const char *file;     /* The file containing the macro invocation.  */
  int line;             /* The line the macro was called on.  */
  size_t call_id;       /* The unique sequence call id of the macro.  */
  int trace : 1;        /* True to trace this macro.  */
  int debug_level : 31; /* The debug level for tracing the macro call.  */
  const char *name;     /* The macro name.  */
  size_t name_len;      /* The length of name.  */
};

extern size_t   m4__adjust_refcount     (m4 *, size_t, bool);
extern bool     m4__arg_adjust_refcount (m4 *, m4_macro_args *, bool);
extern void     m4__push_arg_quote      (m4 *, m4_obstack *, m4_macro_args *,
                                         size_t, const m4_string_pair *);
extern bool     m4__arg_print           (m4 *, m4_obstack *, m4_macro_args *,
                                         size_t, const m4_string_pair *, bool,
                                         m4__symbol_chain **, const char *,
                                         size_t *, bool, bool);

#define VALUE_NEXT(T)           ((T)->next)
#define VALUE_MODULE(T)         ((T)->module)
#define VALUE_FLAGS(T)          ((T)->flags)
#define VALUE_ARG_SIGNATURE(T)  ((T)->arg_signature)
#define VALUE_MIN_ARGS(T)       ((T)->min_args)
#define VALUE_MAX_ARGS(T)       ((T)->max_args)
#define VALUE_PENDING(T)        ((T)->pending_expansions)

#define SYMBOL_NEXT(S)          (VALUE_NEXT             ((S)->value))
#define SYMBOL_MODULE(S)        (VALUE_MODULE           ((S)->value))
#define SYMBOL_FLAGS(S)         (VALUE_FLAGS            ((S)->value))
#define SYMBOL_ARG_SIGNATURE(S) (VALUE_ARG_SIGNATURE    ((S)->value))
#define SYMBOL_MIN_ARGS(S)      (VALUE_MIN_ARGS         ((S)->value))
#define SYMBOL_MAX_ARGS(S)      (VALUE_MAX_ARGS         ((S)->value))
#define SYMBOL_PENDING(S)       (VALUE_PENDING          ((S)->value))

/* Fast macro versions of symbol table accessor functions,
   that also have an identically named function exported in m4module.h.  */
#ifdef NDEBUG
#  define m4_get_symbol_traced(S)       ((S)->traced)
#  define m4_get_symbol_value(S)        ((S)->value)
#  define m4_set_symbol_value(S, V)     ((S)->value = (V))

/* m4_symbol_value_{create,delete} are too complex for a simple macro.  */

#  define m4_is_symbol_value_text(V)    ((V)->type == M4_SYMBOL_TEXT)
#  define m4_is_symbol_value_func(V)    ((V)->type == M4_SYMBOL_FUNC)
#  define m4_is_symbol_value_void(V)    ((V)->type == M4_SYMBOL_VOID)
#  define m4_is_symbol_value_placeholder(V)                             \
                                        ((V)->type == M4_SYMBOL_PLACEHOLDER)
#  define m4_get_symbol_value_text(V)   ((V)->u.u_t.text)
#  define m4_get_symbol_value_len(V)    ((V)->u.u_t.len)
#  define m4_get_symbol_value_quote_age(V)      ((V)->u.u_t.quote_age)
#  define m4_get_symbol_value_func(V)   ((V)->u.builtin->builtin.func)
#  define m4_get_symbol_value_builtin(V) (&(V)->u.builtin->builtin)
#  define m4_get_symbol_value_placeholder(V)                            \
                                        ((V)->u.u_t.text)
#  define m4_symbol_value_flatten_args(V)                               \
  (BIT_TEST ((V)->flags, VALUE_FLATTEN_ARGS_BIT))

#  define m4_set_symbol_value_text(V, T, L, A)                          \
  ((V)->type = M4_SYMBOL_TEXT, (V)->u.u_t.text = (T),                   \
   (V)->u.u_t.len = (L), (V)->u.u_t.quote_age = (A))
#  define m4_set_symbol_value_placeholder(V, T)                         \
  ((V)->type = M4_SYMBOL_PLACEHOLDER, (V)->u.u_t.text = (T))
#  define m4__set_symbol_value_builtin(V, B)                            \
  ((V)->type = M4_SYMBOL_FUNC, (V)->u.builtin = (B),                    \
   VALUE_MODULE (V) = (B)->module,                                      \
   VALUE_FLAGS (V) = (B)->builtin.flags,                                \
   VALUE_MIN_ARGS (V) = (B)->builtin.min_args,                          \
   VALUE_MAX_ARGS (V) = (B)->builtin.max_args)
#endif



/* m4_symbol_value.flags bit masks.  Be sure these are a consistent
   superset of the M4_BUILTIN_* bit masks, so we can copy
   m4_builtin.flags to m4_symbol_arg.flags.  We can use additional
   bits for private use.  */

#define VALUE_FLATTEN_ARGS_BIT          (1 << 0)
#define VALUE_BLIND_ARGS_BIT            (1 << 1)
#define VALUE_SIDE_EFFECT_ARGS_BIT      (1 << 2)
#define VALUE_DELETED_BIT               (1 << 3)


struct m4_symbol_arg {
  int           index;
  int           flags;
  char *        default_val;
};

#define SYMBOL_ARG_INDEX(A)     ((A)->index)
#define SYMBOL_ARG_FLAGS(A)     ((A)->flags)
#define SYMBOL_ARG_DEFAULT(A)   ((A)->default_val)

/* m4_symbol_arg.flags bit masks:  */

#define SYMBOL_ARG_REST_BIT     (1 << 0)
#define SYMBOL_ARG_KEY_BIT      (1 << 1)

extern void m4__symtab_remove_module_references (m4_symbol_table *,
                                                 m4_module *);
extern bool m4__symbol_value_print (m4 *, m4_symbol_value *, m4_obstack *,
                                    const m4_string_pair *, bool,
                                    m4__symbol_chain **, size_t *, bool);



/* --- SYNTAX TABLE MANAGEMENT --- */

/* CHAR_RETRY must be last, because we size the syntax table to hold
   all other characters and sentinels. */
#define CHAR_EOF        (UCHAR_MAX + 1) /* Return on EOF.  */
#define CHAR_BUILTIN    (UCHAR_MAX + 2) /* Return for BUILTIN token.  */
#define CHAR_QUOTE      (UCHAR_MAX + 3) /* Return for quoted string.  */
#define CHAR_ARGV       (UCHAR_MAX + 4) /* Return for $@ reference.  */
#define CHAR_RETRY      (UCHAR_MAX + 5) /* Return for end of input block.  */

#define DEF_LQUOTE      "`"     /* Default left quote delimiter.  */
#define DEF_RQUOTE      "\'"    /* Default right quote delimiter.  */
#define DEF_BCOMM       "#"     /* Default begin comment delimiter.  */
#define DEF_ECOMM       "\n"    /* Default end comment delimiter.  */

struct m4_syntax_table {
  /* Please read the comment at the top of input.c for details.  table
     holds the current syntax, and orig holds the default syntax.  */
  unsigned short table[CHAR_RETRY];
  unsigned short orig[CHAR_RETRY];

  m4_string_pair quote; /* Quote delimiters.  */
  m4_string_pair comm;  /* Comment delimiters.  */

  char dollar; /* Dollar character, if is_single_dollar.  */

  /* True iff only one start and end quote delimiter exist.  */
  bool_bitfield is_single_quotes : 1;

  /* True iff only one start and end comment delimiter exist.  */
  bool_bitfield is_single_comments : 1;

  /* True iff only one byte has M4_SYNTAX_DOLLAR.  */
  bool_bitfield is_single_dollar : 1;

  /* True iff some character has M4_SYNTAX_ESCAPE.  */
  bool_bitfield is_macro_escaped : 1;

  /* True iff a changesyntax call has impacted something that requires
     cleanup at the end.  */
  bool_bitfield suspect : 1;

  /* Track the number of changesyntax calls.  This saturates at
     0xffff, so the idea is that most users won't be changing the
     syntax that frequently; perhaps in the future we will cache
     frequently used syntax schemes by index.  */
  unsigned short syntax_age;

  /* Track the current quote age, determined by all significant
     changequote, changecom, and changesyntax calls, since any of
     these can alter the rescan of a prior parameter in a quoted
     context.  */
  unsigned int quote_age;

  /* Track a cached quote pair on the input obstack.  */
  m4_string_pair *cached_quote;

  /* Storage for a simple cached quote that can be recreated on the fly.  */
  char cached_lquote[2];
  char cached_rquote[2];
  m4_string_pair cached_simple;
};

/* Fast macro versions of syntax table accessor functions,
   that also have an identically named function exported in m4module.h.  */
#ifdef NDEBUG
#  define m4_get_syntax_lquote(S)               ((S)->quote.str1)
#  define m4_get_syntax_rquote(S)               ((S)->quote.str2)
#  define m4_get_syntax_bcomm(S)                ((S)->comm.str1)
#  define m4_get_syntax_ecomm(S)                ((S)->comm.str2)
#  define m4_get_syntax_quotes(S)               (&(S)->quote)
#  define m4_get_syntax_comments(S)             (&(S)->comm)

#  define m4_is_syntax_single_quotes(S)         ((S)->is_single_quotes)
#  define m4_is_syntax_single_comments(S)       ((S)->is_single_comments)
#  define m4_is_syntax_single_dollar(S)         ((S)->is_single_dollar)
#  define m4_is_syntax_macro_escaped(S)         ((S)->is_macro_escaped)
#endif

/* Return the current quote age.  */
#define m4__quote_age(S)                ((S)->quote_age)

/* Return true if the current quote age guarantees that parsing the
   current token in the context of a quoted string of the same quote
   age will give the same parse.  */
#define m4__safe_quotes(S)              (((S)->quote_age & 0xffff) != 0)

/* Set or refresh the cached quote.  */
extern const m4_string_pair *m4__quote_cache (m4_syntax_table *,
                                              m4_obstack *obs, unsigned int,
                                              const m4_string_pair *);

/* Clear the cached quote.  */
#define m4__quote_uncache(S)            ((S)->cached_quote = NULL)


/* --- MACRO MANAGEMENT --- */

/* Various different token types.  */
typedef enum {
  M4_TOKEN_EOF,         /* End of file, M4_SYMBOL_VOID.  */
  M4_TOKEN_NONE,        /* Discardable token, M4_SYMBOL_VOID.  */
  M4_TOKEN_STRING,      /* Quoted string, M4_SYMBOL_TEXT or M4_SYMBOL_COMP.  */
  M4_TOKEN_COMMENT,     /* Comment, M4_SYMBOL_TEXT or M4_SYMBOL_COMP.  */
  M4_TOKEN_SPACE,       /* Whitespace, M4_SYMBOL_TEXT.  */
  M4_TOKEN_WORD,        /* An identifier, M4_SYMBOL_TEXT.  */
  M4_TOKEN_OPEN,        /* Argument list start, M4_SYMBOL_TEXT.  */
  M4_TOKEN_COMMA,       /* Argument separator, M4_SYMBOL_TEXT.  */
  M4_TOKEN_CLOSE,       /* Argument list end, M4_SYMBOL_TEXT.  */
  M4_TOKEN_SIMPLE,      /* Single character, M4_SYMBOL_TEXT.  */
  M4_TOKEN_MACDEF,      /* Builtin token, M4_SYMBOL_FUNC or M4_SYMBOL_COMP.  */
  M4_TOKEN_ARGV         /* A series of parameters, M4_SYMBOL_COMP.  */
} m4__token_type;

extern  void            m4__make_text_link (m4_obstack *, m4__symbol_chain **,
                                            m4__symbol_chain **);
extern  void            m4__append_builtin (m4_obstack *, const m4__builtin *,
                                            m4__symbol_chain **,
                                            m4__symbol_chain **);
extern  bool            m4__push_symbol (m4 *, m4_symbol_value *, size_t,
                                         bool);
extern  m4_obstack      *m4__push_wrapup_init (m4 *, const m4_call_info *,
                                               m4__symbol_chain ***);
extern  void            m4__push_wrapup_finish (void);
extern  m4__token_type  m4__next_token (m4 *, m4_symbol_value *, int *,
                                        m4_obstack *, bool,
                                        const m4_call_info *);
extern  bool            m4__next_token_is_open (m4 *);

/* Fast macro versions of macro argv accessor functions,
   that also have an identically named function exported in m4module.h.  */
#ifdef NDEBUG
# define m4_arg_argc(A)         (A)->argc
# define m4_arg_info(A)         (A)->info
# define m4_arg_scratch(C)                              \
  ((C)->arg_stacks[(C)->expansion_level - 1].argv)
#endif /* NDEBUG */


/* --- PATH MANAGEMENT --- */

typedef struct m4__search_path m4__search_path;

struct m4__search_path {
  m4__search_path *next;        /* next directory to search */
  const char *dir;              /* directory */
  int len;
};

struct m4__search_path_info {
  m4__search_path *list;        /* the list of path directories */
  m4__search_path *list_end;    /* the end of same */
  int max_length;               /* length of longest directory name */
};

extern void m4__include_init (m4 *);


/* Debugging the memory allocator.  */

#if WITH_DMALLOC
#  define DMALLOC_FUNC_CHECK
#  include <dmalloc.h>
#endif /* WITH_DMALLOC */



/* Convenience macro to zero a variable after freeing it, as well as
   casting away any const.  */
#define DELETE(Expr)    ((Expr) = (free ((void *) (Expr)), (void *) 0))

/* Avoid negative logic when comparing two strings.  */
#define STREQ(a, b) (strcmp (a, b) == 0)
#define STRNEQ(a, b) (strcmp (a, b) != 0)


#if DEBUG
# define DEBUG_INCL     1
# define DEBUG_INPUT    1
# define DEBUG_MACRO    1
# define DEBUG_MODULES  1
# define DEBUG_OUTPUT   1
# define DEBUG_STKOVF   1
# define DEBUG_SYM      1
# define DEBUG_SYNTAX   1
#endif

#endif /* m4private.h */
