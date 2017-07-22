/*
  param.c: Parameter management

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

#include <string.h>
#include "common.h"
#include "param.h"

RTS_PRIVATE void
rts_param_destroy(struct rts_param *param)
{
  if (param->name != NULL)
    free(param->name);

  if (param->value != NULL)
    free(param->value);

  free(param);
}

RTS_PRIVATE struct rts_param *
rts_param_new(const char *name, const char *value)
{
  struct rts_param *new = NULL;

  RTS_TRYCATCH(new = calloc(1, sizeof (struct rts_param)), goto fail);

  RTS_TRYCATCH(new->name = strdup(name), goto fail);
  RTS_TRYCATCH(new->value = strdup(value), goto fail);

  return new;

fail:
  if (new != NULL)
    rts_param_destroy(new);

  return NULL;
}

void
rts_params_destroy(rts_params_t *params)
{
  unsigned int i;

  for (i = 0;  i < params->param_count; ++i)
    if (params->param_list[i] != NULL)
      rts_param_destroy(params->param_list[i]);

  if (params->param_list != NULL)
    free(params->param_list);

  free(params);
}

rts_params_t *
rts_params_new(void)
{
  rts_params_t *new = NULL;

  RTS_TRYCATCH(new = calloc(1, sizeof(rts_params_t)), goto fail);

  return new;

fail:
  if (new != NULL)
    rts_params_destroy(new);

  return NULL;
}

RTS_PRIVATE struct rts_param **
rts_params_lookup_ptr(const rts_params_t *params, const char *name)
{
  unsigned int i;

  for (i = 0;  i < params->param_count; ++i)
    if (params->param_list[i] != NULL)
      if (strcmp(params->param_list[i]->name, name) == 0)
        return params->param_list + i;

  return NULL;
}

RTSBOOL
rts_params_set(rts_params_t *params, const char *name, const char *value)
{
  struct rts_param **slot;
  char *dup = NULL;
  struct rts_param *param = NULL;
  RTSBOOL  ok = RTS_FALSE;

  slot = rts_params_lookup_ptr(params, name);

  if (value == NULL) {
    /* Value set to NULL. Destroy this param and place a gap */
    RTS_TRYCATCH(slot != NULL, goto done);

    rts_param_destroy(*slot);
    *slot = NULL;
  } else {
    if (slot != NULL) {
      /* Param alreuedy defined. Replace current value */
      RTS_TRYCATCH(dup = strdup(value), goto done);

      free((*slot)->value);
      (*slot)->value = dup;
    } else {
      /* Param does not exists. Create and append */
      RTS_TRYCATCH(param = rts_param_new(name, value), goto done);
      RTS_TRYCATCH(
          PTR_LIST_APPEND_CHECK(params->param, param) != -1,
          goto done);
    }
  }

  ok = RTS_TRUE;

done:
  if (!ok) {
    if (param != NULL)
      rts_param_destroy(param);
  }
  return ok;
}

const char *
rts_params_get(const rts_params_t *params, const char *name)
{
  struct rts_param **slot;

  if ((slot = rts_params_lookup_ptr(params, name)) != NULL)
    return (*slot)->value;

  return NULL;
}

RTSBOOL
rts_params_parse(rts_params_t *params, const char *str)
{
  char *p, *q; /* String start and end */
  char *dup = NULL;
  const char *name;
  RTSBOOL ok = RTS_FALSE;

  RTS_TRYCATCH(dup = strdup(str), goto done);

  p = dup;

  while (*p != '\0') {
    /* Extract parameter name */
    if ((q = strchr(p, '=')) == NULL)
      goto done;

    name = p;
    *q++ = '\0';

    /* Extract value */
    p = q;

    if ((q = strchr(p, ',')) == NULL) {
      q = p + strlen(p);
    } else {
      *q++ = '\0';
    }

    RTS_TRYCATCH(rts_params_set(params, name, p), goto done);

    p = q;
  }

  ok = RTS_TRUE;

done:
  if (dup != NULL)
    free(dup);

  return ok;
}
