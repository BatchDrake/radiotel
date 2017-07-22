/*
  common.h: Generic types and definitions for the radiotelescope utility
  library.

  Copyright (C) 2017 Gonzalo José Carracedo Carballal <BatchDrake@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#ifndef _RTSUTIL_COMMON_H
#define _RTSUTIL_COMMON_H

#include <math.h>
#include <complex.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdio.h> /* For ASSERT */

#include <util.h> /* For token pasting */

/* TODO: Add defines for FFTW functions */

#define RTS_ENSURE(expr, code)  \
  if (!(expr)) {                \
    code;                       \
  }

#define RTS_ASSERT(expr)        \
    RTS_ENSURE(                 \
        expr,                   \
        do {                    \
          fprintf(              \
              stderr,           \
              "*** Internal error: assertion \"%s\" in %s:%d failed\n", \
              STRINGIFY(expr),  \
              __FILE__,         \
              __LINE__);        \
          abort();              \
        } while (0))

#define RTS_TRYCATCH(expr, act) \
    RTS_ENSURE(                 \
        expr,                   \
        do {                    \
          fprintf(              \
              stderr,           \
              "Unhandled exception in %s:%d: !\"%s\"\n", \
              __FILE__,         \
              __LINE__,         \
              STRINGIFY(expr)); \
          act;                  \
        } while (0))

#define RTS_PRIVATE static

enum rtsbool {
  RTS_FALSE = 0,
  RTS_TRUE
};

typedef enum rtsbool RTSBOOL;

typedef double RTSFLOAT;

typedef complex double RTSCOMPLEX;

typedef uint32_t RTSCOUNT;

#endif /* _RTSUTIL_COMMON_H */
