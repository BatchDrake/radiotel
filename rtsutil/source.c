/*
  source.c: Abstraction of a generic signal source

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

#include "source.h"

PTR_LIST_CONST_PRIVATE(struct rts_signal_source, source);

const struct rts_signal_source *
rts_signal_source_lookup(const char *name)
{
  unsigned int i;

  for (i = 0; i < source_count; ++i)
    if (strcmp(source_list[i]->name, name) == 0)
      return source_list[i];

  return NULL;
}

RTSBOOL
rts_signal_source_register(const struct rts_signal_source *src)
{
  RTS_ASSERT(src->name != NULL);
  RTS_ASSERT(src->open != NULL);
  RTS_ASSERT(src->acquire != NULL);
  RTS_ASSERT(src->close != NULL);

  RTS_ASSERT(rts_signal_source_lookup(src->name) == NULL);

  RTS_TRYCATCH(
      PTR_LIST_APPEND_CHECK(source, (void *) src) != -1,
      return RTS_FALSE);

  return RTS_TRUE;
}

rts_srchnd_t *
rts_source_open(const struct rts_signal_source *src, const rts_params_t *params)
{
  rts_srchnd_t *hnd = NULL;

  RTS_TRYCATCH(hnd = malloc(sizeof (rts_srchnd_t)), goto fail);

  hnd->src = src;

  RTS_TRYCATCH(hnd->handle = (src->open) (params, &hnd->info), goto fail);

  return hnd;

fail:
  if (hnd != NULL)
    free(hnd);

  return NULL;
}

RTSCOUNT
rts_source_acquire(rts_srchnd_t *hnd, RTSCOMPLEX *buffer, RTSCOUNT count)
{
  return (hnd->src->acquire) (hnd->handle, buffer, count);
}

void
rts_source_close(rts_srchnd_t *hnd)
{
  (hnd->src->close) (hnd->handle);

  free(hnd);
}

RTSBOOL
rts_register_builtin_sources(void)
{
  RTSBOOL ok = RTS_FALSE;

  RTS_TRYCATCH(rts_file_source_register(), goto done);

  ok = RTS_TRUE;

done:
  return ok;
}
