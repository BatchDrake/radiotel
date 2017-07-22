/*
  spectrogram.h: Spectrum computation using FFT

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

#ifndef _RTSUTIL_SPECTROGRAM_H
#define _RTSUTIL_SPECTROGRAM_H

#include "source.h"

#include <complex.h>
#include <fftw3.h>

#define RTS_SOURCE_FFTW_PREFIX fftw
#define RTS_FFTW(method) JOIN(RTS_SOURCE_FFTW_PREFIX, method)

struct rts_spectrogram_params {
  RTSCOUNT bins;
  RTSFLOAT avg_time;
};

struct rts_spectrogram {
  struct rts_spectrogram_params params;
  rts_srchnd_t *handle;
  RTSCOUNT frames;

  RTS_FFTW(_complex) *window;
  RTS_FFTW(_plan) fft_plan;
  RTS_FFTW(_complex) *fft;

  RTSFLOAT *spectrum; /* Averaged spectrum */

  RTSCOUNT frame_count;
  RTSCOUNT window_ptr;

  /* Statistical properties */
  RTSCOUNT total_samples;
  RTSCOUNT got_samples;
  RTSCOUNT reset_count;

  /* Spectrum range */
  RTSFLOAT min;
  RTSFLOAT max;
};

typedef struct rts_spectrogram rts_spectrogram_t;

RTS_PRIVATE inline RTSCOUNT
rts_spectrogram_get_reset_count(const rts_spectrogram_t *spect)
{
  return spect->reset_count;
}

RTS_PRIVATE inline RTSCOUNT
rts_spectrogram_get_total_samples(const rts_spectrogram_t *spect)
{
  return spect->total_samples;
}

RTS_PRIVATE inline RTSCOUNT
rts_spectrogram_get_got_samples(const rts_spectrogram_t *spect)
{
  return spect->got_samples;
}

RTS_PRIVATE inline RTSCOUNT
rts_spectrogram_get_samp_rate(const rts_spectrogram_t *spect)
{
  return spect->handle->info.samp_rate;
}

rts_spectrogram_t *rts_spectrogram_new(
    rts_srchnd_t *hnd,
    struct rts_spectrogram_params *params);

void rts_spectrogram_destroy(rts_spectrogram_t *spect);

RTSBOOL rts_spectrogram_acquire(rts_spectrogram_t *spect);

RTSBOOL rts_spectrogram_complete(const rts_spectrogram_t *spect);

void rts_spectrogram_reset(rts_spectrogram_t *spect);

const RTSFLOAT *rts_spectrogram_get_cumulative(const rts_spectrogram_t *spect);

RTSCOUNT rts_spectrogram_get_frame_count(const rts_spectrogram_t *spect);

RTSFLOAT rts_spectrogram_get_progress(const rts_spectrogram_t *spect);

void rts_spectrogram_get_range(
    const rts_spectrogram_t *spect,
    RTSFLOAT *min,
    RTSFLOAT *max,
    RTSFLOAT *f_lo,
    RTSFLOAT *f_hi);

RTSBOOL rts_spectrogram_dump_matlab(
    const rts_spectrogram_t *spect,
    const char *pfx);

#endif /* _RTSUTIL_SPECTROGRAM_H */
