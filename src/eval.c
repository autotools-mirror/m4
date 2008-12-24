/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2006, 2007, 2008
   Free Software Foundation, Inc.

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

/* This file contains the functions to evaluate integer expressions for
   the "eval" macro.  It is a little, fairly self-contained module, with
   its own scanner, and a recursive descent parser.  The only entry point
   is evaluate ().  */

#include "m4.h"

/* Evaluates token types.  */

typedef enum eval_token
  {
    ERROR, BADOP,
    PLUS, MINUS,
    EXPONENT,
    TIMES, DIVIDE, MODULO,
    ASSIGN, EQ, NOTEQ, GT, GTEQ, LS, LSEQ,
    LSHIFT, RSHIFT,
    LNOT, LAND, LOR,
    NOT, AND, OR, XOR,
    LEFTP, RIGHTP,
    QUESTION, COLON,
    NUMBER, EOTEXT
  }
eval_token;

/* Error types.  */

typedef enum eval_error
  {
    NO_ERROR,
    DIVIDE_ZERO,
    MODULO_ZERO,
    NEGATIVE_EXPONENT,
    /* All errors prior to SYNTAX_ERROR can be ignored in a dead
       branch of && and ||.  All errors after are just more details
       about a syntax error.  */
    SYNTAX_ERROR,
    MISSING_RIGHT,
    MISSING_COLON,
    UNKNOWN_INPUT,
    EXCESS_INPUT,
    INVALID_OPERATOR,
    EMPTY_ARGUMENT
  }
eval_error;

static eval_error condition_term (const call_info *, eval_token, int32_t *);
static eval_error logical_or_term (const call_info *, eval_token, int32_t *);
static eval_error logical_and_term (const call_info *, eval_token, int32_t *);
static eval_error or_term (const call_info *, eval_token, int32_t *);
static eval_error xor_term (const call_info *, eval_token, int32_t *);
static eval_error and_term (const call_info *, eval_token, int32_t *);
static eval_error equality_term (const call_info *, eval_token, int32_t *);
static eval_error cmp_term (const call_info *, eval_token, int32_t *);
static eval_error shift_term (const call_info *, eval_token, int32_t *);
static eval_error add_term (const call_info *, eval_token, int32_t *);
static eval_error mult_term (const call_info *, eval_token, int32_t *);
static eval_error exp_term (const call_info *, eval_token, int32_t *);
static eval_error unary_term (const call_info *, eval_token, int32_t *);
static eval_error simple_term (const call_info *, eval_token, int32_t *);

/*--------------------.
| Lexical functions.  |
`--------------------*/

/* Pointer to next character of input text.  */
static const char *eval_text;

/* Value of eval_text, from before last call of eval_lex ().  This is so we
   can back up, if we have read too much.  */
static const char *last_text;

/* Detect when to end parsing.  */
static const char *end_text;

/* Prime the lexer at the start of TEXT, with length LEN.  */
static void
eval_init_lex (const char *text, size_t len)
{
  eval_text = text;
  end_text = text + len;
  last_text = NULL;
}

static void
eval_undo (void)
{
  eval_text = last_text;
}

/* VAL is numerical value, if any.  */

static eval_token
eval_lex (int32_t *val)
{
  while (eval_text != end_text && isspace (to_uchar (*eval_text)))
    eval_text++;

  last_text = eval_text;

  if (eval_text == end_text)
    return EOTEXT;

  if (isdigit (to_uchar (*eval_text)))
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
	      while (isdigit (to_uchar (*eval_text)) && base <= 36)
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

      /* FIXME - this calculation can overflow.  Consider xstrtol.  */
      *val = 0;
      for (; *eval_text; eval_text++)
	{
	  if (isdigit (to_uchar (*eval_text)))
	    digit = *eval_text - '0';
	  else if (islower (to_uchar (*eval_text)))
	    digit = *eval_text - 'a' + 10;
	  else if (isupper (to_uchar (*eval_text)))
	    digit = *eval_text - 'A' + 10;
	  else
	    break;

	  if (base == 1)
	    {
	      if (digit == 1)
		(*val)++;
	      else if (digit == 0 && !*val)
		continue;
	      else
		break;
	    }
	  else if (digit >= base)
	    break;
	  else
	    *val = *val * base + digit;
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
    case '=':
      if (*eval_text == '=')
	{
	  eval_text++;
	  return EQ;
	}
      return ASSIGN;
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
	  if (*++eval_text == '=')
	    return BADOP;
	  return RSHIFT;
	}
      return GT;
    case '<':
      if (*eval_text == '=')
	{
	  eval_text++;
	  return LSEQ;
	}
      else if (*eval_text == '<')
	{
	  if (*++eval_text == '=')
	    return BADOP;
	  return LSHIFT;
	}
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
    case '?':
      return QUESTION;
    case ':':
      return COLON;
    default:
      return ERROR;
    }
}

/*---------------------------------------.
| Main entry point, called from "eval".	 |
`---------------------------------------*/

bool
evaluate (const call_info *me, const char *expr, size_t len, int32_t *val)
{
  eval_token et;
  eval_error err;

  eval_init_lex (expr, len);
  et = eval_lex (val);
  if (et == EOTEXT)
    err = EMPTY_ARGUMENT;
  else
    err = condition_term (me, et, val);

  if (err == NO_ERROR && *eval_text != '\0')
    {
      if (eval_lex (val) == BADOP)
	err = INVALID_OPERATOR;
      else
	err = EXCESS_INPUT;
    }

  if (err != NO_ERROR)
    expr = quotearg_style_mem (locale_quoting_style, expr, len);
  switch (err)
    {
      /* Cases where result is printed.  */
    case NO_ERROR:
      return false;

    case EMPTY_ARGUMENT:
      m4_warn (0, me, _("empty string treated as 0"));
      return false;

      /* Cases where error makes result meaningless.  */
    case MISSING_RIGHT:
      m4_warn (0, me, _("missing right parenthesis: %s"), expr);
      break;

    case MISSING_COLON:
      m4_warn (0, me, _("missing colon: %s"), expr);
      break;

    case SYNTAX_ERROR:
      m4_warn (0, me, _("bad expression: %s"), expr);
      break;

    case UNKNOWN_INPUT:
      m4_warn (0, me, _("bad input: %s"), expr);
      break;

    case EXCESS_INPUT:
      m4_warn (0, me, _("excess input: %s"), expr);
      break;

    case INVALID_OPERATOR:
      m4_warn (0, me, _("invalid operator: %s"), expr);
      break;

    case DIVIDE_ZERO:
      m4_warn (0, me, _("divide by zero: %s"), expr);
      break;

    case MODULO_ZERO:
      m4_warn (0, me, _("modulo by zero: %s"), expr);
      break;

    case NEGATIVE_EXPONENT:
      m4_warn (0, me, _("negative exponent: %s"), expr);
      break;

    default:
      assert (!"evaluate");
      abort ();
    }

  return true;
}

/*---------------------------.
| Recursive descent parser.  |
`---------------------------*/

static eval_error
condition_term (const call_info *me, eval_token et, int32_t *v1)
{
  int32_t v2;
  int32_t v3;
  eval_error er;

  if ((er = logical_or_term (me, et, v1)) != NO_ERROR)
    return er;

  if ((et = eval_lex (&v2)) == QUESTION)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      /* Implement short-circuiting of valid syntax.  */
      /* C requires 'logical_or_term ? expression : condition_term';
	 if we ever introduce assignment_term or comma_term, then
	 condition_term and expression are no longer synonymous.  */
      er = condition_term (me, et, &v2);
      if (er != NO_ERROR
	  && !(*v1 == 0 && er < SYNTAX_ERROR))
	return er;

      et = eval_lex (&v3);
      if (et == ERROR)
	return UNKNOWN_INPUT;
      if (et != COLON)
	return MISSING_COLON;

      et = eval_lex (&v3);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      er = condition_term (me, et, &v3);
      if (er != NO_ERROR
	  && !(*v1 != 0 && er < SYNTAX_ERROR))
	return er;

      *v1 = *v1 ? v2 : v3;
    }
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
logical_or_term (const call_info *me, eval_token et, int32_t *v1)
{
  int32_t v2;
  eval_error er;

  if ((er = logical_and_term (me, et, v1)) != NO_ERROR)
    return er;

  while ((et = eval_lex (&v2)) == LOR)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      /* Implement short-circuiting of valid syntax.  */
      er = logical_and_term (me, et, &v2);
      if (er == NO_ERROR)
	*v1 = *v1 || v2;
      else if (*v1 != 0 && er < SYNTAX_ERROR)
	*v1 = 1;
      else
	return er;
    }
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
logical_and_term (const call_info *me, eval_token et, int32_t *v1)
{
  int32_t v2;
  eval_error er;

  if ((er = or_term (me, et, v1)) != NO_ERROR)
    return er;

  while ((et = eval_lex (&v2)) == LAND)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      /* Implement short-circuiting of valid syntax.  */
      er = or_term (me, et, &v2);
      if (er == NO_ERROR)
	*v1 = *v1 && v2;
      else if (*v1 == 0 && er < SYNTAX_ERROR)
	; /* v1 is already 0 */
      else
	return er;
    }
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
or_term (const call_info *me, eval_token et, int32_t *v1)
{
  int32_t v2;
  eval_error er;

  if ((er = xor_term (me, et, v1)) != NO_ERROR)
    return er;

  while ((et = eval_lex (&v2)) == OR)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = xor_term (me, et, &v2)) != NO_ERROR)
	return er;

      *v1 |= v2;
    }
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
xor_term (const call_info *me, eval_token et, int32_t *v1)
{
  int32_t v2;
  eval_error er;

  if ((er = and_term (me, et, v1)) != NO_ERROR)
    return er;

  while ((et = eval_lex (&v2)) == XOR)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = and_term (me, et, &v2)) != NO_ERROR)
	return er;

      *v1 ^= v2;
    }
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
and_term (const call_info *me, eval_token et, int32_t *v1)
{
  int32_t v2;
  eval_error er;

  if ((er = equality_term (me, et, v1)) != NO_ERROR)
    return er;

  while ((et = eval_lex (&v2)) == AND)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = equality_term (me, et, &v2)) != NO_ERROR)
	return er;

      *v1 &= v2;
    }
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
equality_term (const call_info *me, eval_token et, int32_t *v1)
{
  eval_token op;
  int32_t v2;
  eval_error er;

  if ((er = cmp_term (me, et, v1)) != NO_ERROR)
    return er;

  /* In the 1.4.x series, we maintain the traditional behavior that
     '=' is a synonym for '=='; however, this is contrary to POSIX and
     we hope to convert '=' to mean assignment in 2.0.  */
  while ((op = eval_lex (&v2)) == EQ || op == NOTEQ || op == ASSIGN)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = cmp_term (me, et, &v2)) != NO_ERROR)
	return er;

      if (op == ASSIGN)
      {
	m4_warn (0, me, _("recommend ==, not =, for equality"));
	op = EQ;
      }
      *v1 = (op == EQ) == (*v1 == v2);
    }
  if (op == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
cmp_term (const call_info *me, eval_token et, int32_t *v1)
{
  eval_token op;
  int32_t v2;
  eval_error er;

  if ((er = shift_term (me, et, v1)) != NO_ERROR)
    return er;

  while ((op = eval_lex (&v2)) == GT || op == GTEQ
	 || op == LS || op == LSEQ)
    {

      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = shift_term (me, et, &v2)) != NO_ERROR)
	return er;

      switch (op)
	{
	case GT:
	  *v1 = *v1 > v2;
	  break;

	case GTEQ:
	  *v1 = *v1 >= v2;
	  break;

	case LS:
	  *v1 = *v1 < v2;
	  break;

	case LSEQ:
	  *v1 = *v1 <= v2;
	  break;

	default:
	  assert (!"cmp_term");
	  abort ();
	}
    }
  if (op == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
shift_term (const call_info *me, eval_token et, int32_t *v1)
{
  eval_token op;
  int32_t v2;
  uint32_t u1;
  eval_error er;

  if ((er = add_term (me, et, v1)) != NO_ERROR)
    return er;

  while ((op = eval_lex (&v2)) == LSHIFT || op == RSHIFT)
    {

      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = add_term (me, et, &v2)) != NO_ERROR)
	return er;

      /* Minimize undefined C behavior (shifting by a negative number,
	 shifting by the width or greater, left shift overflow, or
	 right shift of a negative number).  Implement Java 32-bit
	 wrap-around semantics.  This code assumes that the
	 implementation-defined overflow when casting unsigned to
	 signed is a silent twos-complement wrap-around.  */
      switch (op)
	{
	case LSHIFT:
	  u1 = *v1;
	  u1 <<= (uint32_t) (v2 & 0x1f);
	  *v1 = u1;
	  break;

	case RSHIFT:
	  u1 = *v1 < 0 ? ~*v1 : *v1;
	  u1 >>= (uint32_t) (v2 & 0x1f);
	  *v1 = *v1 < 0 ? ~u1 : u1;
	  break;

	default:
	  assert (!"shift_term");
	  abort ();
	}
    }
  if (op == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
add_term (const call_info *me, eval_token et, int32_t *v1)
{
  eval_token op;
  int32_t v2;
  eval_error er;

  if ((er = mult_term (me, et, v1)) != NO_ERROR)
    return er;

  while ((op = eval_lex (&v2)) == PLUS || op == MINUS)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = mult_term (me, et, &v2)) != NO_ERROR)
	return er;

      /* Minimize undefined C behavior on overflow.  This code assumes
	 that the implementation-defined overflow when casting
	 unsigned to signed is a silent twos-complement
	 wrap-around.  */
      if (op == PLUS)
	*v1 = (int32_t) ((uint32_t) *v1 + (uint32_t) v2);
      else
	*v1 = (int32_t) ((uint32_t) *v1 - (uint32_t) v2);
    }
  if (op == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
mult_term (const call_info *me, eval_token et, int32_t *v1)
{
  eval_token op;
  int32_t v2;
  eval_error er;

  if ((er = exp_term (me, et, v1)) != NO_ERROR)
    return er;

  while ((op = eval_lex (&v2)) == TIMES || op == DIVIDE || op == MODULO)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = exp_term (me, et, &v2)) != NO_ERROR)
	return er;

      /* Minimize undefined C behavior on overflow.  This code assumes
	 that the implementation-defined overflow when casting
	 unsigned to signed is a silent twos-complement
	 wrap-around.  */
      switch (op)
	{
	case TIMES:
	  *v1 = (int32_t) ((uint32_t) *v1 * (uint32_t) v2);
	  break;

	case DIVIDE:
	  if (v2 == 0)
	    return DIVIDE_ZERO;
	  else if (v2 == -1)
	    /* Avoid overflow, and the x86 SIGFPE on INT_MIN / -1.  */
	    *v1 = (int32_t) -(uint32_t) *v1;
	  else
	    *v1 /= v2;
	  break;

	case MODULO:
	  if (v2 == 0)
	    return MODULO_ZERO;
	  else if (v2 == -1)
	    /* Avoid the x86 SIGFPE on INT_MIN % -1.  */
	    *v1 = 0;
	  else
	    *v1 %= v2;
	  break;

	default:
	  assert (!"mult_term");
	  abort ();
	}
    }
  if (op == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
exp_term (const call_info *me, eval_token et, int32_t *v1)
{
  uint32_t result;
  int32_t v2;
  eval_error er;

  if ((er = unary_term (me, et, v1)) != NO_ERROR)
    return er;

  while ((et = eval_lex (&v2)) == EXPONENT)
    {
      et = eval_lex (&v2);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = exp_term (me, et, &v2)) != NO_ERROR)
	return er;

      /* Minimize undefined C behavior on overflow.  This code assumes
	 that the implementation-defined overflow when casting
	 unsigned to signed is a silent twos-complement
	 wrap-around.  */
      result = 1;
      if (v2 < 0)
	return NEGATIVE_EXPONENT;
      if (*v1 == 0 && v2 == 0)
	return DIVIDE_ZERO;
      while (v2-- > 0)
	result *= (uint32_t) *v1;
      *v1 = result;
    }
  if (et == ERROR)
    return UNKNOWN_INPUT;

  eval_undo ();
  return NO_ERROR;
}

static eval_error
unary_term (const call_info *me, eval_token et, int32_t *v1)
{
  eval_token et2 = et;
  eval_error er;

  if (et == PLUS || et == MINUS || et == NOT || et == LNOT)
    {
      et2 = eval_lex (v1);
      if (et2 == ERROR)
	return UNKNOWN_INPUT;

      if ((er = unary_term (me, et2, v1)) != NO_ERROR)
	return er;

      /* Minimize undefined C behavior on overflow.  This code assumes
	 that the implementation-defined overflow when casting
	 unsigned to signed is a silent twos-complement
	 wrap-around.  */
      if (et == MINUS)
	*v1 = (int32_t) -(uint32_t) *v1;
      else if (et == NOT)
	*v1 = ~*v1;
      else if (et == LNOT)
	*v1 = *v1 == 0 ? 1 : 0;
    }
  else if ((er = simple_term (me, et, v1)) != NO_ERROR)
    return er;

  return NO_ERROR;
}

static eval_error
simple_term (const call_info *me, eval_token et, int32_t *v1)
{
  int32_t v2;
  eval_error er;

  switch (et)
    {
    case LEFTP:
      et = eval_lex (v1);
      if (et == ERROR)
	return UNKNOWN_INPUT;

      if ((er = condition_term (me, et, v1)) != NO_ERROR)
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
