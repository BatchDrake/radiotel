/*
  alsa.c: Soundcard signal source through ALSA

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


#include "alsa.h"

void
alsa_state_destroy(struct alsa_state *state)
{
  if (state->handle != NULL)
    snd_pcm_close(state->handle);

  free(state);
}

struct alsa_state *
alsa_state_new(const struct alsa_params *params)
{
  struct alsa_state *new = NULL;
  snd_pcm_hw_params_t *hw_params = NULL;
  int err = 0;
  int dir;
  unsigned int rate;
  RTSBOOL ok = RTS_FALSE;

  RTS_TRYCATCH(new = calloc(1, sizeof(struct alsa_state)), goto done);

  new->fc = params->fc;
  new->dc_remove = params->dc_remove;

  RTS_TRYCATCH(
      (err = snd_pcm_open(
          &new->handle,
          params->device,
          SND_PCM_STREAM_CAPTURE,
          0)) >= 0,
      goto done);

  RTS_TRYCATCH(
      (err = snd_pcm_hw_params_malloc(&hw_params)) >= 0,
      goto done);

  RTS_TRYCATCH(
      (err = snd_pcm_hw_params_any(new->handle, hw_params)) >= 0,
      goto done);

  RTS_TRYCATCH(
      (err = snd_pcm_hw_params_set_access(
          new->handle,
          hw_params,
          SND_PCM_ACCESS_RW_INTERLEAVED)) >= 0,
      goto done);

  RTS_TRYCATCH(
      (err = snd_pcm_hw_params_set_format(
          new->handle,
          hw_params,
          SND_PCM_FORMAT_S16_LE)) >= 0,
      goto done);

  rate = params->samp_rate;

  RTS_TRYCATCH(
      (err = snd_pcm_hw_params_set_rate_near(
          new->handle,
          hw_params,
          &rate,
          0)) >= 0,
      goto done);


  new->samp_rate = rate;

  RTS_TRYCATCH(
      (err = snd_pcm_hw_params_set_channels(
          new->handle,
          hw_params,
          1)) >= 0,
      goto done);

  RTS_TRYCATCH(
      (err = snd_pcm_hw_params(new->handle, hw_params)) >= 0,
      goto done);

  RTS_TRYCATCH(
      (err = snd_pcm_prepare(new->handle)) >= 0,
      goto done);

  ok = RTS_TRUE;

done:
  if (err < 0)
    fprintf(
        stderr,
        "ALSA source initialization failed: %s\n",
        snd_strerror(err));

  if (hw_params != NULL)
    snd_pcm_hw_params_free(hw_params);

  if (!ok && new != NULL) {
    alsa_state_destroy(new);
    new = NULL;
  }

  return new;
}

/************************ RTS Interface Callbacks ****************************/

RTS_PRIVATE void *
rts_alsa_open(const rts_params_t *params, struct rts_signal_source_info *info)
{
  struct alsa_state *state = NULL;
  struct alsa_params alsa_params = alsa_params_INITIALIZER;
  const char *str;

  if ((str = rts_params_get(params, "fs")) != NULL)
    if (sscanf(str, "%u", &alsa_params.samp_rate) < 1) {
      fprintf(
          stderr,
          "ALSA error: Cannot open ALSA file source: wrong sample rate\n");
      return NULL;
    }

  info->samp_rate = alsa_params.samp_rate;

  if ((str = rts_params_get(params, "fc")) == NULL) {
    fprintf(
        stderr,
        "ALSA error: Cannot open BladeRF file source: `fc' not set\n");
    goto fail;
  }

  if (sscanf(str, "%lli", &info->freq) < 1) {
    fprintf(
        stderr,
        "ALSA error: Cannot open BladeRF file source: wrong central frequency\n");
    return NULL;
  }

  if ((state = alsa_state_new(&alsa_params)) == NULL) {
    fprintf(
        stderr,
        "ALSA error: Create alsa state failed\n");
    goto fail;
  }

  return state;

fail:
  if (state != NULL)
    alsa_state_destroy(state);

  return NULL;
}

RTS_PRIVATE RTSCOUNT
rts_alsa_acquire(void *handle, RTSCOMPLEX *buffer, RTSCOUNT count)
{
  int status;
  int i;
  RTSCOMPLEX samp;
  struct alsa_state *state = (struct alsa_state *) handle;

  count = MIN(count, ALSA_INTEGER_BUFFER_SIZE);

  count = snd_pcm_readi(state->handle, state->buffer, count);
  if (count > 0) {
    /*
     * ALSA does not seem to allow to read FLOAT64_LE directly. We have
     * to transform the integer samples manually
     */

    if (state->dc_remove) {
      for (i = 0; i < count; ++i) {
        samp = state->buffer[i] / 32768.0;
        buffer[i] = samp - state->last;
        state->last = samp;
      }
    } else {
      for (i = 0; i < count; ++i)
        buffer[i] = state->buffer[i] / 32768.0;
    }
  }

  return count;
}

RTS_PRIVATE void
rts_alsa_close(void *handle)
{
  alsa_state_destroy((struct alsa_state *) handle);
}

RTSBOOL
rts_alsa_source_register(void)
{
  static struct rts_signal_source src =
  {
      .name = "alsa",
      .open = rts_alsa_open,
      .acquire = rts_alsa_acquire,
      .close = rts_alsa_close
  };

  RTS_TRYCATCH(rts_signal_source_register(&src), return RTS_FALSE);

  return RTS_TRUE;
}


