/* GNU m4 -- A simple macro processor
   Copyright (C) 1995, 1998 Free Software Foundation, Inc.
  
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

/* This file contains the functions to evaluate integer or multiple
 * precision expressions for the "eval" macro.
 */

/* THIS FILE IS INTENDED FOR INCLUSION IN eval.c NOT FOR COMPILATION */

#include "numb.h"

#ifdef USE_GMP

static eval_t numb_ZERO;
static eval_t numb_ONE;

static int numb_initialised = 0;

static void
numb_initialise(void) {
  if (numb_initialised)
    return;

  numb_init(numb_ZERO);
  numb_set_si(&numb_ZERO,0);
 
  numb_init(numb_ONE);
  numb_set_si(&numb_ONE,1);

  numb_initialised = 1;
}

static void
numb_obstack(struct obstack *obs, const eval_t value, 
	     const int radix, int min)
{
  const char *s;

  mpz_t i;
  mpz_init(i);

  mpq_get_num(i,value);
  s = mpz_get_str((char *)0, radix, i);

  if (*s == '-')
    {
      obstack_1grow (obs, '-');
      min--;
      s++;
    }
  for (min -= strlen (s); --min >= 0;)
    obstack_1grow (obs, '0');

  obstack_grow (obs, s, strlen (s));

  mpq_get_den(i,value);
  if (mpz_cmp_si(i,(long)1)!=0) {
    obstack_1grow (obs, ':');
    s = mpz_get_str((char *)0, radix, i);
    obstack_grow (obs, s, strlen (s));
  }

  mpz_clear(i);
}

#define NOISY ""
#define QUIET (char *)0

static void
mpq2mpz(mpz_t z, const eval_t q, const char *noisily)
{
  if (noisily && mpz_cmp_si(mpq_denref(q),(long)1)!=0) {
    M4ERROR((warning_status, 0,
	     _("Loss of precision in eval: %s"),
	     noisily));
  }
  mpz_div(z,mpq_numref(q),mpq_denref(q));
}

static void
mpz2mpq(eval_t q, const mpz_t z)
{
  mpq_set_si(q,(long)0,(unsigned long)1);
  mpq_set_num(q,z);
}

static void
numb_divide(eval_t *x, const eval_t *y)
{
   mpq_t qres;
   mpz_t zres;

   mpq_init(qres);
   mpq_div(qres,*x,*y);

   mpz_init(zres);
   mpz_div(zres,mpq_numref(qres),mpq_denref(qres));
   mpq_clear(qres);

   mpz2mpq(*x,zres);
   mpz_clear(zres);
}

static void
numb_modulo(eval_t *x, const eval_t *y)
{
   mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

   mpz_init(xx);
   mpq2mpz(xx,*x,NOISY);

   mpz_init(yy);
   mpq2mpz(yy,*y,NOISY);

   mpz_init(res);
   mpz_mod(res,xx,yy);

   mpz_clear(xx);
   mpz_clear(yy);

   mpz2mpq(*x,res);
   mpz_clear(res);
}

static void
numb_and(eval_t *x, const eval_t *y)
{
   mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

   mpz_init(xx);
   mpq2mpz(xx,*x,NOISY);

   mpz_init(yy);
   mpq2mpz(yy,*y,NOISY);

   mpz_init(res);
   mpz_and(res,xx,yy);

   mpz_clear(xx);
   mpz_clear(yy);

   mpz2mpq(*x,res);
   mpz_clear(res);
}

static void
numb_ior(eval_t *x, const eval_t *y)
{
   mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

   mpz_init(xx);
   mpq2mpz(xx,*x,NOISY);

   mpz_init(yy);
   mpq2mpz(yy,*y,NOISY);

   mpz_init(res);
   mpz_ior(res,xx,yy);

   mpz_clear(xx);
   mpz_clear(yy);

   mpz2mpq(*x,res);
   mpz_clear(res);
}

static void
numb_eor(eval_t *x, const eval_t *y)
{
   mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

   mpz_init(xx);
   mpq2mpz(xx,*x,NOISY);

   mpz_init(yy);
   mpq2mpz(yy,*y,NOISY);

   mpz_init(res);

#if 0
   mpz_xor(res,xx,yy);
#else  /* 0 */
   /* a^b = (a|b) & !(a&b) */
   {
     mpz_t and_ab, ior_ab, nand_ab;

     mpz_init(ior_ab);
     mpz_ior(ior_ab,xx,yy);

     mpz_init(and_ab);
     mpz_and(and_ab,xx,yy);

     mpz_init(nand_ab);
     mpz_com(nand_ab,and_ab);

     mpz_and(res,ior_ab,nand_ab);

     mpz_clear(and_ab);
     mpz_clear(ior_ab);
     mpz_clear(nand_ab);
   }
#endif /* 0 */

   mpz_clear(xx);
   mpz_clear(yy);

   mpz2mpq(*x,res);
   mpz_clear(res);
}

static void
numb_not(eval_t *x)
{
   mpz_t xx, res;

  /* x should be integral */

   mpz_init(xx);
   mpq2mpz(xx,*x,NOISY);

   mpz_init(res);
   mpz_com(res,xx);

   mpz_clear(xx);

   mpz2mpq(*x,res);
   mpz_clear(res);
}

static void
numb_lshift(eval_t *x, const eval_t *y)
{
   mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

   mpz_init(xx);
   mpq2mpz(xx,*x,NOISY);

   mpz_init(yy);
   mpq2mpz(yy,*y,NOISY);

   mpz_init(res);
   { /* bug: need to determine if y is too big or negative */
     long int exp = mpz_get_si(yy);
     if (exp >= 0) {
       mpz_mul_2exp(res,xx,(unsigned)exp);
     } else {
       mpz_div_2exp(res,xx,(unsigned)-exp);
     }
   }

   mpz_clear(xx);
   mpz_clear(yy);

   mpz2mpq(*x,res);
   mpz_clear(res);
}

static void
numb_rshift(eval_t *x, const eval_t *y)
{
   mpz_t xx, yy, res;

  /* x should be integral */
  /* y should be integral */

   mpz_init(xx);
   mpq2mpz(xx,*x,NOISY);

   mpz_init(yy);
   mpq2mpz(yy,*y,NOISY);

   mpz_init(res);
   { /* bug: need to determine if y is too big or negative */
     long int exp = mpz_get_si(yy);
     if (exp >= 0) {
       mpz_div_2exp(res,xx,(unsigned)exp);
     } else {
       mpz_mul_2exp(res,xx,(unsigned)-exp);
     }
   }

   mpz_clear(xx);
   mpz_clear(yy);

   mpz2mpq(*x,res);
   mpz_clear(res);
}


#else  /* USE_GMP */

static void
numb_initialise(void) 
{
  ;
}


/*------------------------------------------------------------------------.
| The function ntoa () converts VALUE to a signed ascii representation in |
| radix RADIX.								  |
`------------------------------------------------------------------------*/

/* Digits for number to ascii conversions.  */
static char const ntoa_digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static const char *
ntoa (eval_t value, int radix)
{
  boolean negative;
  ueval_t uvalue;
  static char str[256];
  register char *s = &str[sizeof str];

  *--s = '\0';

  if (value < 0)
    {
      negative = TRUE;
      uvalue = (ueval_t) -value;
    }
  else
    {
      negative = FALSE;
      uvalue = (ueval_t) value;
    }

  do
    {
      *--s = ntoa_digits[uvalue % radix];
      uvalue /= radix;
    }
  while (uvalue > 0);

  if (negative)
    *--s = '-';
  return s;
}

static void
numb_obstack(struct obstack *obs, const eval_t value,
	     const int radix, int min)
{
  const char *s = ntoa (value, radix);

  if (*s == '-')
    {
      obstack_1grow (obs, '-');
      min--;
      s++;
    }
  for (min -= strlen (s); --min >= 0;)
    obstack_1grow (obs, '0');

  obstack_grow (obs, s, strlen (s));
}


#endif /* USE_GMP */


static void
numb_pow (eval_t *x, const eval_t *y)
{
  /* y should be integral */

  eval_t ans, yy;

  numb_init(ans);
  numb_set_si(&ans,1);

  numb_init(yy);
  numb_set(yy,*y);

  if (numb_negativep(yy)) {
    numb_invert(*x);
    numb_negate(yy);
  }

  while (numb_positivep(yy)) {
    numb_times(ans,*x);
    numb_decr(yy);
  }
  numb_set(*x,ans);

  numb_fini(ans);
  numb_fini(yy);
}
