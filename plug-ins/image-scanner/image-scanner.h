/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * image-scanner - Image scanner plug-in for GIMP that uses SANE.
 * Copyright (C) 2024 Draekko
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

#ifndef __IMAGE_SCANNER_H__
#define __IMAGE_SCANNER_H__

typedef struct _ImageScanner      ImageScanner;
typedef struct _ImageScannerClass ImageScannerClass;

struct _ImageScanner
{
  GimpPlugIn parent_instance;
};

struct _ImageScannerClass
{
  GimpPlugInClass parent_class;
};

enum
{
  ME_RENDER_TEXT,
  ME_RENDER_OTHER,
};

enum
{
  //IMAGE_SCANNER_NEW_LAYER,
  IMAGE_SCANNER_CURRENT_LAYER,
  IMAGE_SCANNER_NEW_IMAGE,
};

enum
{
  IMAGE_SCANNER_MM_UNIT,
  IMAGE_SCANNER_CM_UNIT,
  IMAGE_SCANNER_IN_UNIT,
};

typedef struct
{
  gchar    *label;
  gint      renderer_type;
} me_column_info;

enum
{
  LIST_ADDRESS,
  LIST_VENDOR,
  LIST_MODEL,
  LIST_INFO,
};

static const me_column_info scanner_device_info[] =
{
  { N_("Address"),  ME_RENDER_TEXT },
  { N_("Vendor"),   ME_RENDER_TEXT },
  { N_("Model"),    ME_RENDER_TEXT },
  { N_("Info"),    ME_RENDER_TEXT },
};
static const gint n_scanner_device_info = G_N_ELEMENTS (scanner_device_info);

static const me_column_info scanner_unset_info[] =
{
  { N_("*unset*"),  ME_RENDER_TEXT },
};
static const gint n_scanner_unset_info = G_N_ELEMENTS (scanner_unset_info);

typedef struct
{
   const char * name;
   const char * model;
   const char * vendor;
   const char * info;
} device_data;

#define IMAGE_SCANNER_TYPE (imagescanner_get_type ())
#define IMAGE_SCANNER(obj) \
(G_TYPE_CHECK_INSTANCE_CAST ((obj), IMAGE_SCANNER_TYPE, Pnm))

#define DEBUGLOG 1
#define PLUG_IN_BINARY  "image-scanner"
#define PLUG_IN_ROLE    "gimp-image-scanner-dialog"
#define PLUG_IN_PROC    "plug-in-image-scanner"

const gdouble letter_w = 215.9; // Letter size in mm
const gdouble letter_h = 279.4; // Letter size in mm

const gdouble legal_w = 215.9; // Legal size in mm
const gdouble legal_h = 355.6; // Legal size in mm

const gdouble A4_w    = 210.0; // A4 size in mm
const gdouble A4_h    = 297.0; // A4 size in mm

const gdouble B5_w    = 176.0; // B5 size in mm
const gdouble B5_h    = 250.0; // B5 size in mm

const gdouble P4_w    = 215.0; // P4 size in mm
const gdouble P4_h    = 280.0; // P4 size in mm

GimpProcedureConfig *globalConfig;
SANE_Device        **device_list;
GtkListStore         *res_store1;
GtkListStore         *res_store2;
GtkListStore         *res_store3;
GtkListStore         *list_store;
GtkWidget            *list_view;
GtkWidget            *crop_left_scaler;
GtkWidget            *crop_top_scaler;
GtkWidget            *crop_right_scaler;
GtkWidget            *crop_bottom_scaler;
GtkWidget            *rescombo1;
GtkWidget            *rescombo2;
GtkWidget            *rescombo3;
GtkWidget            *progressbar;
GtkWidget            *message;
GtkWidget            *crop_left_scaler;
GtkWidget            *crop_right_scaler;
GtkWidget            *crop_top_scaler;
GtkWidget            *crop_bottom_scaler;

gint     devices = 0;
gchar   *current_device_name;
gint     resolutions[1024];
gint     res_count = 0;
gchar    modes[10][1024];
gint     modes_count = 0;
gchar    sources[10][1024];
gint     source_count = 0;
gint     input_type = IMAGE_SCANNER_CURRENT_LAYER;
gint     resolution_index = 0;
gint     mode_index = 0;
gint     source_index = 0;
gint     units_measurement = IMAGE_SCANNER_IN_UNIT;
gint     last_units_measurement = IMAGE_SCANNER_IN_UNIT;
gint     res_opt = 0;
gint     mode_opt = 0;
gint     source_opt = 0;
gint     page_left_opt = 0;
gint     page_top_opt = 0;
gint     page_right_opt = 0;
gint     page_bottom_opt = 0;
gdouble  page_left = 215.9; // Letter size
gdouble  page_top = 279.4; // Letter size
gdouble  page_right = 215.9; // Letter size
gdouble  page_bottom = 279.4; // Letter size
gdouble  page_bottom_temp = 279.4; // Letter size
gdouble  left_current = 0;
gdouble  top_current = 0;
gdouble  right_current = letter_w;
gdouble  bottom_current = letter_h;
gboolean use_color = TRUE;
gboolean use_flatbed = TRUE;
gboolean use_adf = FALSE;
gboolean init = TRUE;

static void
image_scanner_scan_callback (GtkWidget *dialog);

static GtkWidget *
image_scanner_create_page_grid (GtkWidget   *notebook,
                                const gchar *tab_name);
extern void
flatbed_start_scan(const char *device);

#endif /* __IMAGE_SCANNER_H__ */
