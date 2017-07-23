/*
  spectrogram.c: Implementation of the spectrum feature

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

#include <math.h>

#include "spectrogram.h"

rts_spectrogram_t *
rts_spectrogram_new(rts_srchnd_t *hnd, struct rts_spectrogram_params *params)
{
  rts_spectrogram_t *new = NULL;

  RTS_TRYCATCH(new = calloc(1, sizeof (rts_spectrogram_t)), goto fail);

  new->params = *params;
  new->handle = hnd;
  new->frames = ceil((params->avg_time * hnd->info.samp_rate) / params->bins);
  new->total_samples = params->bins * new->frames;

  RTS_TRYCATCH(
      new->window = fftw_malloc(params->bins * sizeof(RTS_FFTW(_complex))),
      goto fail);

  RTS_TRYCATCH(
      new->fft = fftw_malloc(params->bins * sizeof(RTS_FFTW(_complex))),
      goto fail);

  RTS_TRYCATCH(
      new->fft_plan = RTS_FFTW(_plan_dft_1d)(
          params->bins,
          new->window,
          new->fft,
          FFTW_FORWARD,
          FFTW_ESTIMATE),
      goto fail);

  RTS_TRYCATCH(
      new->spectrum = calloc(sizeof(RTSCOMPLEX), params->bins),
      goto fail);

  return new;

fail:
  if (new != NULL)
    rts_spectrogram_destroy(new);

  return NULL;
}

void
rts_spectrogram_destroy(rts_spectrogram_t *spect)
{
  if (spect->fft_plan != NULL)
    RTS_FFTW(_destroy_plan)(spect->fft_plan);

  if (spect->window != NULL)
    fftw_free(spect->window);

  if (spect->fft != NULL)
    fftw_free(spect->fft);

  if (spect->spectrum != NULL)
    free(spect->spectrum);

  free(spect);
}

RTSBOOL
rts_spectrogram_acquire(rts_spectrogram_t *spect)
{
  RTSCOUNT needed;
  RTSCOUNT got;
  RTSFLOAT psd;
  int i;

  if (rts_spectrogram_complete(spect))
    return RTS_TRUE;

  needed = spect->params.bins - spect->window_ptr;

  got = rts_source_acquire(
      spect->handle,
      spect->window + spect->window_ptr,
      needed);

  switch (got) {
    case RTS_SOURCE_ACQUIRE_RESULT_EOS:
      /* TODO: Return half-integrated spectrum */
      fprintf(stderr, "spectrogram: end of stream!\n");
      return RTS_FALSE;

    case RTS_SOURCE_ACQUIRE_RESULT_ERROR:
      fprintf(stderr, "spectrogram: source error\n");
      return RTS_FALSE;
  }

  spect->window_ptr  += got;
  spect->got_samples += got;

  if (spect->window_ptr == spect->params.bins) {
    /* Perform FFT in the current window */
    RTS_FFTW(_execute)(spect->fft_plan);

    spect->min = INFINITY;
    spect->max = -INFINITY;

    /* Save power spectrum */
    for (i = 0; i < spect->params.bins; ++i) {
      psd = creal(spect->fft[i] * conj(spect->fft[i])) / spect->params.bins;

      if (spect->frame_count == 0)
        spect->spectrum[i] = psd; /* If reading first frame, store directly */
      else
        spect->spectrum[i] += psd; /* Otherwise, accumulate */

      /* In last iteration before reset, update limits */
      if (i > 1 && i < (spect->params.bins - 2)) {
        if (spect->spectrum[i] < spect->min)
          spect->min = spect->spectrum[i];
        if (spect->spectrum[i] > spect->max)
          spect->max = spect->spectrum[i];
      }
    }

    /* Reset window pointer, increment frame counter */
    spect->window_ptr = 0;
    ++spect->frame_count;
  }

  return RTS_TRUE;
}

RTSBOOL
rts_spectrogram_complete(const rts_spectrogram_t *spect)
{
  return spect->frame_count == spect->frames;
}

void
rts_spectrogram_reset(rts_spectrogram_t *spect)
{
  spect->got_samples = 0;
  spect->frame_count = 0;
  spect->window_ptr = 0;
  ++spect->reset_count;
}

const RTSFLOAT *
rts_spectrogram_get_cumulative(const rts_spectrogram_t *spect)
{
  return spect->spectrum;
}

RTSCOUNT
rts_spectrogram_get_frame_count(const rts_spectrogram_t *spect)
{
  return spect->frame_count;
}

RTSFLOAT
rts_spectrogram_get_progress(const rts_spectrogram_t *spect)
{
  return (RTSFLOAT) spect->got_samples / (RTSFLOAT) spect->total_samples;
}

void
rts_spectrogram_get_range(
    const rts_spectrogram_t *spect,
    RTSFLOAT *min,
    RTSFLOAT *max,
    RTSFLOAT *f_lo,
    RTSFLOAT *f_hi)
{
  *min = spect->min;
  *max = spect->max;

  *f_lo = spect->handle->info.freq - spect->handle->info.samp_rate / 2;
  *f_hi = spect->handle->info.freq + spect->handle->info.samp_rate / 2;
}

RTSBOOL
rts_spectrogram_dump_matlab(const rts_spectrogram_t *spect, const char *pfx)
{
  char *fullpath = NULL;
  FILE *fp = NULL;
  const RTSFLOAT *spectrum = NULL;
  unsigned int count;
  unsigned int i;
  RTSBOOL ok = RTS_FALSE;

  RTS_TRYCATCH(
      fullpath = strbuild("%s_%05d.m", pfx, spect->reset_count),
      goto done);

  RTS_TRYCATCH(fp = fopen(fullpath, "w"), goto done);

  RTS_TRYCATCH(fprintf(fp, "spectrum = [") > 0, goto done);

  count    = rts_spectrogram_get_frame_count(spect);
  spectrum = rts_spectrogram_get_cumulative(spect);

  for (i = 0; i < spect->params.bins; ++i) {
    RTS_TRYCATCH(
        fprintf(fp, "%s\n%.9e", i == 0 ? "" : ";", spectrum[i] / count) > 0,
        goto done);
  }

  RTS_TRYCATCH(fprintf(fp, "\n];") > 0, goto done);

  ok = RTS_TRUE;

done:
  if (fullpath != NULL)
    free(fullpath);

  if (fp != NULL)
    fclose(fp);

  return ok;
}
