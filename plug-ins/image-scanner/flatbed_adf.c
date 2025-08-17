/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * image-scanner - Image scannerplug-in for the GIMP that interfaces
 *                 with SANE to scan images into GIMP.
 * Copyright (C) 2025  Draekko
 *
 * portions of the scan code taken from scanadf from SANE frontend
 * code and portions of that code are based on
 * bnhscan by tummy.com and
 * scanimage by Andreas Beck and David Mosberger
 * Copyright (C) 1999 Tom Martone
 * Copyright (C) 2005 Rene Rebe ([ -p | --pipe] script option)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

  // https://www.gnu.org/licenses/license-list.en.html

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <locale.h>
#include <sane/sane.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "libgimp/stdplugins-intl.h"

#include "flatbed_adf.h"

static void
write_pnm_header_to_file (FILE *fp, SANE_Frame format, int width, int height, int depth)
{
  switch (format)
    {
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    case SANE_FRAME_RGB:
      fprintf (fp, "P6\n# SANE data follows\n%d %d\n255\n", width, height);
      break;

    case SANE_FRAME_GRAY:
      if (depth == 1)
        fprintf (fp, "P4\n# SANE data follows\n%d %d\n", width, height);
      else
        fprintf (fp, "P5\n# SANE data follows\n%d %d\n255\n", width, height);
      break;
    default:
      /* default action for unknown frametypes; don't write a header */
      break;
    }
}

static void *
advance (Image *image)
{
  if (++image->x >= image->width)
    {
      image->x = 0;
      if (++image->y >= image->height || !image->data)
        {
          size_t old_size = 0, new_size;

          if (image->data)
            old_size = image->height * image->width * image->Bpp;

          image->height += STRIP_HEIGHT;
          new_size = image->height * image->width * image->Bpp;

          if (image->data)
            image->data = realloc (image->data, new_size);
          else
            image->data = malloc (new_size);
          if (image->data)
            memset (image->data + old_size, 0, new_size - old_size);
        }
    }
  if (!image->data)
    fprintf (stderr, _("can't allocate image buffer (%dx%d)\n"),
             image->width, image->height);
  return image->data;
}


const gchar*
sane_strframe (SANE_Frame f)
{
  switch (f)
    {
    case SANE_FRAME_GRAY:
      return "gray";

    case SANE_FRAME_RGB:
      return "RBG";

    case SANE_FRAME_RED:
      return "red";

    case SANE_FRAME_GREEN:
      return "green";

    case SANE_FRAME_BLUE:
      return "blue";

#if 0	// Not currently support by SANE

    case SANE_FRAME_TEXT:
      return "text";

    case SANE_FRAME_JPEG:
      return "jpeg";

    case SANE_FRAME_G31D:
      return "g31d";

    case SANE_FRAME_G32D:
      return "g32d";

    case SANE_FRAME_G42D:
      return "g42d";
#endif

    default:
      return "unknown";
    }
}

static SANE_Int
get_resolution (SANE_Device *device)
{
  const SANE_Option_Descriptor *opt;
  SANE_Int                      res = 200;
  SANE_Word                     val;

  if (resolution_opt >= 0)
    {
      opt = sane_get_option_descriptor (device, resolution_opt);

      sane_control_option (device, resolution_opt,
                           SANE_ACTION_GET_VALUE, &val, 0);
      switch (opt->type)
        {
        case SANE_TYPE_INT:
          res = val;
          break;

        case SANE_TYPE_FIXED:
          res = (SANE_Int) SANE_UNFIX(val);
          break;

        case SANE_TYPE_STRING:
        case SANE_TYPE_BOOL:
        default:
          if (verbose)
            fprintf (stderr,
                    _("Peculiar option data type for resolution, "
                    "using default value.\n"));
          break;
        }
    }
  else
    {
      if (verbose)
        fprintf (stderr, _("No resolution option found, using default value.\n"));
    }

  return res;
}

static int
exec_script (SANE_Handle device, const gchar *script, const gchar* fname,
             SANE_Bool use_pipe, SANE_Parameters *parm, int *fd)
{
  static gchar   cmd[PATH_MAX * 2];
  static gchar   env[7][PATH_MAX * 2];
  gint           pipefd[2];
  gint           pid;
  SANE_Int       res;
  SANE_Frame     format;
  extern gchar **environ;

  res = get_resolution (device);

  format = parm->format;
  if (format == SANE_FRAME_RED ||
      format == SANE_FRAME_GREEN ||
      format == SANE_FRAME_BLUE)
    {
      /* the resultant format is RGB */
      format = SANE_FRAME_RGB;
    }

  snprintf (env[0], sizeof (env[0]), "SCAN_RES=%d", res);
  if (putenv (env[0]))
    fprintf (stderr, _("putenv:failed\n"));

  snprintf (env[1], sizeof (env[1]), "SCAN_WIDTH=%d", parm->pixels_per_line);
  if (putenv (env[1]))
    fprintf (stderr, _("putenv:failed\n"));

  snprintf (env[2], sizeof (env[2]), "SCAN_HEIGHT=%d", parm->lines);
  if (putenv (env[2]))
    fprintf (stderr, _("putenv:failed\n"));

  snprintf (env[3], sizeof (env[3]), "SCAN_FORMAT_ID=%d", (int) parm->format);
  if (putenv (env[3]))
    fprintf (stderr, _("putenv:failed\n"));

  snprintf (env[4], sizeof (env[4]), "SCAN_FORMAT=%s",
            sane_strframe (parm->format));
  if (putenv (env[4]))
    fprintf (stderr, _("putenv:failed\n"));

  snprintf (env[5], sizeof (env[5]), "SCAN_DEPTH=%d", parm->depth);
  if (putenv (env[5]))
    fprintf (stderr, _("putenv:failed\n"));

  snprintf (env[6], sizeof (env[6]), "SCAN_PIPE=%d", use_pipe);
  if (putenv (env[6]))
    fprintf (stderr, _("putenv:failed\n"));

  if (use_pipe) {
    if (pipe(pipefd) != 0)
       use_pipe = 0;
  }

  /*signal(SIGCHLD, SIG_IGN);*/
  switch ((pid = fork ()))
    {
    case -1:
      /*  fork failed  */
      fprintf (stderr, _("Error forking: %s (%d)\n"), strerror (errno), errno);
      break;

    case 0:
      /*  in child process  */
      if (use_pipe)
        {
          dup2 (pipefd[0], 0);
          close (pipefd[0]);
          close (pipefd[1]);
        }
      sprintf (cmd, "%s '%s'", script, fname);
      execle (script, script, fname, NULL, environ);
      exit (0);

    default:
      if (verbose)
        fprintf (stderr, _("Started script `%s' as pid=%d\n"), script, pid);
      break;
    }

  if (use_pipe) {
    close (pipefd[0]);
    *fd = pipefd[1];
  }

  return pid;
}

static SANE_Status
scan_it_raw (SANE_Handle device, const gchar *fname, SANE_Bool raw,
             const gchar *script, SANE_Bool use_pipe)
{
  gint            i;
  gint            len;
  gint            first_frame = 1;
  gint            offset = 0;
  gint            must_buffer = 0;
  SANE_Byte       buffer[32*1024];
  SANE_Byte       min = 0xff;
  SANE_Byte       max = 0;
  SANE_Parameters parm;
  SANE_Status     status;
  Image           image = {0, };
  FILE           *fp = NULL;
  gint            pid = 0;

  do
    {
#ifdef SANE_STATUS_WARMING_UP
      do
        {
          status = sane_start (device);
	} while (status == SANE_STATUS_WARMING_UP);
#else
      status = sane_start (device);
#endif
      if (status != SANE_STATUS_GOOD)
	{
          if (status == SANE_STATUS_INVAL)
            {
              fprintf (stderr, _("sane_start: The scan cannot be started with the current set of options\n"));
            }

	  if (status != SANE_STATUS_NO_DOCS)
	    {
	      fprintf (stderr, _("sane_start: %s\n"),
		       sane_strstatus (status));
	    }
	  goto cleanup;
	}

      status = sane_get_parameters (device, &parm);
      if (status != SANE_STATUS_GOOD)
	{
	  fprintf (stderr, _("sane_get_parameters: %s\n"),
		   sane_strstatus (status));
	  goto cleanup;
	}

      if (script && use_pipe)
      {
	int fd = 0;
	pid = exec_script(device, script, fname, use_pipe, &parm, &fd);
	fp = fdopen (fd, "wb");
      }
      else
        fp = fopen (fname, "wb");

      if (!fp) {
	fprintf (stderr, _("Error opening output `%s': %s (%d)\n"),
		fname, strerror(errno), errno);
	status = SANE_STATUS_IO_ERROR;
	goto cleanup;
      }

      if (verbose)
	{
	  if (first_frame)
	    {
	      if (sane_isbasicframe(parm.format))
		{
		  if (parm.lines >= 0)
		    fprintf (stderr, _("scanning image of size %dx%d pixels"
			     " at %d bits/pixel\n"),
			     parm.pixels_per_line, parm.lines,
			     8 * parm.bytes_per_line / parm.pixels_per_line);
		  else
		    fprintf (stderr, _("scanning image %d pixels wide and "
			     "variable height at %d bits/pixel\n"),
			     parm.pixels_per_line,
			     8 * parm.bytes_per_line / parm.pixels_per_line);
		}
	      else
		{
		    fprintf (stderr, _("receiving %s frame "
			     "bytes/line=%d, "
			     "pixels/line=%d, "
			     "lines=%d, "
			     "depth=%d\n"),
			     sane_strframe(parm.format),
			     parm.bytes_per_line,
			     parm.pixels_per_line,
			     parm.lines,
			     parm.depth);
		}
	    }
	  fprintf (stderr, _("acquiring %s frame\n"),
		   sane_strframe(parm.format));
	}

      if (first_frame)
	{
	  switch (parm.format)
	    {
	    case SANE_FRAME_RED:
	    case SANE_FRAME_GREEN:
	    case SANE_FRAME_BLUE:
	      assert (parm.depth == 8);
	      must_buffer = 1;
	      offset = parm.format - SANE_FRAME_RED;
	      break;

	    case SANE_FRAME_RGB:
	      assert (parm.depth == 8);
	    case SANE_FRAME_GRAY:
	      assert (parm.depth == 1 || parm.depth == 8);
	      /* if we're writing raw, we skip the header and never
	       * have to buffer a single frame format.
	       */
	      if (raw == SANE_FALSE)
		{
		  if (parm.lines < 0)
		    {
		      must_buffer = 1;
		      offset = 0;
		    }
		  else
		    {
		      write_pnm_header_to_file (fp, parm.format,
						parm.pixels_per_line,
						parm.lines, parm.depth);
		    }
		}
	      break;

	    default:
	      /* Default action for unknown frametypes; write them out
	       * without a header; issue a warning in verbose mode.
	       * Since we're not writing a header, there's no need to
	       * buffer.
	       */
	      if (verbose)
		{
		  fprintf (stderr, _("unknown frame format %d\n"),
			  (int) parm.format);
		}
	      if (!parm.last_frame)
		{
		  status = SANE_STATUS_INVAL;
		  fprintf (stderr, _("bad %s frame: must be last_frame\n"),
			   sane_strframe (parm.format));
		  goto cleanup;
		}
	      break;
	    }

	  if (must_buffer)
	    {
	      /* We're either scanning a multi-frame image or the
                 scanner doesn't know what the eventual image height
                 will be (common for hand-held scanners).  In either
                 case, we need to buffer all data before we can write
                 the image.  */
	      image.width  = parm.pixels_per_line;
	      if (parm.lines >= 0)
		/* See advance(); we allocate one extra line so we
		   don't end up realloc'ing in when the image has been
		   filled in.  */
		image.height = parm.lines - STRIP_HEIGHT + 1;
	      else
		image.height = 0;
	      image.Bpp = 3;
	      if (parm.format == SANE_FRAME_GRAY ||
		  !sane_isbasicframe(parm.format))
		image.Bpp = 1;
	      image.x = image.width - 1;
	      image.y = -1;
	      if (!advance (&image))
		goto cleanup;
	    }
	}
      else
	{
	  assert (parm.format >= SANE_FRAME_RED
		  && parm.format <= SANE_FRAME_BLUE);
	  offset = parm.format - SANE_FRAME_RED;
	  image.x = image.y = 0;
	}

      while (1)
	{
	  status = sane_read (device, buffer, sizeof (buffer), &len);
	  if (status != SANE_STATUS_GOOD)
	    {
	      if (verbose && parm.depth == 8)
		fprintf (stderr, _("min/max graylevel value = %d/%d\n"),
			 min, max);
	      if (status != SANE_STATUS_EOF)
		{
		  fprintf (stderr, _("sane_read: %s\n"),
			   sane_strstatus (status));
		  goto cleanup;
		}
	      break;
	    }
	  if (must_buffer)
	    {
	      switch (parm.format)
		{
		case SANE_FRAME_RED:
		case SANE_FRAME_GREEN:
		case SANE_FRAME_BLUE:
		  for (i = 0; i < len; ++i)
		    {
		      image.data[offset + 3*i] = buffer[i];
		      if (!advance (&image))
			goto cleanup;
		    }
		  offset += 3*len;
		  break;

		case SANE_FRAME_RGB:
		  for (i = 0; i < len; ++i)
		    {
		      image.data[offset + i] = buffer[i];
		      if ((offset + i) % 3 == 0 && !advance (&image))
			goto cleanup;
		    }
		  offset += len;
		  break;

		case SANE_FRAME_GRAY:
		  for (i = 0; i < len; ++i)
		    {
		      image.data[offset + i] = buffer[i];
		      if (!advance (&image))
			goto cleanup;
		    }
		  offset += len;
		  break;
		default:
		  /* optional frametypes are never buffered */
		  fprintf (stderr,
		          _("ERROR: trying to buffer %s"
			  " frametype\n"),
			  sane_strframe(parm.format));
		  break;
		}
	    }
	  else
	    fwrite (buffer, 1, len, fp);

	  if (verbose && parm.depth == 8)
	    {
	      for (i = 0; i < len; ++i)
		if (buffer[i] >= max)
		  max = buffer[i];
		else if (buffer[i] < min)
		  min = buffer[i];
	    }
	}
      first_frame = 0;
    }
  while (!parm.last_frame);

  if (must_buffer)
    {
      image.height = image.y;
      if (raw == SANE_FALSE)
	{
	  /* if we're writing raw, we skip the header */
	  write_pnm_header_to_file (fp, parm.format, image.width,
				    image.height, parm.depth);
	}
      fwrite (image.data, image.Bpp, image.height * image.width, fp);
    }

  if (fp)
    {
      fclose (fp);
      fp = NULL;
    }

  if (script && !use_pipe)
    pid = exec_script (device, script, fname, use_pipe, &parm, NULL);

  if (script) {
    int exit_status = 0;
    waitpid (pid, &exit_status, 0);
    if (exit_status && verbose)
      fprintf (stderr, _("WARNING: child exited with %d\n"), exit_status);
  }

cleanup:
  if (image.data)
    free (image.data);
  if (fp) fclose(fp);

  return status;
}

static SANE_Int
scan_docs (SANE_Handle device, int start, int end, int no_overwrite, SANE_Bool raw,
           const gchar *outfmt, const gchar *script, SANE_Bool use_pipe)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int scannedPages = 0;
  SANE_Char fname[PATH_MAX];
  struct stat statbuf;
  int res;

  while (end < 0 || start <= end)
    {
      sprintf (fname, "%s%s%d%s", path, out, scannedPages, pnm);

      /* does the filename already exist? */
      if (no_overwrite)
        {
          res = stat (fname, &statbuf);
          if (res == 0)
            {
              status = SANE_STATUS_INVAL;
              fprintf (stderr, _("Filename %s already exists; will not overwrite\n"), fname);
            }
        }

      /* Scan the document */
      if (status == SANE_STATUS_GOOD)
        status = scan_it_raw(device, fname, raw, script, use_pipe);

      /* Any scan errors? */
      if (status == SANE_STATUS_NO_DOCS)
        {
          /* out of paper in the hopper; this is our normal exit */
          status = SANE_STATUS_GOOD;
          break;
        }
      else if (status == SANE_STATUS_EOF)
        {
          /* done with this doc */
          status = SANE_STATUS_GOOD;
          fprintf (stderr, _("Scanned document %d\n"), scannedPages);
          scannedPages++;
          start++;
        }
      else
        {
          /* unexpected error */
          fprintf (stderr, _("unexpected error: %s\n"), sane_strstatus(status));
          break;
        }
    }

  maxloop = scannedPages;

  return status;
}


void
flatbed_start_scan(const gchar *device_name)
{
  const SANE_Option_Descriptor *opt;
  SANE_Status                   status;
  SANE_Handle                   devhandle;
  SANE_Int                      info;
  gint                          maxdocs;

  sane_init (0, 0);

  status = sane_open(device_name, &devhandle);
  if (status != SANE_STATUS_GOOD) {
      g_print (_("Cannot find a scanner device, make sure it is turned on and "
                 "connected to the computer.\n"));
      gimp_message (_("Cannot find a scanner device, make sure it is turned on "
                      "and connected to the computer.\n"));
      sane_exit();
      return;
  }

  /* resolution */
  opt = sane_get_option_descriptor (devhandle, res_opt);
  status = sane_control_option (devhandle, res_opt, SANE_ACTION_SET_VALUE,
                               &resolutions[resolution_index], &info);
  if (status != SANE_STATUS_GOOD)
  {
      g_print(_("Cannot set resolution\n"));
      gimp_message(_("Cannot set resolution\n"));
  }

  /* mode */
  opt = sane_get_option_descriptor (devhandle, mode_opt);
  status = sane_control_option (devhandle, mode_opt, SANE_ACTION_SET_VALUE,
                               &modes[mode_index], &info);
  if (status != SANE_STATUS_GOOD)
    {
      g_print (_("Cannot set mode\n"));
      gimp_message (_("Cannot set mode\n"));
    }

  /* source */
  opt = sane_get_option_descriptor (devhandle, source_opt);
  status = sane_control_option (devhandle, source_opt, SANE_ACTION_SET_VALUE,
                               &sources[source_index], &info);
  if (status != SANE_STATUS_GOOD)
    {
      g_print (_("Cannot set source\n"));
      gimp_message (_("Cannot set source\n"));
    }

  /* set page crops */
  gdouble mult = 1.0;
  if (units_measurement == 1 /* IMAGE_SCANNER_CM_UNIT */)
    {
      mult = 10.0;
    }
  if (units_measurement == 2 /* IMAGE_SCANNER_IN_UNIT */)
    {
      mult = 25.4;
    }
  gdouble value = left_current * mult;
  SANE_Word fixed = SANE_FIX(value);
  opt = sane_get_option_descriptor (devhandle, page_left_opt);
  status = sane_control_option (devhandle, page_left_opt, SANE_ACTION_SET_VALUE,
                               &fixed, &info);
  if (status != SANE_STATUS_GOOD)
    {
      g_print (_("Cannot set page crop left\n"));
      gimp_message (_("Cannot set page crop left\n"));
    }

  value = right_current * mult;
  fixed = SANE_FIX(value);
  opt = sane_get_option_descriptor (devhandle, page_right_opt);
  status = sane_control_option (devhandle, page_right_opt, SANE_ACTION_SET_VALUE,
                               &fixed, &info);
  if (status != SANE_STATUS_GOOD)
    {
      g_print (_("Cannot set page crop right\n"));
      gimp_message (_("Cannot set page crop right\n"));
    }

  value = top_current * mult;
  fixed = SANE_FIX(value);
  opt = sane_get_option_descriptor (devhandle, page_top_opt);
  status = sane_control_option (devhandle, page_top_opt, SANE_ACTION_SET_VALUE,
                               &fixed, &info);
  if (status != SANE_STATUS_GOOD)
    {
      g_print (_("Cannot set page crop top\n"));
      gimp_message (_("Cannot set page crop top\n"));
    }

  value = bottom_current * mult;
  fixed = SANE_FIX(value);
  opt = sane_get_option_descriptor (devhandle, page_bottom_opt);
  status = sane_control_option (devhandle, page_bottom_opt, SANE_ACTION_SET_VALUE,
                               &fixed, &info);
  if (status != SANE_STATUS_GOOD)
    {
      g_print (_("Cannot set page crop bottom\n"));
      gimp_message (_("Cannot set page crop bottom\n"));
    }

  maxdocs = 1;
  if (!use_flatbed)
    {
      maxdocs = 1000;
    }
  status = scan_docs (devhandle, 1, maxdocs, FALSE, FALSE, "pnm", NULL, FALSE);
  if (status != SANE_STATUS_GOOD)
    {
      const gchar output[PATH_MAX];
      GFile      *tmpfile;
      tmpfile = g_file_new_for_path (output);
      g_file_delete (tmpfile, NULL, NULL);
      g_object_unref (tmpfile);
      g_print (_("ERROR: FAILED TO SCAN\n"));
      gimp_message (_("ERROR: FAILED TO SCAN\n"));
    }
  else
    {
      GimpImage *scanimage;
      GimpLayer *layer;
      g_print ("use adf maxloop = %d\n", maxloop);
      for (int loop = 0; loop < maxloop; loop++)
        {
          const gchar output[PATH_MAX];
          const gchar page[256];
          sprintf (output, "%s%s%d%s", path, out, loop, pnm);
          GFile *tmpfile;
          tmpfile = g_file_new_for_path (output);

          sprintf (page, "Page %d", loop + 1);

          if (input_type == 1 /* IMAGE_SCANNER_NEW_IMAGE */ || loop == 0)
            {
              scanimage = gimp_file_load (GIMP_RUN_NONINTERACTIVE, tmpfile);
              gimp_display_new (scanimage);
              GimpLayer **layers = gimp_image_get_layers (scanimage);
              gimp_item_set_name (GIMP_ITEM (layers[0]), page);
            }
          else
            {
              layer = gimp_file_load_layer (GIMP_RUN_NONINTERACTIVE, scanimage, tmpfile);
              gimp_item_set_name (GIMP_ITEM (layer), page);
              gimp_image_insert_layer (scanimage, layer, NULL, 0);
            }

          g_file_delete (tmpfile, NULL, NULL);
          g_object_unref (tmpfile);
        }
    }

  sane_cancel (devhandle);

  sane_close (devhandle);

  sane_exit ();
}

