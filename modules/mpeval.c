/* GNU m4 -- A simple macro processor
   Copyright (C) 2000-2001, 2006-2008, 2010, 2013 Free Software
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

#include <config.h>

/* Build using only the exported interfaces, unless NDEBUG is set, in
   which case use private symbols to speed things up as much as possible.  */
#ifndef NDEBUG
#  include <m4/m4module.h>
#else
#  include "m4private.h"
#endif

#if HAVE_GMP_H
#  include <gmp.h>
#endif

/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

           function     macros  blind   side    minargs maxargs */
#define builtin_functions                                       \
  BUILTIN (mpeval,      false,  true,   true,   1,      3  )    \



#define numb_set(ans, i) mpq_set (ans, i)
#define numb_set_si(ans, i) mpq_set_si (*(ans), (long) i, (unsigned long) 1)

#define numb_init(x) mpq_init (x)
#define numb_fini(x) mpq_clear (x)

#define numb_zerop(x)     (mpq_cmp (x, numb_ZERO) == 0)
#define numb_positivep(x) (mpq_cmp (x, numb_ZERO) >  0)
#define numb_negativep(x) (mpq_cmp (x, numb_ZERO) <  0)

#define numb_eq(x, y) numb_set (x, mpq_cmp (x, y) == 0 ? numb_ONE : numb_ZERO)
#define numb_ne(x, y) numb_set (x, mpq_cmp (x, y) != 0 ? numb_ONE : numb_ZERO)
#define numb_lt(x, y) numb_set (x, mpq_cmp (x, y) <  0 ? numb_ONE : numb_ZERO)
#define numb_le(x, y) numb_set (x, mpq_cmp (x, y) <= 0 ? numb_ONE : numb_ZERO)
#define numb_gt(x, y) numb_set (x, mpq_cmp (x, y) >  0 ? numb_ONE : numb_ZERO)
#define numb_ge(x, y) numb_set (x, mpq_cmp (x, y) >= 0 ? numb_ONE : numb_ZERO)

#define numb_lnot(x)    numb_set (x, numb_zerop (x) ? numb_ONE : numb_ZERO)
#define numb_lior(x, y) numb_set (x, numb_zerop (x) ? y : x)
#define numb_land(x, y) numb_set (x, numb_zerop (x) ? numb_ZERO : y)

#define reduce1(f1, x)                                                  \
  do                                                                    \
    {                                                                   \
      number T;                                                         \
      mpq_init (T);                                                     \
      f1 (T, x);                                                        \
      mpq_set (x, T);                                                   \
      mpq_clear (T);                                                    \
    }                                                                   \
  while (0)
#define reduce2(f2,x,y)                                                 \
  do                                                                    \
    {                                                                   \
      number T;                                                         \
      mpq_init (T);                                                     \
      f2 (T, (x), (y));                                                 \
      mpq_set ((x), T);                                                 \
      mpq_clear (T);                                                    \
    }                                                                   \
  while (0)

#define numb_plus(x, y)  reduce2 (mpq_add, x, y)
#define numb_minus(x, y) reduce2 (mpq_sub, x, y)
#define numb_negate(x)   reduce1 (mpq_neg, x)

#define numb_times(x, y) reduce2 (mpq_mul, x, y)
#define numb_ratio(x, y) reduce2 (mpq_div, x, y)
#define numb_invert(x)   reduce1 (mpq_inv, x)

#define numb_incr(n) numb_plus  (n, numb_ONE)
#define numb_decr(n) numb_minus (n, numb_ONE)

/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros, blind, side, min, max)  M4BUILTIN (handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
static const m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, side, min, max)                 \
  M4BUILTIN_ENTRY (handler, #handler, macros, blind, side, min, max)

  builtin_functions
#undef BUILTIN

  { NULL, NULL, 0, 0, 0 },
};


/* A table for mapping m4 symbol names to simple expansion text. */
static const m4_macro m4_macro_table[] =
{
  /* name               text    min     max */
  { "__mpeval__",       "",     0,      0 },
  { NULL,               NULL,   0,      0 },
};


void
include_mpeval (m4 *context, m4_module *module, m4_obstack *obs)
{
  m4_install_builtins (context, module, m4_builtin_table);
  m4_install_macros   (context, module, m4_macro_table);
}


/* GMP defines mpq_t as a 1-element array of struct.  Therefore, `mpq_t'
   is not compatible with `const mpq_t'.  */
typedef mpq_t number;

static void numb_initialise (void);
static void numb_obstack (m4_obstack *obs, const number value,
                          const int radix, int min);
static void mpq2mpz (m4 *context, mpz_t z, const number q, const char *noisily);
static void mpz2mpq (number q, const mpz_t z);
static void numb_divide (number *x, number *y);
static void numb_modulo (m4 *context, number *x, number *y);
static void numb_and (m4 *context, number *x, number *y);
static void numb_ior (m4 *context, number *x, number *y);
static void numb_eor (m4 *context, number *x, number *y);
static void numb_not (m4 *context, number *x);
static void numb_lshift (m4 *context, number *x, number *y);
static void numb_rshift (m4 *context, number *x, number *y);
#define numb_urshift(c, x, y) numb_rshift (c, x, y)


static number numb_ZERO;
static number numb_ONE;

static int numb_initialised = 0;

static void
numb_initialise (void)
{
  if (numb_initialised)
    return;

  numb_init (numb_ZERO);
  numb_set_si (&numb_ZERO, 0);

  numb_init (numb_ONE);
  numb_set_si (&numb_ONE, 1);

  numb_initialised = 1;
}

static void
numb_obstack (m4_obstack *obs, const number value, const int radix,
              int min)
{
  const char *s;
  size_t len;

  mpz_t i;
  mpz_init (i);

  mpq_get_num (i, value);
  s = mpz_get_str (NULL, radix, i);

  if (*s == '-')
    {
      obstack_1grow (obs, '-');
      s++;
    }
  len = strlen (s);
  for (min -= len; --min >= 0;)
    obstack_1grow (obs, '0');

  obstack_grow (obs, s, len);

  mpq_get_den (i, value);
  if (mpz_cmp_si (i, (long) 1) != 0)
    {
      obstack_1grow (obs, '\\');
      s = mpz_get_str ((char *) 0, radix, i);
      obstack_grow (obs, s, strlen (s));
    }

  mpz_clear (i);
}

#define NOISY ""
#define QUIET (char *)0

static void
mpq2mpz (m4 *context, mpz_t z, const number q, const char *noisily)
{
  if (noisily && mpz_cmp_si (mpq_denref (q), (long) 1) != 0)
    m4_warn (context, 0, NULL, _("loss of precision in eval: %s"), noisily);

  mpz_div (z, mpq_numref (q), mpq_denref (q));
}

static void
mpz2mpq (number q, const mpz_t z)
{
  mpq_set_si (q, (long) 0, (unsigned long) 1);
  mpq_set_num (q, z);
}

static void
numb_divide (number * x, number * y)
{
  mpq_t qres;
  mpz_t zres;

  mpq_init (qres);
  mpq_div (qres, *x, *y);

  mpz_init (zres);
  mpz_div (zres, mpq_numref (qres), mpq_denref (qres));
  mpq_clear (qres);

  mpz2mpq (*x, zres);
  mpz_clear (zres);
}

static void
numb_modulo (m4 *context, number * x, number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (context, xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (context, yy, *y, NOISY);

  mpz_init (res);
  mpz_mod (res, xx, yy);

  mpz_clear (xx);
  mpz_clear (yy);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

static void
numb_and (m4 *context, number * x, number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (context, xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (context, yy, *y, NOISY);

  mpz_init (res);
  mpz_and (res, xx, yy);

  mpz_clear (xx);
  mpz_clear (yy);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

static void
numb_ior (m4 *context, number * x, number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (context, xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (context, yy, *y, NOISY);

  mpz_init (res);
  mpz_ior (res, xx, yy);

  mpz_clear (xx);
  mpz_clear (yy);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

static void
numb_eor (m4 *context, number * x, number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (context, xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (context, yy, *y, NOISY);

  mpz_init (res);

#if 0
  mpz_xor (res, xx, yy);
#else /* 0 */
  /* a^b = (a|b) & !(a&b) */
  {
    mpz_t and_ab, ior_ab, nand_ab;

    mpz_init (ior_ab);
    mpz_ior (ior_ab, xx, yy);

    mpz_init (and_ab);
    mpz_and (and_ab, xx, yy);

    mpz_init (nand_ab);
    mpz_com (nand_ab, and_ab);

    mpz_and (res, ior_ab, nand_ab);

    mpz_clear (and_ab);
    mpz_clear (ior_ab);
    mpz_clear (nand_ab);
  }
#endif /* 0 */

  mpz_clear (xx);
  mpz_clear (yy);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

static void
numb_not (m4 *context, number * x)
{
  mpz_t xx, res;

  /* x should be integral */

  mpz_init (xx);
  mpq2mpz (context, xx, *x, NOISY);

  mpz_init (res);
  mpz_com (res, xx);

  mpz_clear (xx);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

static void
numb_lshift (m4 *context, number * x, number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (context, xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (context, yy, *y, NOISY);

  mpz_init (res);
  {
    /* bug: need to determine if y is too big or negative. */
    long int exp = mpz_get_si (yy);
    if (exp >= 0)
      {
        mpz_mul_2exp (res, xx, (unsigned) exp);
      }
    else
      {
        mpz_div_2exp (res, xx, (unsigned) -exp);
      }
  }

  mpz_clear (xx);
  mpz_clear (yy);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

static void
numb_rshift (m4 *context, number * x, number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (context, xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (context, yy, *y, NOISY);

  mpz_init (res);
  {
    /* FIXME: bug - need to determine if y is too big or negative */
    long int exp = mpz_get_si (yy);
    if (exp >= 0)
      {
        mpz_div_2exp (res, xx, (unsigned) exp);
      }
    else
      {
        mpz_mul_2exp (res, xx, (unsigned) -exp);
      }
  }

  mpz_clear (xx);
  mpz_clear (yy);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

#define m4_evaluate     builtin_mpeval
#include "evalparse.c"
