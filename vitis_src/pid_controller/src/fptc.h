#pragma once

// source taken from
// https://github.com/mgetka/fptc-lib/blob/master/src/fptc.h
// 
// @note: A few modifications were done to account for sections not used 
// by our project; most notability removing string based methods and scientific 
// calculation to reduce library size. 

/*
 * fptc.h is a 32-bit or 64-bit fixed point numeric library (modified by mgetka)
 *
 * The symbol FPT_BITS, if defined before this library header file
 * is included, determines the number of bits in the data type (its "width").
 * The default width is 32-bit (FPT_BITS=32) and it can be used
 * on any recent C99 compiler. The 64-bit precision (FPT_BITS=64) is
 * available on compilers which implement 128-bit "long long" types. This
 * precision has been tested on GCC 4.2+.
 *
 * The FPT_WBITS symbols governs how many bits are dedicated to the
 * "whole" part of the number (to the left of the decimal point). The larger
 * this width is, the larger the numbers which can be stored in the fpt
 * number. The rest of the bits (available in the FPT_FBITS symbol) are
 * dedicated to the fraction part of the number (to the right of the decimal
 * point).
 *
 * Since the number of bits in both cases is relatively low, many complex
 * functions (more complex than div & mul) take a large hit on the precision
 * of the end result because errors in precision accumulate.
 * This loss of precision can be lessened by increasing the number of
 * bits dedicated to the fraction part, but at the loss of range.
 *
 * Adventurous users might utilize this library to build two data types:
 * one which has the range, and one which has the precision, and carefully
 * convert between them (including adding two number of each type to produce
 * a simulated type with a larger range and precision).
 *
 * The ideas and algorithms have been cherry-picked from a large number
 * of previous implementations available on the Internet.
 * Tim Hartrick has contributed cleanup and 64-bit support patches.
 *
 * == Special notes for the 32-bit precision ==
 * Signed 32-bit fixed point numeric library for the 24.8 format.
 * The specific limits are -8388608.999... to 8388607.999... and the
 * most precise number is 0.00390625. In practice, you should not count
 * on working with numbers larger than a million or to the precision
 * of more than 2 decimal places. Make peace with the fact that PI
 * is 3.14 here. :)
 */
 
 /*
 * Copyright (c) 2017 Michał Getka <michal.getka@gmail.com>
 * This file contains modified version of original fixed point library. Some
 * additional functionalities and improvements were made.
 */
 
/*-
 * Copyright (c) 2010-2012 Ivan Voras <ivoras@freebsd.org>
 * Copyright (c) 2012 Tim Hartrick <tim@edgecast.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef FPT_BITS
#define FPT_BITS  32
#endif

#include <stdint.h>

#if FPT_BITS == 32
typedef int32_t fpt;
typedef int64_t  fptd;
typedef uint32_t fptu;
typedef uint64_t fptud;
#elif FPT_BITS == 64
typedef int64_t fpt;
typedef __int128_t fptd;
typedef uint64_t fptu;
typedef __uint128_t fptud;
#else
#error "FPT_BITS must be equal to 32 or 64"
#endif

#ifndef FPT_WBITS
#define FPT_WBITS  17
#endif

#if FPT_WBITS >= FPT_BITS
#error "FPT_WBITS must be less than or equal to FPT_BITS"
#endif

#define FPT_VCSID "$Id$"

#define FPT_FBITS  (FPT_BITS - FPT_WBITS)
#define FPT_FMASK  (((fpt)1 << FPT_FBITS) - 1)

#define fl2fpt(R) ((fpt)((R) * FPT_ONE + ((R) >= 0 ? 0.5 : -0.5)))
#define i2fpt(I) ((fptd)(I) << FPT_FBITS)
#define fpt2i(F) ((F) >> FPT_FBITS)

#define i2fpt_norm(I,n) (               \
    (FPT_FBITS - n) >= 0 ?              \
      ((fptd)(I) << (FPT_FBITS - n)) :  \
      ((fptd)(I) >> -(FPT_FBITS - n))   \
  )
#define fpt2i_norm(F,n) (               \
    (FPT_FBITS - n) >= 0 ?              \
      ((F) >> (FPT_FBITS - n)) :        \
      ((F) << -(FPT_FBITS - n))         \
  )
#define fpt_norm(F,from,to) (           \
    (from - to) >= 0 ?                  \
      ((fptd)(F) << (from - to)) :      \
      ((fptd)(F) >> -(from - to))       \
  )                                     \

#define fpt_xadd(A,B) ((A) + (B))
#define fpt_xsub(A,B) ((A) - (B))
#define fpt_xmul(A,B)                   \
  ((fpt)(((fptd)(A) * (fptd)(B)) >> FPT_FBITS))
#define fpt_xdiv(A,B)                   \
  ((fpt)(((fptd)(A) << FPT_FBITS) / (fptd)(B)))
#define fpt_fracpart(A) ((fpt)(A) & FPT_FMASK)

#define FPT_ONE       ((fpt)((fpt)1 << FPT_FBITS))
#define FPT_ZERO      ((fpt)0)
#define FPT_MINUS_ONE (-FPT_ONE)
#define FPT_ONE_HALF  (FPT_ONE >> 1)
#define FPT_TWO       (FPT_ONE + FPT_ONE)
#define FPT_MAX       ((fpt)((fptu)~0 >> 1))
#define FPT_MIN       (~FPT_MAX)
#define FPT_ABS_MAX   FPT_MAX
#define FPT_ABS_MIN   ((fpt)1)
#define FPT_PI        fl2fpt(3.14159265358979323846)
#define FPT_TWO_PI    fl2fpt(2 * 3.14159265358979323846)
#define FPT_HALF_PI   fl2fpt(3.14159265358979323846 / 2)
#define FPT_E         fl2fpt(2.7182818284590452354)

/* Following block of code defines default overflow handling macros for math
 * operations. */
 
#ifndef _fpt_add_overflow_handler
#define _fpt_add_overflow_handler return FPT_MAX;
#endif
#ifndef _fpt_add_underflow_handler
#define _fpt_add_underflow_handler return FPT_MIN;
#endif

#ifndef _fpt_sub_overflow_handler
#define _fpt_sub_overflow_handler return FPT_MAX;
#endif
#ifndef _fpt_sub_underflow_handler
#define _fpt_sub_underflow_handler return FPT_MIN;
#endif

#ifndef _fpt_mul_overflow_handler
#define _fpt_mul_overflow_handler return FPT_MAX;
#endif
#ifndef _fpt_mul_underflow_handler
#define _fpt_mul_underflow_handler return FPT_MIN;
#endif

#ifndef _fpt_div_overflow_handler
#define _fpt_div_overflow_handler return FPT_MAX;
#endif
#ifndef _fpt_div_underflow_handler
#define _fpt_div_underflow_handler return FPT_MIN;
#endif

#define fpt_abs(A) ((A) < 0 ? -(A) : (A))

/* fpt is meant to be usable in environments without floating point support
 * (e.g. microcontrollers, kernels), so we can't use floating point types directly.
 * However floats can occur in places that will be optimised out during compilation. */
#define fpt2fl(T) ((float) ((T)*((float)(1)/(float)(1 << FPT_FBITS))))

/* Adds two fpt numbers, returns the result. */
static inline fpt
fpt_add(fpt A, fpt B)
{
  
  #ifdef FPT_ADD_OVERFLOW_HANDLING
  if ((A > 0) && (B > FPT_MAX - A)) _fpt_add_overflow_handler;
  if ((A < 0) && (B < FPT_MIN - A)) _fpt_add_underflow_handler;
  #endif
  
  return ((A) + (B));
}


/* Subtracts two fpt numbers, returns the result. */
static inline fpt
fpt_sub(fpt A, fpt B)
{
  
  #ifdef FPT_SUB_OVERFLOW_HANDLING
  if ((A < 0) && (B > FPT_MAX + A)) _fpt_sub_overflow_handler;
  if ((A > 0) && (B < FPT_MIN + A)) _fpt_sub_underflow_handler;
  #endif
  
  return ((A) - (B));
}


/* Multiplies two fpt numbers, returns the result. */
static inline fpt
fpt_mul(fpt A, fpt B)
{
  
  #ifdef FPT_MUL_OVERFLOW_HANDLING
  if (A < FPT_ZERO && B < FPT_ZERO) {
    if ((fptd)A < ((fptd)FPT_MAX << FPT_FBITS) / (fptd)B) _fpt_mul_overflow_handler;
  } else if (B > FPT_ZERO) {
    if ((fptd)A > ((fptd)FPT_MAX << FPT_FBITS) / (fptd)B) _fpt_mul_overflow_handler;
    if ((fptd)A < ((fptd)FPT_MIN << FPT_FBITS) / (fptd)B) _fpt_mul_underflow_handler;
  } else if (B < FPT_ZERO) {
    if ((fptd)A > ((fptd)FPT_MIN << FPT_FBITS) / (fptd)B) _fpt_mul_underflow_handler;
  }
  #endif
  
  return (((fptd)A * (fptd)B) >> FPT_FBITS);
}


/* Divides two fpt numbers, returns the result. */
static inline fpt
fpt_div(fpt A, fpt B)
{
  
  #ifdef FPT_DIV_OVERFLOW_HANDLING
  /* The purpose of overflow handling mechanism is not to detect zero division.
   * So it is not checked if A is zero. If this is possible, it should be
   * handled outside the library, and if it is, it would generate excessive
   * computation overhead when doing this in here.*/
  if (fpt_abs(B) <= FPT_ONE) {
    if (A < FPT_ZERO && B < FPT_ZERO) {
      if ((fptd)A << FPT_FBITS < (fptd)FPT_MAX * (fptd)B) _fpt_div_overflow_handler;
    } else if (B > FPT_ZERO) {
      if ((fptd)A << FPT_FBITS > (fptd)FPT_MAX * (fptd)B) _fpt_div_overflow_handler;
      if ((fptd)A << FPT_FBITS < (fptd)FPT_MIN * (fptd)B) _fpt_div_underflow_handler;
    } else if (B < FPT_ZERO) {
      if ((fptd)A << FPT_FBITS > (fptd)FPT_MIN * (fptd)B) _fpt_div_underflow_handler;
    }
  }
  #endif
  
  return (((fptd)A << FPT_FBITS) / (fptd)B);
}

/*
 * Note: adding and substracting fpt numbers can be done by using
 * the regular integer operators + and -.
 */

static inline int
_pow(int x, unsigned int y) {
  
  unsigned int i;
  int ret = 1;
  
  for (i = 0; i<y; i++)
    ret *= x;
  
  return ret;
}







