/* GNU m4 -- A simple macro processor
   Copyright 1995, 1998 Free Software Foundation, Inc.
  
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

/* This file contains the functions to evaluate integer or multiple
 * precision expressions for the "eval" macro.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "m4private.h"

/* number should be at least 32 bits.  */
/* use GNU long long int if available */
#if defined(SIZEOF_LONG_LONG_INT) && SIZEOF_LONG_LONG_INT > 0
typedef long long int number;
typedef unsigned long long int unumber;
#else
typedef long int number;
typedef unsigned long int unumber;
#endif
 
#define int2numb(i) ((number)(i))
#define numb2int(n) ((n))

#define numb_set(ans,x) ((ans) = x)
#define numb_set_si(ans,si) (*(ans) = int2numb(si))

#define numb_init(x) x=((number)0)
#define numb_fini(x)

#define numb_decr(n) (n) -= 1

#define numb_ZERO ((number)0)
#define numb_ONE  ((number)1)

#define numb_zerop(x)     ((x) == numb_ZERO)
#define numb_positivep(x) ((x) >  numb_ZERO)
#define numb_negativep(x) ((x) <  numb_ZERO)


#define numb_eq(x,y) ((x) = ((x) == (y)))
#define numb_ne(x,y) ((x) = ((x) != (y)))
#define numb_lt(x,y) ((x) = ((x) <  (y)))
#define numb_le(x,y) ((x) = ((x) <= (y)))
#define numb_gt(x,y) ((x) = ((x) >  (y)))
#define numb_ge(x,y) ((x) = ((x) >= (y)))

#define numb_lnot(x)   ((x) = (! (x)))
#define numb_lior(x,y) ((x) = ((x) || (y)))
#define numb_land(x,y) ((x) = ((x) && (y)))

#define numb_not(x)   (*(x) = int2numb(~numb2int(*(x))))
#define numb_eor(x,y) (*(x) = int2numb(numb2int(*(x)) ^ numb2int(*(y))))
#define numb_ior(x,y) (*(x) = int2numb(numb2int(*(x)) | numb2int(*(y))))
#define numb_and(x,y) (*(x) = int2numb(numb2int(*(x)) & numb2int(*(y))))

#define numb_plus(x,y)  ((x) = ((x) + (y)))
#define numb_minus(x,y) ((x) = ((x) - (y)))
#define numb_negate(x)  ((x) = (- (x)))

#define numb_times(x,y)  ((x) = ((x) * (y)))
#define numb_ratio(x,y)  ((x) = ((x) / ((y))))
#define numb_divide(x,y) (*(x) = (*(x) / (*(y))))
#define numb_modulo(x,y) (*(x) = (*(x) % *(y)))
#define numb_invert(x)   ((x) = 1 / (x))

#define numb_lshift(x,y) (*(x) = (*(x) << *(y)))
#define numb_rshift(x,y) (*(x) = (*(x) >> *(y)))

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
ntoa (number value, int radix)
{
  boolean negative;
  unumber uvalue;
  static char str[256];
  register char *s = &str[sizeof str];

  *--s = '\0';

  if (value < 0)
    {
      negative = TRUE;
      uvalue = (unumber) -value;
    }
  else
    {
      negative = FALSE;
      uvalue = (unumber) value;
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
numb_obstack(struct obstack *obs, const number value,
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

#include "evalparse.c"
