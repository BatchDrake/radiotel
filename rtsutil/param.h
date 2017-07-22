/*
  param.h: Parameter management

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

#ifndef _RTSUTIL_PARAM_H
#define _RTSUTIL_PARAM_H

#include <util.h>
#include "common.h"

struct rts_param {
  char *name;
  char *value;
};

struct rts_params {
  PTR_LIST(struct rts_param, param);
};

typedef struct rts_params rts_params_t;

rts_params_t *rts_params_new(void);

RTSBOOL rts_params_set(rts_params_t *params, const char *name, const char *val);

const char *rts_params_get(const rts_params_t *params, const char *name);

RTSBOOL rts_params_parse(rts_params_t *params, const char *str);

void rts_params_destroy(rts_params_t *params);

#endif /* _RTSUTIL_PARAM_H */
