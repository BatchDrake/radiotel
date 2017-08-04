/*
 * main.c: entry point for radiotel
 * Creation date: Mon Jul 17 10:47:46 2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <radiotel.h>

#include <rtsutil/source.h>
#include <rtsutil/spectrogram.h>
#include <sys/time.h>

#define RADTEL_NIGHT_MODE

#define RADTEL_AVG_TIME 60.0
#define RADTEL_BINS     2048

#define RADTEL_SNAPSHOT_DIR "snapshots"
#define RADTEL_SNAPSHOT_TMP_BMP "/tmp/.spectrogram.bmp"

#if RADTEL_FULL_SCREEN
#  define WINDOW_WIDTH  1920
#  define WINDOW_HEIGHT 1080
#else
#  define WINDOW_WIDTH  1400
#  define WINDOW_HEIGHT 800
#endif

#ifdef RADTEL_NIGHT_MODE
#  define SPECTRUM_TEXT_COLOR 0xbf0000
#  define SPECTRUM_AXES_COLOR 0x400000
#  define SPECTRUM_FOREGROUND 0xff0000
#  define SPECTRUM_BACKGROUND 0x000000
#else /* !defined(RADTEL_NIGHT_MODE) */
#  define SPECTRUM_TEXT_COLOR 0xbfbfbf
#  define SPECTRUM_AXES_COLOR 0x404040
#  define SPECTRUM_FOREGROUND 0x00ff00
#  define SPECTRUM_BACKGROUND 0x1f1f1f
#endif /* RADTEL_NIGHT_MODE */

#define SPECTRUM_X 32
#define SPECTRUM_Y 64

#define SPECTRUM_TITLE "* * * RADIOTELESCOPE SPECTRUM INTEGRATOR * * *"

#define SPECTRUM_PROGRESS_X 32
#define SPECTRUM_PROGRESS_Y (WINDOW_HEIGHT - 16)
#define SPECTRUM_PROGRESS_WIDTH (WINDOW_WIDTH - 2 * SPECTRUM_PROGRESS_X)
#define SPECTRUM_PROGRESS_HEIGHT 8

#define SPECTRUM_H_DIVS 20
#define SPECTRUM_V_DIVS 10

#define SPECTRUM_WIDTH  (WINDOW_WIDTH - 2 * SPECTRUM_X)
#define SPECTRUM_HEIGHT (WINDOW_HEIGHT - 3 * SPECTRUM_Y / 2)

#define RTS_TO_POWER_DB(mag) (10 * log10(mag))

char *snapshot_dir;
char *matlab_temp;

void
radtel_redraw_spectrum(display_t *disp, const rts_spectrogram_t *spect)
{
  const RTSFLOAT *spectrum = NULL;
  unsigned int count;
  RTSFLOAT prev_x = 0;
  RTSFLOAT prev_y = 0;
  RTSFLOAT x, y;
  unsigned int i;
  unsigned int p;
  RTSFLOAT min, max;
  RTSFLOAT f_lo, f_hi;
  RTSFLOAT range;
  struct tm tm_buf;
  time_t now;
  char now_str[30];

  /* Get spectrogram properties */
  count    = rts_spectrogram_get_frame_count(spect);
  spectrum = rts_spectrogram_get_cumulative(spect);
  rts_spectrogram_get_range(spect, &min, &max, &f_lo, &f_hi);

  /* Adjust range */
  if (min == 0 && max == 0) {
    min = -60;
    max = 0;
  } else {
    min = RTS_TO_POWER_DB(min / count);
    max = RTS_TO_POWER_DB(max / count);
  }

  range = max - min;

  /* Clear previous spectrum */
  clear(disp, OPAQUE(SPECTRUM_BACKGROUND));

  /* Draw text */
  display_printf(
      disp,
      WINDOW_WIDTH / 2 - strlen(SPECTRUM_TITLE) * 4,
      2,
      OPAQUE(SPECTRUM_TEXT_COLOR),
      OPAQUE(SPECTRUM_BACKGROUND),
      "%s",
      SPECTRUM_TITLE);

  time(&now);
  asctime_r(gmtime_r(&now, &tm_buf), now_str);
  now_str[strlen(now_str) - 1] = '\0';

  display_printf(
      disp,
      SPECTRUM_X + 2,
      20,
      OPAQUE(SPECTRUM_TEXT_COLOR),
      OPAQUE(SPECTRUM_BACKGROUND),
      "Time: %s UTC -- ",
      now_str);

  display_printf(
      disp,
      SPECTRUM_X + 38 * 8,
      20,
      OPAQUE(SPECTRUM_TEXT_COLOR),
      OPAQUE(SPECTRUM_BACKGROUND),
      "dB range: %+5.1lf dB to %+5.1lf dB (%+5.1lf dB) -- ",
      min,
      max,
      range);

  display_printf(
      disp,
      SPECTRUM_X + 83 * 8,
      20,
      OPAQUE(SPECTRUM_TEXT_COLOR),
      OPAQUE(SPECTRUM_BACKGROUND),
      "Frequency range: %lg MHz to %lg MHz (%lg MHz)",
      (RTSFLOAT) (f_lo * 1e-6),
      (RTSFLOAT) (f_hi * 1e-6),
      (RTSFLOAT) ((f_hi - f_lo) * (RTSFLOAT) 1e-6));

  display_printf(
      disp,
      SPECTRUM_X + 2,
      30,
      OPAQUE(SPECTRUM_TEXT_COLOR),
      OPAQUE(SPECTRUM_BACKGROUND),
      "Spectrum snapshot count: %d (integration window: %lg s)",
      rts_spectrogram_get_reset_count(spect),
      rts_spectrogram_get_got_samples(spect)
      / (RTSFLOAT) rts_spectrogram_get_samp_rate(spect));

  for (i = 0; i < SPECTRUM_H_DIVS; ++i) {
    x = (RTSFLOAT) i / (RTSFLOAT) SPECTRUM_H_DIVS * SPECTRUM_WIDTH + SPECTRUM_X;
    line(disp, x, SPECTRUM_Y, x, SPECTRUM_Y + SPECTRUM_HEIGHT, OPAQUE(SPECTRUM_AXES_COLOR));
  }

  for (i = 0; i < SPECTRUM_V_DIVS; ++i) {
    y = (RTSFLOAT) i / (RTSFLOAT) SPECTRUM_V_DIVS * SPECTRUM_HEIGHT + SPECTRUM_Y;
    line(disp, SPECTRUM_X, y, SPECTRUM_X + SPECTRUM_WIDTH, y, OPAQUE(SPECTRUM_AXES_COLOR));
  }

  for (i = 0; i < RADTEL_BINS; ++i) {
    p = (i + RADTEL_BINS / 2) % RADTEL_BINS;

    x = (RTSFLOAT) i / (RTSFLOAT) RADTEL_BINS * SPECTRUM_WIDTH + SPECTRUM_X;
    y = (RTS_TO_POWER_DB(spectrum[p] / count) - min) / range;

    /* Limit spectrum values if they fall out of range */
    if (y < 0)
      y = 0;
    else if (y > 1)
      y = 1;

    y = (1 - y) * SPECTRUM_HEIGHT + SPECTRUM_Y;

    if (i > 0)
      line(disp, prev_x, prev_y, x, y, OPAQUE(SPECTRUM_FOREGROUND));

    prev_x = x;
    prev_y = y;
  }

  box(
      disp,
      SPECTRUM_X,
      SPECTRUM_Y,
      SPECTRUM_X + SPECTRUM_WIDTH - 1,
      SPECTRUM_Y + SPECTRUM_HEIGHT - 1,
      OPAQUE(SPECTRUM_TEXT_COLOR));

  fbox(
      disp,
      SPECTRUM_PROGRESS_X,
      SPECTRUM_PROGRESS_Y,
      SPECTRUM_PROGRESS_X
      + (SPECTRUM_PROGRESS_WIDTH - 1)
      * rts_spectrogram_get_progress(spect),
      SPECTRUM_PROGRESS_Y + SPECTRUM_PROGRESS_HEIGHT - 1,
      OPAQUE(SPECTRUM_FOREGROUND));

  box(
      disp,
      SPECTRUM_PROGRESS_X,
      SPECTRUM_PROGRESS_Y,
      SPECTRUM_PROGRESS_X + SPECTRUM_PROGRESS_WIDTH - 1,
      SPECTRUM_PROGRESS_Y + SPECTRUM_PROGRESS_HEIGHT - 1,
      OPAQUE(SPECTRUM_TEXT_COLOR));

  display_refresh(disp);
}

RTSBOOL
radtel_dump_screenshot(display_t *disp)
{
  char *path = NULL;
  char *command = NULL;
  static int times = 0;
  RTSBOOL ok = RTS_FALSE;

  RTS_TRYCATCH(
      path = strbuild("%s/spectrogram-%05d.png", snapshot_dir, times++),
      goto done);

  RTS_TRYCATCH(
      command = strbuild("convert %s %s", RADTEL_SNAPSHOT_TMP_BMP, path),
      goto done);

  RTS_TRYCATCH(display_dump(RADTEL_SNAPSHOT_TMP_BMP, disp) == 0, goto done);

  RTS_TRYCATCH(system(command) == 0, goto done);

  ok = RTS_TRUE;

done:
  if (path != NULL)
    free(path);

  if (command != NULL)
    free(command);

  return ok;
}

RTSBOOL
radtel_start_rx(rts_srchnd_t *handle)
{
  rts_spectrogram_t *spect = NULL;
  struct rts_spectrogram_params params;
  display_t *disp = NULL;
  struct timeval tv, otv;
  struct timeval sub;
  RTSBOOL ok = RTS_FALSE;

  params.avg_time = RADTEL_AVG_TIME;
  params.bins     = RADTEL_BINS;

  RTS_TRYCATCH(spect = rts_spectrogram_new(handle, &params), goto done);

  RTS_TRYCATCH(disp = display_new(WINDOW_WIDTH, WINDOW_HEIGHT), goto done);

  for (;;) {
    gettimeofday(&otv, NULL);

    while (!rts_spectrogram_complete(spect)) {
      if (!rts_spectrogram_acquire(spect)) {
        fprintf(stderr, "RX: finished\n");
        goto done;
      }

      gettimeofday(&tv, NULL);

      timersub(&tv, &otv, &sub);

      if (sub.tv_sec >= 1 && rts_spectrogram_get_frame_count(spect) > 0) {
        radtel_redraw_spectrum(disp, spect);
        otv = tv;
      }
    }

    /* TODO: Dump spectrogram */
    radtel_redraw_spectrum(disp, spect);

    if (!rts_spectrogram_dump_matlab(spect, matlab_temp))
      fprintf(stderr, "Warning: failed to save spectrum in Matlab format\n");

    if (!radtel_dump_screenshot(disp))
      fprintf(stderr, "Warning: failed to dump screenshot\n");

    rts_spectrogram_reset(spect);
  }

  ok = RTS_TRUE;

done:
  if (disp != NULL)
    display_end(disp);

  if (spect != NULL)
    rts_spectrogram_destroy(spect);

  return ok;
}

RTSBOOL
rtadtel_init_snapshot_dir(void)
{
  char *path = NULL;
  RTSBOOL found = RTS_FALSE;
  unsigned int i = 0;
  RTSBOOL ok = RTS_FALSE;

  if (access(RADTEL_SNAPSHOT_DIR, F_OK) == -1)
    RTS_TRYCATCH(mkdir(RADTEL_SNAPSHOT_DIR, 0755) != -1, goto done);

  do {
    RTS_TRYCATCH(
        path = strbuild("%s/capture-%03d", RADTEL_SNAPSHOT_DIR, i++),
        goto done);

    if (access(path, F_OK) == -1) {
      RTS_TRYCATCH(matlab_temp = strbuild("%s/data", path), goto done);
      RTS_TRYCATCH(mkdir(path, 0755) != -1, goto done);

      snapshot_dir = path;
      path = NULL;
      found = RTS_TRUE;
    } else {
      free(path);
      path = NULL;
    }
  } while (!found);

  ok = RTS_TRUE;

done:
  if (path != NULL)
    free(path);

  return ok;
}

int
main(int argc, char *argv[], char *envp[])
{
  int ret_code = EXIT_FAILURE;
  rts_srchnd_t *handle = NULL;
  const struct rts_signal_source *source = NULL;
  rts_params_t *params = NULL;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s source-type parameters\n", argv[0]);
    goto done;
  }

  if (!rts_register_builtin_sources()) {
    fprintf(stderr, "%s: failed to initialize builting sources\n", argv[0]);
    goto done;
  }

  if ((source = rts_signal_source_lookup(argv[1])) == NULL) {
    fprintf(stderr, "%s: unsupported source type `%s'\n", argv[0], argv[1]);
    goto done;
  }

  if ((params = rts_params_new()) == NULL) {
    fprintf(stderr, "%s: failed to create params\n", argv[0]);
    goto done;
  }

  if (!rts_params_parse(params, argv[2])) {
    fprintf(stderr, "%s: failed to parse source parameters\n", argv[0]);
    goto done;
  }

  if ((handle = rts_source_open(source, params)) == NULL) {
    fprintf(stderr, "%s: failed to open source\n", argv[0]);
    goto done;
  }

  if (!rtadtel_init_snapshot_dir()) {
    fprintf(stderr, "%s: failed to init capture directory\n", argv[0]);
    goto done;
  }

  (void) radtel_start_rx(handle);

  ret_code = EXIT_SUCCESS;

done:
  if (handle != NULL)
    rts_source_close(handle);

  if (params != NULL)
    rts_params_destroy(params);

  exit (ret_code);
}


