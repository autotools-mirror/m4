/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 90, 91, 92, 93, 94 Free Software Foundation, Inc.

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

#include "m4.h"

#ifdef USE_GMP
#define NUMB_MP 1
#endif

#ifdef USE_GMP
#include "gmp.h"

/* eval_t should be at least 32 bits.  */
typedef mpq_t eval_t;

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
{ eval_t T; mpq_init(T); f1(T,x);   mpq_set(x,T); mpq_clear(T); }
#define reduce2(f2,x,y) \
{ eval_t T; mpq_init(T); f2(T,(x),(y)); mpq_set((x),T); mpq_clear(T); }

#define numb_plus(x,y)  reduce2(mpq_add,x,y)
#define numb_minus(x,y) reduce2(mpq_sub,x,y)
#define numb_negate(x)  reduce1(mpq_neg,x)

#define numb_times(x,y) reduce2(mpq_mul,x,y)
#define numb_ratio(x,y) reduce2(mpq_div,x,y)
#define numb_invert(x)  reduce1(mpq_inv,x)

#define numb_decr(n) numb_minus(n,numb_ONE)


#else  /* not USE_GMP */

/* eval_t should be at least 32 bits.  */
/* use GNU long long int if available */
#if defined(SIZEOF_LONG_LONG_INT) && SIZEOF_LONG_LONG_INT > 0
typedef long long int eval_t;
typedef unsigned long long int ueval_t;
#else
typedef long int eval_t;
typedef unsigned long int ueval_t;
#endif
 
#define int2numb(i) ((eval_t)(i))
#define numb2int(n) ((n))

#define numb_set(ans,x) ((ans) = x)
#define numb_set_si(ans,si) (*(ans) = int2numb(si))

#define numb_init(x) x=((eval_t)0)
#define numb_fini(x)

#define numb_decr(n) (n) -= 1

#define numb_ZERO ((eval_t)0)
#define numb_ONE  ((eval_t)1)

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

#endif /* USE_GMP */
