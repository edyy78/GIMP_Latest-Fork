/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * image-scanner - Image scannerplug-in for the GIMP that interfaces
 *                 with SANE to scan images into GIMP.
 * Copyright (C) 2025  Draekko
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

static SANE_Status
scan_it_raw (SANE_Handle device, const gchar *fname, SANE_Bool raw,
             const gchar *script, SANE_Bool use_pipe);

void
flatbed_start_scan(const gchar *device);

#define BASE_OPTSTRING	"d:hLvVNTo:s:e:S:pr"
#define STRIP_HEIGHT	256	/* # lines we increment image height */
#define sane_isbasicframe(f) ( (f) == SANE_FRAME_GRAY || \
(f) == SANE_FRAME_RGB || \
(f) == SANE_FRAME_RED || \
(f) == SANE_FRAME_GREEN || \
(f) == SANE_FRAME_BLUE )

static int resolution_opt = -1, x_resolution_opt = -1, y_resolution_opt = -1;
static int verbose;

typedef struct
{
  u_char *data;
  int Bpp;		/* bytes/pixel */
  int width;
  int height;
  int x;
  int y;
}
Image;

const gchar * path = "/tmp/";
const gchar * out = "saneimagescanner";
const gchar * pnm = ".pnm";

gint maxloop = 1;

extern gint res_opt;
extern gint mode_opt;
extern gint source_opt;
extern gint page_left_opt;
extern gint page_top_opt;
extern gint page_right_opt;
extern gint page_bottom_opt;

extern gint resolutions[1024];
extern gint res_count;
extern gchar modes[10][1024];
extern gint modes_count;
extern gchar sources[10][1024];
extern gint source_count;

extern gint resolution_index;
extern gint mode_index;
extern gint source_index;

extern gdouble left_current;
extern gdouble right_current;
extern gdouble top_current;
extern gdouble bottom_current;

extern gint units_measurement;
extern gint input_type;

extern gboolean use_color;
extern gboolean use_flatbed;
