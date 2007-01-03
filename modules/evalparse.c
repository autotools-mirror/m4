/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2001, 2006, 2007
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301  USA
*/

/* This file contains the functions to evaluate integer expressions
   for the "eval" and "evalmp" builtins.  It is a little, fairly
   self-contained module, with its own scanner, and a recursive descent
   parser.

   It has been carefully factored for use from the GMP module builtin,
   mpeval: any actual operation performed on numbers is abstracted by
   a set of macro definitions.  For plain `eval', `number' is some
   long int type, and `numb_*' manipulate those long ints.  When
   using GMP, `number' is typedef'd to `mpq_t' (the arbritrary
   precision fractional numbers type of GMP), and `numb_*' are mapped
   to GMP functions.

   There is only one entry point, `m4_evaluate', a single function for
   both `eval' and `mpeval', but which is redefined appropriately when
   this file is #included into its clients.  */

#include <assert.h>
#include <ctype.h>

typedef enum eval_token
  {
    ERROR, BADOP,
    PLUS, MINUS,
    EXPONENT,
    TIMES, DIVIDE, MODULO, RATIO,
    EQ, NOTEQ, GT, GTEQ, LS, LSEQ,
    LSHIFT, RSHIFT,
    LNOT, LAND, LOR,
    NOT, AND, OR, XOR,
    LEFTP, RIGHTP,
    NUMBER, EOTEXT
  }
eval_token;

/* Error types.  */

typedef enum eval_error
  {
    NO_ERROR,
    MISSING_RIGHT,
    SYNTAX_ERROR,
    UNKNOWN_INPUT,
    EXCESS_INPUT,
    INVALID_OPERATOR,
    DIVIDE_ZERO,
    MODULO_ZERO
  }
eval_error;

static eval_error logical_or_term   (m4 *, eval_token, number *);
static eval_error logical_and_term  (m4 *, eval_token, number *);
static eval_error or_term	    (m4 *, eval_token, number *);
static eval_error xor_term	    (m4 *, eval_token, number *);
static eval_error and_term	    (m4 *, eval_token, number *);
static eval_error equality_term	    (m4 *, eval_token, number *);
static eval_error cmp_term	    (m4 *, eval_token, number *);
static eval_error shift_term	    (m4 *, eval_token, number *);
static eval_error add_term	    (m4 *, eval_token, number *);
static eval_error mult_term	    (m4 *, eval_token, number *);
static eval_error exp_term	    (m4 *, eval_token, number *);
static eval_error unary_term	    (m4 *, eval_token, number *);
static eval_error simple_term	    (m4 *, eval_token, number *);
static void	  numb_pow	    (number *x, number *y);



/* --- LEXICAL FUNCTIONS --- */

/* Pointer to next character of input text.  */
static const unsigned char *eval_text;

/* Value of eval_text, from before last call of eval_lex ().  This is so we
   can back up, if we have read too much.  */
static const unsigned char *last_text;

static void
eval_init_lex (const unsigned char *text)
{
  eval_text = text;
  last_text = NULL;
}

static void
eval_undo (void)
{
  eval_text = last_text;
}

/* VAL is numerical value, if any.  Recognize C assignment operators,
   even though we cannot support them, to issue better error
   messages.  */

static eval_token
eval_lex (number *val)
{
  while (isspace (*eval_text))
    eval_text++;

  last_text = eval_text;

  if (*eval_text == '\0')
    return EOTEXT;

  if (isdigit (*eval_text))
    {
      int base, digit;

      if (*eval_text == '0')
	{
	  eval_text++;
	  switch (*eval_text)
	    {
	    case 'x':
	    case 'X':
	      base = 16;
	      eval_text++;
	      break;

	    case 'b':
	    case 'B':
	      base = 2;
	      eval_text++;
	      break;

	    case 'r':
	    case 'R':
	      base = 0;
	      eval_text++;
	      while (isdigit (*eval_text) && base <= 36)
		base = 10 * base + *eval_text++ - '0';
	      if (base == 0 || base > 36 || *eval_text != ':')
		return ERROR;
	      eval_text++;
	      break;

	    default:
	      base = 8;
	    }
	}
      else
	base = 10;

      numb_set_si (val, 0);
      for (; *eval_text; eval_text++)
	{
	  if (isdigit (*eval_text))
	    digit = *eval_text - '0';
	  else if (islower (*eval_text))
	    digit = *eval_text - 'a' + 10;
	  else if (isupper (*eval_text))
	    digit = *eval_text - 'A' + 10;
	  else
	    break;

	  if (digit >= base)
	    break;

	  { /* (*val) = (*val) * base; */
	    number xbase;
	    numb_init (xbase);
	    numb_set_si (&xbase, base);
	    numb_times (*val, xbase);
	    numb_fini (xbase);
	  }
	  { /* (*val) = (*val) + digit; */
	    number xdigit;
	    numb_init (xdigit);
	    numb_set_si (&xdigit, digit);
	    numb_plus (*val, xdigit);
	    numb_fini (xdigit);
	  }
	}
      return NUMBER;
    }

  switch (*eval_text++)
    {
    case '+':
      if (*eval_text == '+' || *eval_text == '=')
	return BADOP;
      return PLUS;
    case '-':
      if (*eval_text == '-' || *eval_text == '=')
	return BADOP;
      return MINUS;
    case '*':
      if (*eval_text == '*')
	{
	  eval_text++;
	  return EXPONENT;
	}
      else if (*eval_text == '=')
	return BADOP;
      return TIMES;
    case '/':
      if (*eval_text == '=')
	return BADOP;
      return DIVIDE;
    case '%':
      if (*eval_text == '=')
	return BADOP;
      return MODULO;
    case ':':
      return RATIO; /* FIXME - this clashes with supporting ?:.  */
    case '=':
      if (*eval_text == '=')
	{
	  eval_text++;
	  return EQ;
	}
      return BADOP;
    case '!':
      if (*eval_text == '=')
	{
	  eval_text++;
	  return NOTEQ;
	}
      return LNOT;
    case '>':
      if (*eval_text == '=')
	{
	  eval_text++;
	  return GTEQ;
	}
      else if (*eval_text == '>')
	{
	  if (*eval_text++ == '=')
	    return BADOP;
	  return RSHIFT;
	}
      else
	return GT;
    case '<':
      if (*eval_text == '=')
	{
	  eval_text++;
	  return LSEQ;
	}
      else if (*eval_text == '<')
	{
	  if (*eval_text++ == '=')
	    return BADOP;
	  return LSHIFT;
	}
      else
	return LS;
    case '^':
      if (*eval_text == '=')
	return BADOP;
      return XOR;
    case '~':
      return NOT;
    case '&':
      if (*eval_text == '&')
	{
	  eval_text++;
	  return LAND;
	}
      else if (*eval_text == '=')
	return BADOP;
      return AND;
    case '|':
      if (*eval_text == '|')
	{
	  eval_text++;
	  return LOR;
	}
      else if (*eval_text == '=')
	return BADOP;
      return OR;
    case '(':
      return LEFTP;
    case ')':
      return RIGHTP;
    default:
      return ERROR;
    }
}

/* Recursive descent parser.  */
static eval_error
logical_or_term (m4 *context, eval_token et, number *v1)
{
  number v2;
  eval_error er;

  if ((er = logical_and_term (context, et, v1)) != NO_ERROR)
    return er;

  numb_init (v2);
  while ((et = eval_lex (&v2)) == LOR)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      /* Implement short-circuiting of valid syntax.  */
      er = logical_and_term (context, et, &v2);
      if (er == NO_ERROR)
	numb_lior (*v1, v2);
      else if (! numb_zerop (*v1)
	       && (er == DIVIDE_ZERO || er == MODULO_ZERO))
	numb_set (*v1, numb_ONE);
      else
	return er;
    }
  numb_fini (v2);
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
logical_and_term (m4 *context, eval_token et, number *v1)
{
  number v2;
  eval_error er;

  if ((er = or_term (context, et, v1)) != NO_ERROR)
    return er;

  numb_init (v2);
  while ((et = eval_lex (&v2)) == LAND)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      /* Implement short-circuiting of valid syntax.  */
      er = or_term (context, et, &v2);
      if (er == NO_ERROR)
	numb_land (*v1, v2);
      else if (numb_zerop (*v1)
	       && (er == DIVIDE_ZERO || er == MODULO_ZERO))
	numb_set (*v1, numb_ZERO);
      else
	return er;
    }
  numb_fini (v2);
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
or_term (m4 *context, eval_token et, number *v1)
{
  number v2;
  eval_error er;

  if ((er = xor_term (context, et, v1)) != NO_ERROR)
    return er;

  numb_init (v2);
  while ((et = eval_lex (&v2)) == OR)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = xor_term (context, et, &v2)) != NO_ERROR)
	return er;

      numb_ior (context, v1, &v2);
    }
  numb_fini (v2);
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
xor_term (m4 *context, eval_token et, number *v1)
{
  number v2;
  eval_error er;

  if ((er = and_term (context, et, v1)) != NO_ERROR)
    return er;

  numb_init (v2);
  while ((et = eval_lex (&v2)) == XOR)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = and_term (context, et, &v2)) != NO_ERROR)
	return er;

      numb_eor (context, v1, &v2);
    }
  numb_fini (v2);
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
and_term (m4 *context, eval_token et, number *v1)
{
  number v2;
  eval_error er;

  if ((er = equality_term (context, et, v1)) != NO_ERROR)
    return er;

  numb_init (v2);
  while ((et = eval_lex (&v2)) == AND)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = equality_term (context, et, &v2)) != NO_ERROR)
	return er;

      numb_and (context, v1, &v2);
    }
  numb_fini (v2);
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
equality_term (m4 *context, eval_token et, number *v1)
{
  eval_token op;
  number v2;
  eval_error er;

  if ((er = cmp_term (context, et, v1)) != NO_ERROR)
    return er;

  numb_init (v2);
  while ((op = eval_lex (&v2)) == EQ || op == NOTEQ)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = cmp_term (context, et, &v2)) != NO_ERROR)
	return er;

      if (op == EQ)
	numb_eq (*v1, v2);
      else
	numb_ne (*v1, v2);
    }
  numb_fini (v2);
  if (op == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
cmp_term (m4 *context, eval_token et, number *v1)
{
  eval_token op;
  number v2;
  eval_error er;

  if ((er = shift_term (context, et, v1)) != NO_ERROR)
    return er;

  numb_init (v2);
  while ((op = eval_lex (&v2)) == GT || op == GTEQ
	 || op == LS || op == LSEQ)
    {

      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = shift_term (context, et, &v2)) != NO_ERROR)
	return er;

      switch (op)
	{
	case GT:
	  numb_gt (*v1, v2);
	  break;

	case GTEQ:
	  numb_ge (*v1, v2);
	  break;

	case LS:
	  numb_lt (*v1, v2);
	  break;

	case LSEQ:
	  numb_le (*v1, v2);
	  break;

	default:
	  assert (!"INTERNAL ERROR: bad comparison operator in cmp_term ()");
	  abort ();
	}
    }
  numb_fini (v2);
  if (op == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
shift_term (m4 *context, eval_token et, number *v1)
{
  eval_token op;
  number v2;
  eval_error er;

  if ((er = add_term (context, et, v1)) != NO_ERROR)
    return er;

  numb_init (v2);
  while ((op = eval_lex (&v2)) == LSHIFT || op == RSHIFT)
    {

      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = add_term (context, et, &v2)) != NO_ERROR)
	return er;

      switch (op)
	{
	case LSHIFT:
	  numb_lshift (context, v1, &v2);
	  break;

	case RSHIFT:
	  numb_rshift (context, v1, &v2);
	  break;

	default:
	  assert (!"INTERNAL ERROR: bad shift operator in shift_term ()");
	  abort ();
	}
    }
  numb_fini (v2);
  if (op == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
add_term (m4 *context, eval_token et, number *v1)
{
  eval_token op;
  number v2;
  eval_error er;

  if ((er = mult_term (context, et, v1)) != NO_ERROR)
    return er;

  numb_init (v2);
  while ((op = eval_lex (&v2)) == PLUS || op == MINUS)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = mult_term (context, et, &v2)) != NO_ERROR)
	return er;

      if (op == PLUS)
	numb_plus (*v1, v2);
      else
	numb_minus (*v1, v2);
    }
  numb_fini (v2);
  if (op == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
mult_term (m4 *context, eval_token et, number *v1)
{
  eval_token op;
  number v2;
  eval_error er;

  if ((er = exp_term (context, et, v1)) != NO_ERROR)
    return er;

  numb_init (v2);
  while (op = eval_lex (&v2),
	 op == TIMES
	 || op == DIVIDE
	 || op == MODULO
	 || op == RATIO)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = exp_term (context, et, &v2)) != NO_ERROR)
	return er;

      switch (op)
	{
	case TIMES:
	  numb_times (*v1, v2);
	  break;

	case DIVIDE:
	  if (numb_zerop (v2))
	    return DIVIDE_ZERO;
	  else
	    numb_divide(v1, &v2);
	  break;

	case RATIO:
	  if (numb_zerop (v2))
	    return DIVIDE_ZERO;
	  else
	    numb_ratio (*v1, v2);
	  break;

	case MODULO:
	  if (numb_zerop (v2))
	    return MODULO_ZERO;
	  else
	    numb_modulo (context, v1, &v2);
	  break;

	default:
	  assert (!"INTERNAL ERROR: bad operator in mult_term ()");
	  abort ();
	}
    }
  numb_fini (v2);
  if (op == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
exp_term (m4 *context, eval_token et, number *v1)
{
  number result;
  number v2;
  eval_error er;

  if ((er = unary_term (context, et, v1)) != NO_ERROR)
    return er;
  memcpy (&result, v1, sizeof(number));

  numb_init (v2);
  while ((et = eval_lex (&v2)) == EXPONENT)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = exp_term (context, et, &v2)) != NO_ERROR)
	return er;

      numb_pow (v1, &v2);
    }
  numb_fini (v2);
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
unary_term (m4 *context, eval_token et, number *v1)
{
  eval_token et2 = et;
  eval_error er;

  if (et == PLUS || et == MINUS || et == NOT || et == LNOT)
    {
      et2 = eval_lex (v1);
      if (et2 == ERROR)
	return UNKNOWN_INPUT;

      if ((er = unary_term (context, et2, v1)) != NO_ERROR)
	return er;

      if (et == MINUS)
	numb_negate(*v1);
      else if (et == NOT)
	numb_not (context, v1);
      else if (et == LNOT)
	numb_lnot (*v1);
    }
  else
    if ((er = simple_term (context, et, v1)) != NO_ERROR)
      return er;

  return NO_ERROR;
}

static eval_error
simple_term (m4 *context, eval_token et, number *v1)
{
  number v2;
  eval_error er;

  switch (et)
    {
    case LEFTP:
      et = eval_lex (v1);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = logical_or_term (context, et, v1)) != NO_ERROR)
	return er;

      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if (et != RIGHTP)
	return MISSING_RIGHT;

      break;

    case NUMBER:
      break;

    case BADOP:
      return INVALID_OPERATOR;

    default:
      return SYNTAX_ERROR;
    }
  return NO_ERROR;
}

/* Main entry point, called from "eval" and "mpeval" builtins.  */
void
m4_evaluate (m4 *context, m4_obstack *obs, int argc, m4_symbol_value **argv)
{
  int		radix	= 10;
  int		min	= 1;
  number	val;
  eval_token	et;
  eval_error	err;

  if (argc >= 3 && !m4_numeric_arg (context, argc, argv, 2, &radix))
    return;

  if (radix <= 1 || radix > 36)
    {
      m4_error (context, 0, 0, _("%s: radix out of range: %d"),
		M4ARG (0), radix);
      return;
    }

  if (argc >= 4 && !m4_numeric_arg (context, argc, argv, 3, &min))
    return;

  if (min <= 0)
    {
      m4_error (context, 0, 0, _("%s: negative width: %d"), M4ARG (0), min);
      return;
    }

  numb_initialise ();
  eval_init_lex (M4ARG (1));

  numb_init (val);
  et = eval_lex (&val);
  err = logical_or_term (context, et, &val);

  if (err == NO_ERROR && *eval_text != '\0')
    {
      if (eval_lex (&val) == BADOP)
	err = INVALID_OPERATOR;
      else
	err = EXCESS_INPUT;
    }

  switch (err)
    {
    case NO_ERROR:
      numb_obstack (obs, val, radix, min);
      break;

    case MISSING_RIGHT:
      m4_error (context, 0, 0, _("%s: missing right parenthesis: %s"),
		M4ARG (0), M4ARG (1));
      break;

    case SYNTAX_ERROR:
      m4_error (context, 0, 0, _("%s: bad expression: %s"),
		M4ARG (0), M4ARG (1));
      break;

    case UNKNOWN_INPUT:
      m4_error (context, 0, 0, _("%s: bad input: %s"), M4ARG (0), M4ARG (1));
      break;

    case EXCESS_INPUT:
      m4_error (context, 0, 0, _("%s: excess input: %s"), M4ARG (0),
		M4ARG (1));
      break;

    case INVALID_OPERATOR:
      m4_error (context, 0, 0, _("%s: invalid operator: %s"), M4ARG (0),
		M4ARG (1));
      break;

    case DIVIDE_ZERO:
      m4_error (context, 0, 0, _("%s: divide by zero: %s"), M4ARG (0),
		M4ARG (1));
      break;

    case MODULO_ZERO:
      m4_error (context, 0, 0, _("%s: modulo by zero: %s"), M4ARG (0),
		M4ARG (1));
      break;

    default:
      assert (!"INTERNAL ERROR: bad error code in evaluate ()");
      abort ();
    }

  numb_fini (val);
}

static void
numb_pow (number *x, number *y)
{
  /* y should be integral */

  number ans, yy;

  numb_init (ans);
  numb_set_si (&ans, 1);

  numb_init (yy);
  numb_set (yy, *y);

  if (numb_negativep (yy))
    {
      numb_invert (*x);
      numb_negate (yy);
    }

  while (numb_positivep (yy))
    {
      numb_times (ans, *x);
      numb_decr (yy);
    }
  numb_set (*x, ans);

  numb_fini (ans);
  numb_fini (yy);
}
