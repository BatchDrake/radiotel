/*
  source.h: Abstraction of a generic signal source

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

#ifndef _RTSUTIL_SOURCE_H
#define _RTSUTIL_SOURCE_H

#include "common.h"
#include "param.h"

#define RTS_SOURCE_ACQUIRE_RESULT_EOS    0
#define RTS_SOURCE_ACQUIRE_RESULT_ERROR -1

struct rts_signal_source_info {
  unsigned int samp_rate;
  int64_t freq;
};

struct rts_signal_source {
  const char *name;
  RTSFLOAT dr; /* Dynamic range: |max| / |min| */

  void *(*open) (const rts_params_t *params, struct rts_signal_source_info *inf);

  RTSCOUNT (*acquire) (void *hnd, RTSCOMPLEX *buffer, RTSCOUNT count);

  void (*close) (void *hnd);
};

struct rts_signal_source_handle {
  const struct rts_signal_source *src;
  struct rts_signal_source_info info;
  void *handle;
};

typedef struct rts_signal_source_handle rts_srchnd_t;

RTSBOOL rts_signal_source_register(const struct rts_signal_source *src);

const struct rts_signal_source *rts_signal_source_lookup(const char *name);

rts_srchnd_t *rts_source_open(
    const struct rts_signal_source *src,
    const rts_params_t *params);

RTSCOUNT rts_source_acquire(
    rts_srchnd_t *hnd,
    RTSCOMPLEX *buffer,
    RTSCOUNT count);

void rts_source_close(rts_srchnd_t *hnd);

RTSBOOL rts_file_source_register(void);

RTSBOOL rts_register_builtin_sources(void);

#endif /* _RTSUTIL_SOURCE_H */
