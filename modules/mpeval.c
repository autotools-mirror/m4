/* GNU m4 -- A simple macro processor
   Copyright 2000 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307  USA
*/

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <m4module.h>

#if !USE_GMP

M4INIT_HANDLER (mpeval)
{
  const char s[] = "libgmp support was not compiled in";

  if (obs)
    obstack_grow (obs, s, strlen(s));
}

#else /* USE_GMP */

#if HAVE_GMP_H
#  include <gmp.h>
#endif


/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	mpeval_LTX_m4_builtin_table
#define m4_macro_table		mpeval_LTX_m4_macro_table


/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

		function	macros	blind */
#define builtin_functions			\
	BUILTIN(mpeval,		FALSE,	TRUE )



#define numb_set(ans,i) mpq_set(ans,i)
#define numb_set_si(ans,i) mpq_set_si(*(ans),(long)i,(unsigned long)1)

#define numb_init(x) mpq_init((x))
#define numb_fini(x) mpq_clear((x))

#define numb_zerop(x)     (mpq_cmp(x,numb_ZERO) == 0)
#define numb_positivep(x) (mpq_cmp(x,numb_ZERO) >  0)
#define numb_negativep(x) (mpq_cmp(x,numb_ZERO) <  0)

#define numb_eq(x,y) numb_set(x,mpq_cmp(x,y)==0? numb_ONE: numb_ZERO)
#define numb_ne(x,y) numb_set(x,mpq_cmp(x,y)!=0? numb_ONE: numb_ZERO)
#define numb_lt(x,y) numb_set(x,mpq_cmp(x,y)< 0? numb_ONE: numb_ZERO)
#define numb_le(x,y) numb_set(x,mpq_cmp(x,y)<=0? numb_ONE: numb_ZERO)
#define numb_gt(x,y) numb_set(x,mpq_cmp(x,y)> 0? numb_ONE: numb_ZERO)
#define numb_ge(x,y) numb_set(x,mpq_cmp(x,y)>=0? numb_ONE: numb_ZERO)

#define numb_lnot(x)   numb_set(x,numb_zerop(x)? numb_ONE: numb_ZERO)
#define numb_lior(x,y) numb_set(x,numb_zerop(x)? y: numb_ONE)
#define numb_land(x,y) numb_set(x,numb_zerop(x)? numb_ZERO: y)

#define reduce1(f1,x) \
  { number T; mpq_init(T); f1(T,x);   mpq_set(x,T); mpq_clear(T); }
#define reduce2(f2,x,y) \
  { number T; mpq_init(T); f2(T,(x),(y)); mpq_set((x),T); mpq_clear(T); }

#define numb_plus(x,y)  reduce2(mpq_add,x,y)
#define numb_minus(x,y) reduce2(mpq_sub,x,y)
#define numb_negate(x)  reduce1(mpq_neg,x)

#define numb_times(x,y) reduce2(mpq_mul,x,y)
#define numb_ratio(x,y) reduce2(mpq_div,x,y)
#define numb_invert(x)  reduce1(mpq_inv,x)

#define numb_decr(n) numb_minus(n,numb_ONE)

/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros,  blind)	M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind)		\
	{ STR(handler), CONC(builtin_, handler), macros, blind },
  builtin_functions
#undef BUILTIN

  { 0, 0, FALSE, FALSE },
};


/* A table for mapping m4 symbol names to simple expansion text. */
m4_macro m4_macro_table[] =
{
  /* name                      text */
  { "__gmp__",                 "" },
  { 0, 0 },
};


/* number should be at least 32 bits.  */
typedef mpq_t number;

static void numb_initialise (void);
static void numb_obstack (struct obstack *obs, const number value,
			  const int radix, int min);
static void mpq2mpz (mpz_t z, const number q, const char *noisily);
static void mpz2mpq (number q, const mpz_t z);
static void numb_divide (number *x, const number *y);
static void numb_modulo (number *x, const number *y);
static void numb_and (number *x, const number *y);
static void numb_ior (number *x, const number *y);
static void numb_eor (number *x, const number *y);
static void numb_not (number *x);
static void numb_lshift (number *x, const number *y);
static void numb_rshift (number *x, const number *y);


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
numb_obstack (struct obstack *obs, const number value, const int radix,
	      int min)
{
  const char *s;

  mpz_t i;
  mpz_init (i);

  mpq_get_num (i, value);
  s = mpz_get_str ((char *) 0, radix, i);

  if (*s == '-')
    {
      obstack_1grow (obs, '-');
      min--;
      s++;
    }
  for (min -= strlen (s); --min >= 0;)
    obstack_1grow (obs, '0');

  obstack_grow (obs, s, strlen (s));

  mpq_get_den (i, value);
  if (mpz_cmp_si (i, (long) 1) != 0)
    {
      obstack_1grow (obs, ':');
      s = mpz_get_str ((char *) 0, radix, i);
      obstack_grow (obs, s, strlen (s));
    }

  mpz_clear (i);
}

#define NOISY ""
#define QUIET (char *)0

static void
mpq2mpz (mpz_t z, const number q, const char *noisily)
{
  if (noisily && mpz_cmp_si (mpq_denref (q), (long) 1) != 0)
    {
      M4ERROR ((warning_status, 0,
		_("Loss of precision in eval: %s"), noisily));
    }

  mpz_div (z, mpq_numref (q), mpq_denref (q));
}

static void
mpz2mpq (number q, const mpz_t z)
{
  mpq_set_si (q, (long) 0, (unsigned long) 1);
  mpq_set_num (q, z);
}

static void
numb_divide (number * x, const number * y)
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
numb_modulo (number * x, const number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (yy, *y, NOISY);

  mpz_init (res);
  mpz_mod (res, xx, yy);

  mpz_clear (xx);
  mpz_clear (yy);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

static void
numb_and (number * x, const number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (yy, *y, NOISY);

  mpz_init (res);
  mpz_and (res, xx, yy);

  mpz_clear (xx);
  mpz_clear (yy);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

static void
numb_ior (number * x, const number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (yy, *y, NOISY);

  mpz_init (res);
  mpz_ior (res, xx, yy);

  mpz_clear (xx);
  mpz_clear (yy);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

static void
numb_eor (number * x, const number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (yy, *y, NOISY);

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
numb_not (number * x)
{
  mpz_t xx, res;

  /* x should be integral */

  mpz_init (xx);
  mpq2mpz (xx, *x, NOISY);

  mpz_init (res);
  mpz_com (res, xx);

  mpz_clear (xx);

  mpz2mpq (*x, res);
  mpz_clear (res);
}

static void
numb_lshift (number * x, const number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (yy, *y, NOISY);

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
numb_rshift (number * x, const number * y)
{
  mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

  mpz_init (xx);
  mpq2mpz (xx, *x, NOISY);

  mpz_init (yy);
  mpq2mpz (yy, *y, NOISY);

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

#define m4_evaluate	builtin_mpeval
#include "evalparse.c"

#endif /* USE_GMP */
