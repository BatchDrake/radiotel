/*
  bladerf.c: BladeRF signal source

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

#include "bladerf.h"

RTS_PRIVATE void
bladeRF_state_destroy(struct bladeRF_state *state)
{
  if (state->dev != NULL)
    bladerf_close(state->dev);

  if (state->buffer != NULL)
    free(state->buffer);

  free(state);
}

RTS_PRIVATE RTSBOOL
bladeRF_state_init_sync(struct bladeRF_state *state)
{
  int status;

  status = bladerf_sync_config(
      state->dev,
      BLADERF_MODULE_RX,
      BLADERF_FORMAT_SC16_Q11,
      16,
      state->params.bufsiz,
      8,
      3500);
  if (status != 0) {
    fprintf(
        stderr,
        "BladeRF error: Failed to configure RX sync interface: %s\n",
        bladerf_strerror(status));
    return RTS_FALSE;
  }

  return RTS_TRUE;
}

RTS_PRIVATE struct bladeRF_state *
bladeRF_state_new(const struct bladeRF_params *params)
{
  struct bladeRF_state *new = NULL;
  struct bladerf_devinfo dev_info;
  unsigned int actual_samp_rate;
  unsigned int actual_fc;
  unsigned int actual_bw;

  bladerf_xb300_amplifier amp;
  int status;

  if ((new = calloc(1, sizeof (struct bladeRF_state))) == NULL)
    goto fail;

  new->params = *params;

  /* 1 sample: 2 components (I & Q) */
  if ((new->buffer = malloc(sizeof(uint16_t) * params->bufsiz * 2)) == NULL)
    goto fail;

  bladerf_init_devinfo(&dev_info);

  if (params->serial != NULL) {
    strncpy(
        dev_info.serial,
        params->serial,
        sizeof(dev_info.serial) - 1);
    dev_info.serial[sizeof(dev_info.serial) - 1] = '\0';
  }

  /* Open BladeRF */
  status = bladerf_open_with_devinfo(&new->dev, &dev_info);
  if (status == BLADERF_ERR_NODEV) {
    if (params->serial != NULL)
      fprintf(
          stderr,
          "BladeRF error: No bladeRF devices with serial %s\n",
          params->serial);
    else
      fprintf(
          stderr,
          "BladeRF error: No available bladeRF devices found\n");

    goto fail;
  } else if (status != 0) {
    fprintf(
        stderr,
        "BladeRF error: Cannot open device: %s\n",
        bladerf_strerror(status));
    goto fail;
  }

  /* Configure center frequency */
  if (params->fc != 0) {
    status = bladerf_set_frequency(new->dev, BLADERF_MODULE_RX, params->fc);
    if (status != 0) {
      fprintf(
          stderr,
          "BladeRF error: Cannot set frequency: %s\n",
          bladerf_strerror(status));
      goto fail;
    }
  }

  status = bladerf_get_frequency(new->dev, BLADERF_MODULE_RX, &actual_fc);
  if (status != 0) {
    fprintf(
        stderr,
        "BladeRF error: Failed to get frequency: %s\n",
        bladerf_strerror(status));
    goto fail;
  }

  /* Configure sample rate */
  if (params->samp_rate != 0) {
    status = bladerf_set_sample_rate(
        new->dev,
        BLADERF_MODULE_RX,
        params->samp_rate,
        &actual_samp_rate);
    if (status != 0) {
      fprintf(
          stderr,
          "BladeRF error: Cannot enable RX module: %s\n",
          bladerf_strerror(status));
      goto fail;
    }
  } else {
    status = bladerf_get_sample_rate(
        new->dev,
        BLADERF_MODULE_RX,
        &actual_samp_rate);
    if (status != 0) {
      fprintf(
          stderr,
          "BladeRF error: Cannot enable RX module: %s\n",
          bladerf_strerror(status));
      goto fail;
    }
  }

  /* Configure bandwidth according to actual sample rate */
  if ((actual_bw = round(actual_samp_rate * .75)) < BLADERF_BANDWIDTH_MIN)
    actual_bw = BLADERF_BANDWIDTH_MIN;

  status = bladerf_set_bandwidth(
      new->dev,
      BLADERF_MODULE_RX,
      actual_bw,
      &actual_bw);
  if (status != 0) {
    fprintf(
        stderr,
        "BladeRF error: Failed to set bandwidth: %s\n",
        bladerf_strerror(status));
    goto fail;
  }

  /* Enable XB-300, if present */
  status = bladerf_expansion_attach(new->dev, BLADERF_XB_300);
  if (status == 0) {
    /* XB-300 found, enable or disable LNA accordingly */
    amp = BLADERF_XB300_AMP_LNA;

    status = bladerf_xb300_set_amplifier_enable(new->dev, amp, params->lna);
    if (status != 0) {
      fprintf(
          stderr,
          "BladeRF error: Cannot enable XB-300: %s\n",
          bladerf_strerror(status));
      goto fail;
    }
  } else if (params->lna) {
    fprintf(
        stderr,
        "BladeRF warning: Cannot enable LNA: XB-300 found\n");
  }

  /* Configure VGA1, VGA2 and LNA gains */
  status = bladerf_set_rxvga1(new->dev, params->vga1);
  if (status != 0) {
    fprintf(
        stderr,
        "BladeRF error: Failed to set VGA1 gain: %s\n",
        bladerf_strerror(status));
    goto fail;
  }

  status = bladerf_set_rxvga2(new->dev, params->vga2);
  if (status != 0) {
    fprintf(
        stderr,
        "BladeRF error: Failed to set VGA2 gain: %s\n",
        bladerf_strerror(status));
    goto fail;
  }

  status = bladerf_set_lna_gain(new->dev, params->lnagain);
  if (status != 0) {
    fprintf(
        stderr,
        "BladeRF error: Failed to set LNA gain: %s\n",
        bladerf_strerror(status));
    goto fail;
  }

  /* Configure sync RX */
  if (!bladeRF_state_init_sync(new)) {
    fprintf(
        stderr,
        "BladeRF error: Failed to init bladeRF in sync mode\n");
    goto fail;
  }

  /* Enable RX */
  status = bladerf_enable_module(new->dev, BLADERF_MODULE_RX, RTS_TRUE);
  if (status != 0) {
    fprintf(
        stderr,
        "BladeRF error: Cannot enable RX module: %s\n",
        bladerf_strerror(status));
    goto fail;
  }

  new->samp_rate = actual_samp_rate;
  new->fc = actual_fc;

  return new;

fail:
  if (new != NULL)
    bladeRF_state_destroy(new);

  return NULL;
}

/************************ RTS Interface Callbacks ****************************/

RTS_PRIVATE void *
rts_bladeRF_open(const rts_params_t *params, struct rts_signal_source_info *info)
{
  struct bladeRF_state *state = NULL;
  struct bladeRF_params bladerf_params = bladeRF_params_INITIALIZER;
  const char *str;

  if ((str = rts_params_get(params, "fs")) == NULL) {
    fprintf(
        stderr,
        "BladeRF error: Cannot open BladeRF file source: `fs' not set\n");
    goto fail;
  }

  if (sscanf(str, "%u", &info->samp_rate) < 1) {
    fprintf(
        stderr,
        "BladeRF error: Cannot open BladeRF file source: wrong sample rate\n");
    goto fail;
  }

  bladerf_params.samp_rate = info->samp_rate;

  if ((str = rts_params_get(params, "fc")) == NULL) {
    fprintf(
        stderr,
        "BladeRF error: Cannot open BladeRF file source: `fc' not set\n");
    goto fail;
  }

  if (sscanf(str, "%lli", &info->freq) < 1) {
    fprintf(
        stderr,
        "BladeRF error: Cannot open BladeRF file source: wrong central frequency\n");
    return NULL;
  }

  if ((str = rts_params_get(params, "vga1")) != NULL)
    if (sscanf(str, "%i", &bladerf_params.vga1) < 1) {
      fprintf(
          stderr,
          "BladeRF error: Cannot open BladeRF file source: wrong VGA1 gain\n");
      return NULL;
    }

  if ((str = rts_params_get(params, "vga2")) != NULL)
    if (sscanf(str, "%i", &bladerf_params.vga2) < 1) {
      fprintf(
          stderr,
          "BladeRF error: Cannot open BladeRF file source: wrong VGA2 gain\n");
      return NULL;
    }

  if ((str = rts_params_get(params, "serial")) != NULL)
    bladerf_params.serial = str;

  if ((str = rts_params_get(params, "lna")) != NULL)
    bladerf_params.lna = strcasecmp(str, "yes") == 0
        || strcasecmp(str, "true") == 0
        || strcasecmp(str, "1") == 0;

  if ((str = rts_params_get(params, "lna_gain")) != NULL)
    if (sscanf(str, "%i", &bladerf_params.lnagain) < 1) {
      fprintf(
          stderr,
          "BladeRF error: Cannot open BladeRF file source: wrong LNA gain\n");
      return NULL;
    }

  if ((str = rts_params_get(params, "bufsiz")) != NULL)
    if (sscanf(str, "%u", &bladerf_params.bufsiz) < 1) {
      fprintf(
          stderr,
          "BladeRF error: Cannot open BladeRF file source: wrong buffer size\n");
      return NULL;
    }

  if ((state = bladeRF_state_new(&bladerf_params)) == NULL) {
    fprintf(
        stderr,
        "BladeRF error: Create bladeRF state failed\n");
    goto fail;
  }

  return state;

fail:
  if (state != NULL)
    bladeRF_state_destroy(state);

  return NULL;
}

RTS_PRIVATE RTSCOUNT
rts_bladeRF_acquire(void *handle, RTSCOMPLEX *buffer, RTSCOUNT count)
{
  int status;
  int i;
  struct bladeRF_state *state = (struct bladeRF_state *) handle;

  count = MIN(count, state->params.bufsiz);

  status = bladerf_sync_rx(
      state->dev,
      state->buffer,
      count,
      NULL,
      5000);

  if (status != 0) {
    fprintf(
        stderr,
        "BladeRF error: sync read error: %s\n", bladerf_strerror(status));
    return -1;
  }
    /* Read OK. Transform samples */
  for (i = 0; i < count; ++i)
    buffer[i] =
        state->buffer[i << 1] / 2048.0
        + I * state->buffer[(i << 1) + 1] / 2048.0;

  return count;
}

RTS_PRIVATE void
rts_bladeRF_close(void *handle)
{
  bladeRF_state_destroy((struct bladeRF_state *) handle);
}

RTSBOOL
rts_bladeRF_source_register(void)
{
  static struct rts_signal_source src =
  {
      .name = "bladerf",
      .open = rts_bladeRF_open,
      .acquire = rts_bladeRF_acquire,
      .close = rts_bladeRF_close
  };

  RTS_TRYCATCH(rts_signal_source_register(&src), return RTS_FALSE);

  return RTS_TRUE;
}

