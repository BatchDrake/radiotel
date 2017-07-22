/*
  file.c: Implementaton of the IQ file source

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

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "param.h"
#include "source.h"

#define READ_BUF_MAX 1024

RTS_PRIVATE void *
rts_file_open(const rts_params_t *params, struct rts_signal_source_info *info)
{
  FILE *fp;
  const char *path_str;
  const char *fs_str;
  unsigned int fs;
  const char *fc_str;
  int64_t fc;

  if ((path_str = rts_params_get(params, "path")) == NULL) {
    fprintf(stderr, "Cannot open IQ file source: `path' not set\n");
    return NULL;
  }

  if ((fs_str = rts_params_get(params, "fs")) == NULL) {
    fprintf(stderr, "Cannot open IQ file source: `fs' not set\n");
    return NULL;
  }

  if (sscanf(fs_str, "%u", &fs) < 1) {
    fprintf(stderr, "Cannot open IQ file source: wrong sample rate\n");
    return NULL;
  }

  if ((fc_str = rts_params_get(params, "fc")) != NULL) {
    if (sscanf(fc_str, "%lli", &fc) < 1) {
      fprintf(stderr, "Cannot open IQ file source: wrong central frequency\n");
      return NULL;
    }

    info->freq = fc;
  }

  if ((fp = fopen(path_str, "rb")) == NULL) {
    fprintf(
        stderr,
        "Cannot open IQ file source: cannot open file: %s\n",
        strerror(errno));
    return NULL;
  }

  info->samp_rate = fs;

  return fp;
}

RTS_PRIVATE RTSCOUNT
rts_file_acquire(void *handle, RTSCOMPLEX *buffer, RTSCOUNT count)
{
  complex float samples[READ_BUF_MAX];
  RTSCOUNT got;
  unsigned int i;

  if (count > READ_BUF_MAX)
    count = READ_BUF_MAX;

  got = fread(samples, sizeof (complex float), count, (FILE *) handle);

  if (got > 0)
    for (i = 0; i < got; ++i)
      buffer[i] = samples[i];

  if (got >= 0 && got < count)
    fseek((FILE *) handle, 0, SEEK_SET);

  return got;
}

RTS_PRIVATE void
rts_file_close(void *handle)
{
  fclose((FILE *) handle);
}

RTSBOOL
rts_file_source_register(void)
{
  static struct rts_signal_source src =
  {
      .name = "file",
      .open = rts_file_open,
      .acquire = rts_file_acquire,
      .close = rts_file_close
  };

  RTS_TRYCATCH(rts_signal_source_register(&src), return RTS_FALSE);

  return RTS_TRUE;
}
