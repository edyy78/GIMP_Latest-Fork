/* bmpwrite.c   Writes Bitmap files. Even RLE encoded ones.      */
/*              (Windows (TM) doesn't read all of those, but who */
/*              cares? ;-)                                       */
/*              I changed a few things over the time, so perhaps */
/*              it dos now, but now there's no Windows left on   */
/*              my computer...                                   */

/* Alexander.Schulz@stud.uni-karlsruhe.de                        */

/*
 * GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 */

#include "config.h"

#include <errno.h>
#include <string.h>

#include <glib/gstdio.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "bmp.h"
#include "bmp-export.h"

#include "libgimp/stdplugins-intl.h"


/* Compatibilty settings:
 * ======================
 *
 * These settings control how the file headers are written, they do not limit
 * the availability of any features.
 */

/*
 * comp_current_official_headers_only
 * ----------------------------------
 *
 * Only allow BITMAPINFOHEADER, BITMAPV4HEADER, BITMAPV5HEADER, do not write
 * adobe v2/v3 headers (or even BITMAPCOREHEADER, which we never write,
 * anyway).
 *
 * (GIMP < 3.0: FALSE)
 */
static const gboolean comp_current_official_headers_only = TRUE;


/*
 * comp_16_and_32_only_as_bitfields
 * --------------------------------
 *
 * The original Windows 3 BMP had 24bit as the only non-indexed format.
 * Windows 95 and NT 4.0 introduced 16 and 32 bit, but apparently only as
 * BI_BITFIELDS, not as BI_RGB. (Encyclopedia of Grahics File Formats, 2nd
 * ed.)
 *
 * Currently (at least since Windows 98 / NT 5.0), 16 and 32 bit each have a
 * standard BI_RGB representation (5-5-5 and 8-8-8).
 *
 * There might be old software which cannot read 16/32-bit BI_RGB, but there
 * might also be newer (simple) software which cannot read BI_BITFIELDS at
 * all. There is no certain most compatible setting. Setting to TRUE gives
 * the edge to older but more 'serious' programs.
 *
 * (GIMP < 3.0: TRUE)
 */
static const gboolean comp_16_and_32_only_as_bitfields = TRUE;


/*
 * comp_min_header_for_bitfields
 * -----------------------------
 *
 * Minimum header version when masks (BI_BITFIELDS) are used. BMPINFO_V1 is
 * acceptable and is probably the most compatible option. (but see next
 * section)
 *
 * (GIMP < 3.0: BMPINFO_V3_ADOBE)
 */
static const enum BmpInfoVer comp_min_header_for_bitfields = BMPINFO_V1;


/*
 * comp_min_header_for_non_standard_masks
 * --------------------------------------
 *
 * It gets better. When BI_BITFIELDS was introduced for the v1 header, you
 * couldn't just use any old bitmask. For 16-bit, only 5-5-5 and 5-6-5 were
 * allowed, for 32-bit, only 8-8-8 was allowed. Current MS documentation for
 * the v1 BITMAPINFOHEADER doesn't mention any limitation on 32-bit masks.
 *
 * I doubt that writing a V4 header for non-standard bitmasks will help with
 * compatibility, if anything it'll probably make it worse.
 *
 * But in case we'll give some compatitbility indication in the export dialog,
 * we might want to remember this tidbit.
 *
 * (GIMP < 3.0: n.a., as currently the only non-standard masks are those with
 * alpha-channel, which require a higher header, anyway)
 */
static const gboolean comp_min_header_for_non_standard_masks = BMPINFO_V1;


/*
 * comp_min_header_for_colorspace
 * ------------------------------
 *
 * Minimum header version when color space is written. Should be BMPINFO_V4.
 * V5 is only needed when actually writing the icc profile to the file. We
 * are currently just flagging as sRGB.
 *
 * (GIMP < 3.0: BMPINFO_V5)
 */
static const enum BmpInfoVer comp_min_header_for_colorspace = BMPINFO_V4;




struct Fileinfo
{
  /* image properties */
  gint          width;
  gint          height;
  gint          channels;
  gint          bytesperchannel;
  gint          alpha;
  gint          ncolors;
  /* file properties */
  BitmapChannel cmasks[4];
  gint          bpp;
  gboolean      use_rle;
  gsize         bytes_per_row;
  FILE         *file;
  /* RLE state */
  guint64       length;
  guchar       *row;
  guchar       *chains;
};

static gboolean write_image                  (struct Fileinfo *fi,
                                              GimpDrawable    *drawable,
                                              const Babl      *format,
                                              gint             frontmatter_size);

static gboolean save_dialog                  (GimpProcedure   *procedure,
                                              GObject         *config,
                                              GimpImage       *image,
                                              gboolean         indexed,
                                              gboolean         allow_alpha,
                                              gboolean         allow_rle);

static void     calc_masks_from_bits         (BitmapChannel   *cmasks,
                                              gint             r,
                                              gint             g,
                                              gint             b,
                                              gint             a);

static gint     calc_bitsperpixel_from_masks (BitmapChannel   *cmasks);

static gboolean are_masks_well_known         (BitmapChannel   *cmasks,
                                              gint             bpp);

static gboolean are_masks_v1_standard        (BitmapChannel   *cmasks,
                                              gint             bpp);

static void     set_info_resolution          (BitmapHead      *bih,
                                              GimpImage       *image);

static gint     info_header_size             (enum BmpInfoVer  version);

static gboolean write_rgb                    (const guchar    *src,
                                              struct Fileinfo *fi);

static gboolean write_u16_le                 (FILE            *file,
                                              guint16          u16);

static gboolean write_u32_le                 (FILE            *file,
                                              guint32          u32);

static gboolean write_s32_le                 (FILE            *file,
                                              gint32           s32);

static gboolean write_file_header            (FILE            *file,
                                              BitmapFileHead  *bfh);

static gboolean write_info_header            (FILE            *file,
                                              BitmapHead      *bih,
                                              enum BmpInfoVer  version);

static gboolean
write_color_map (FILE   *f,
                 guint8 *cmap,
                 gint    ncolors)
{
  gint  i;

  /* BMP color map entries are 4 bytes per color, in the order B-G-R-0 */

  for (i = 0; i < ncolors; i++)
    {
      if (EOF == putc (cmap[3 * i + 2], f) ||
          EOF == putc (cmap[3 * i + 1], f) ||
          EOF == putc (cmap[3 * i + 0], f) ||
          EOF == putc (0, f))
        {
          return FALSE;
        }
    }
  return TRUE;
}

static gboolean
warning_dialog (const gchar *primary,
                const gchar *secondary)
{
  GtkWidget *dialog;
  gboolean   ok;

  dialog = gtk_message_dialog_new (NULL, 0,
                                   GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
                                   "%s", primary);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "%s", secondary);

  gimp_window_set_transient (GTK_WINDOW (dialog));
  gtk_widget_show (dialog);

  ok = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK);

  gtk_widget_destroy (dialog);

  return ok;
}

GimpPDBStatusType
export_image (GFile         *gfile,
              GimpImage     *image,
              GimpDrawable  *drawable,
              GimpRunMode    run_mode,
              GimpProcedure *procedure,
              GObject       *config,
              GError       **error)
{
  GimpPDBStatusType ret = GIMP_PDB_EXECUTION_ERROR;
  BitmapFileHead    bitmap_file_head;
  BitmapHead        bitmap_head;
  guchar           *cmap = NULL;
  const Babl       *format;
  GimpImageType     drawable_type;
  gint              i;
  gboolean          write_color_space;
  RGBMode           rgb_format   = RGB_888;
  enum BmpInfoVer   info_version = BMPINFO_V1;
  gint              frontmatter_size;
  gboolean          indexed_bmp = FALSE;
  gboolean          allow_alpha = FALSE;
  gboolean          allow_rle   = FALSE;
  struct Fileinfo   fi;

  memset (&bitmap_file_head, 0, sizeof bitmap_file_head);
  memset (&bitmap_head, 0, sizeof bitmap_head);
  memset (&fi, 0, sizeof fi);

  /* WINDOWS_COLOR_SPACE is the most "don't care" option available for V4+
   * headers, which seems a reasonable default.
   * Microsoft chose to make 0 the value for CALIBRATED_RGB, which would
   * require specifying gamma and endpoints.
   */
  bitmap_head.bV4CSType = V4CS_WINDOWS_COLOR_SPACE;

  drawable_type = gimp_drawable_type (drawable);
  fi.width      = gimp_drawable_get_width (drawable);
  fi.height     = gimp_drawable_get_height (drawable);

  switch (drawable_type)
    {
    case GIMP_RGBA_IMAGE:
      format       = babl_format ("R'G'B'A u8");
      fi.channels  = 4;
      allow_alpha  = TRUE;

      if (run_mode == GIMP_RUN_INTERACTIVE)
        g_object_set (config,
                      "rgb-format", "rgba-8888",
                      "use-rle",    FALSE,
                      NULL);
      else
        g_object_set (config,
                      "use-rle",    FALSE,
                      NULL);
      break;

    case GIMP_RGB_IMAGE:
      format       = babl_format ("R'G'B' u8");
      fi.channels  = 3;

      if (run_mode == GIMP_RUN_INTERACTIVE)
        g_object_set (config,
                      "rgb-format", "rgb-888",
                      "use-rle",    FALSE,
                      NULL);
      else
        g_object_set (config,
                      "use-rle", FALSE,
                      NULL);
      break;

    case GIMP_GRAYA_IMAGE:
      if (run_mode == GIMP_RUN_INTERACTIVE &&
          ! warning_dialog (_("Cannot export indexed image with "
                              "transparency in BMP file format."),
                            _("Alpha channel will be ignored.")))
        {
          ret = GIMP_PDB_CANCEL;
          goto abort;
        }

     /* fallthrough */

    case GIMP_GRAY_IMAGE:
      if (drawable_type == GIMP_GRAYA_IMAGE)
        {
          format      = babl_format ("Y'A u8");
          fi.channels = 2;
        }
      else
        {
          format      = babl_format ("Y' u8");
          fi.channels = 1;
        }

      indexed_bmp = TRUE;
      fi.ncolors  = 256;

      /* create a gray-scale color map */
      cmap = g_malloc (fi.ncolors * 3);
      for (i = 0; i < fi.ncolors; i++)
        cmap[3 * i + 0] = cmap[3 * i + 1] = cmap[3 * i + 2] = i;

      break;

    case GIMP_INDEXEDA_IMAGE:
      if (run_mode == GIMP_RUN_INTERACTIVE &&
          ! warning_dialog (_("Cannot export indexed image with "
                              "transparency in BMP file format."),
                            _("Alpha channel will be ignored.")))
        {
          ret = GIMP_PDB_CANCEL;
          goto abort;
        }

     /* fallthrough */

    case GIMP_INDEXED_IMAGE:
      format   = gimp_drawable_get_format (drawable);
      cmap     = gimp_palette_get_colormap (gimp_image_get_palette (image),
                                            babl_format ("R'G'B' u8"), &fi.ncolors, NULL);

      if (drawable_type == GIMP_INDEXEDA_IMAGE)
        fi.channels = 2;
      else
        fi.channels = 1;

      indexed_bmp = TRUE;
      break;

    default:
      g_assert_not_reached ();
    }

  if (indexed_bmp)
    {
      if (fi.ncolors > 2)
        allow_rle = TRUE;
      else
        g_object_set (config, "use-rle", FALSE, NULL);
    }

  /* display export dialog and retreive selected options */

  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      if (! save_dialog (procedure, config, image, indexed_bmp, allow_alpha, allow_rle))
        {
          ret = GIMP_PDB_CANCEL;
          goto abort;
        }
    }

  g_object_get (config,
                "use-rle",           &fi.use_rle,
                "write-color-space", &write_color_space,
                NULL);

  rgb_format = gimp_procedure_config_get_choice_id (GIMP_PROCEDURE_CONFIG (config),
                                                    "rgb-format");

  if (indexed_bmp)
    {
      if (fi.ncolors > 16)
        {
          bitmap_head.biBitCnt = 8;
        }
      else if (fi.ncolors > 2)
        {
          bitmap_head.biBitCnt = 4;
        }
      else
        {
          g_assert (! fi.use_rle);
          bitmap_head.biBitCnt = 1;
        }

      bitmap_head.biClrUsed = fi.ncolors;
      bitmap_head.biClrImp  = fi.ncolors;

      if (fi.use_rle)
        bitmap_head.biCompr = bitmap_head.biBitCnt == 8 ? BI_RLE8 : BI_RLE4;
      else
        bitmap_head.biCompr = BI_RGB;
    }
  else
    {
      switch (rgb_format)
        {
        case RGB_888:
          calc_masks_from_bits (fi.cmasks, 8, 8, 8, 0);
          break;
        case RGBA_8888:
          calc_masks_from_bits (fi.cmasks, 8, 8, 8, 8);
          break;
        case RGBX_8888:
          calc_masks_from_bits (fi.cmasks, 8, 8, 8, 0);
          break;
        case RGB_565:
          calc_masks_from_bits (fi.cmasks, 5, 6, 5, 0);
          break;
        case RGBA_5551:
          calc_masks_from_bits (fi.cmasks, 5, 5, 5, 1);
          break;
        case RGB_555:
          calc_masks_from_bits (fi.cmasks, 5, 5, 5, 0);
          break;
        default:
          g_return_val_if_reached (GIMP_PDB_EXECUTION_ERROR);
        }
      bitmap_head.biBitCnt = calc_bitsperpixel_from_masks (fi.cmasks);

      /* pointless, but it exists: */
      if (bitmap_head.biBitCnt == 24 && rgb_format == RGBX_8888)
        bitmap_head.biBitCnt = 32;

      if (are_masks_well_known (fi.cmasks, bitmap_head.biBitCnt) &&
          (bitmap_head.biBitCnt == 24 || ! comp_16_and_32_only_as_bitfields))
        {
          bitmap_head.biCompr = BI_RGB;
        }
      else
        {
          bitmap_head.biCompr = BI_BITFIELDS;
          for (gint c = 0; c < 4; c++)
            bitmap_head.masks[c] = fi.cmasks[c].mask << fi.cmasks[c].shiftin;

          info_version = MAX (comp_min_header_for_bitfields, info_version);

          if (fi.cmasks[3].mask) /* have alpha channel, need at least v3 */
            info_version = MAX (BMPINFO_V3_ADOBE, info_version);

          if (! are_masks_v1_standard (fi.cmasks, bitmap_head.biBitCnt))
            info_version = MAX (comp_min_header_for_non_standard_masks, info_version);
        }
    }

  gimp_progress_init_printf (_("Exporting '%s'"),
                             gimp_file_get_utf8_name (gfile));

  if (write_color_space)
    {
      bitmap_head.bV4CSType = V4CS_sRGB;

      info_version = MAX (BMPINFO_V4, info_version);
      info_version = MAX (comp_min_header_for_colorspace, info_version);
    }

  if (comp_current_official_headers_only)
    {
      /* don't use v2/v3 headers */
      if (info_version >= BMPINFO_V2_ADOBE)
        info_version = MAX (BMPINFO_V4, info_version);
    }

  /* We should consider rejecting any width > (INT32_MAX - 31) / BitsPerPixel,
   * as the resulting BMP will likely cause integer overflow in other
   * readers.(Currently, GIMP's limit is way lower, anyway)
   */
  g_assert (fi.width <= (G_MAXSIZE - 31) / bitmap_head.biBitCnt);

  fi.bytes_per_row = (((guint64) fi.width * bitmap_head.biBitCnt + 31) / 32) * 4;

  bitmap_head.biSize = info_header_size (info_version);
  frontmatter_size   = 14 + bitmap_head.biSize + 4 * fi.ncolors;

  if (info_version < BMPINFO_V2_ADOBE && bitmap_head.biCompr == BI_BITFIELDS)
    frontmatter_size += 12; /* V1 header stores RGB masks outside header */

  bitmap_file_head.bfOffs = frontmatter_size;

  if (fi.use_rle || (guint64) fi.bytes_per_row * fi.height + frontmatter_size > UINT32_MAX)
    {
      /* for RLE, we don't know the size until after writing the image and
       * will update later.
       * Also, if the size is larger than UINT32_MAX, we write 0. Most (all?)
       * readers will ignore it, anyway. TODO: Might want to issue warning
       * in this case.
       */
      bitmap_file_head.bfSize = 0;
      bitmap_head.biSizeIm    = 0;
    }
  else
    {
      bitmap_file_head.bfSize = frontmatter_size + (fi.bytes_per_row * fi.height);
      bitmap_head.biSizeIm    = fi.bytes_per_row * fi.height;
    }

  bitmap_head.biWidth  = fi.width;
  bitmap_head.biHeight = fi.height;
  bitmap_head.biPlanes = 1;

  set_info_resolution (&bitmap_head, image);

  fi.file = g_fopen (g_file_peek_path (gfile), "wb");
  if (! fi.file)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Could not open '%s' for writing: %s"),
                   gimp_file_get_utf8_name (gfile), g_strerror (errno));
      goto abort;
    }

  bitmap_file_head.zzMagic[0] = 'B';
  bitmap_file_head.zzMagic[1] = 'M';
  if (! write_file_header (fi.file, &bitmap_file_head))
    goto abort_with_standard_message;

  if (! write_info_header (fi.file, &bitmap_head, info_version))
    goto abort_with_standard_message;

  if (fi.ncolors && ! write_color_map (fi.file, cmap, fi.ncolors))
    goto abort_with_standard_message;

  fi.bpp = bitmap_head.biBitCnt;
  if (! write_image (&fi,
                     drawable,
                     format,
                     frontmatter_size))
    goto abort_with_standard_message;

  fclose (fi.file);
  g_free (cmap);

  return GIMP_PDB_SUCCESS;

abort_with_standard_message:
  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
               _("Error writing to file."));

  /* fall through to abort */

abort:
  if (fi.file)
    fclose (fi.file);
  g_free (cmap);

  return ret;
}

static gboolean
are_masks_well_known (BitmapChannel *cmasks, gint bpp)
{
  gint c, bitsperchannel;

  /* 16/24/32-bit BMPs each have one 'well-known' BI_RGB representation
   * that doesn't require to write the masks with BI_BITFIELDS
   */

  if (cmasks[3].nbits != 0) /* alpha */
    return FALSE;

  if (bpp == 16)
    bitsperchannel = 5;
  else if (bpp == 24 || bpp == 32)
    bitsperchannel = 8;
  else
    return FALSE;

  for (c = 0; c < 3; c++)
    {
      if (cmasks[c].nbits != bitsperchannel)
        return FALSE;
    }

  return TRUE;
}

static gboolean
are_masks_v1_standard (BitmapChannel *cmasks, gint bpp)
{
  /* BITMAPINFOHEADER allowed only 5-5-5 or 5-6-5 for 16-bit and only 8-8-8
   * for 32-bit
   */

  if (cmasks[3].nbits != 0) /* alpha */
    return FALSE;

  if (bpp == 16)
    {
      if (cmasks[0].nbits == 5 &&
          (cmasks[1].nbits == 5 || cmasks[1].nbits == 6) &&
          cmasks[2].nbits == 5)
        {
          return TRUE;
        }
    }
  else if (bpp == 32)
    {
      if (cmasks[0].nbits == 8 &&
          cmasks[1].nbits == 8 &&
          cmasks[2].nbits == 8)
        {
          return TRUE;
        }
    }

  return FALSE;
}

static gint
calc_bitsperpixel_from_masks (BitmapChannel *cmasks)
{
  gint c, bitsum = 0;

  for (c = 0; c < 4; c++)
    bitsum += cmasks[c].nbits;

  if (bitsum > 16)
    {
      if (bitsum == 24 && are_masks_well_known (cmasks, 24))
        return 24;
      else
        return 32;
    }
  return 16;
}

static void
calc_masks_from_bits (BitmapChannel *cmasks, gint r, gint g, gint b, gint a)
{
  gint nbits[4] = { r, g, b, a };
  gint order[4] = { 2, 1, 0, 3 }; /* == B-G-R-A */
  gint c, bitsum = 0;

  /* calculate bitmasks for given channel bit-depths.
   *
   * Note: while for BI_BITFIELDS we are free to place the masks in any order,
   * we also use the masks for the well known 16/24/32 bit formats, we just
   * don't write them to the file. So the masks here must be prepared in the
   * proper order for those formats which is from high to low: R-G-B.
   * BMPs are little endian, so in the file they end up B-G-R-(A).
   * Because it is confusing, here in other words: blue has a shift of 0,
   * red has the second-highest shift, alpha has the highest shift.
   */

  for (c = 0; c < 4; c++)
    {
      cmasks[order[c]].nbits     = nbits[order[c]];
      cmasks[order[c]].mask      = (1UL << nbits[order[c]]) - 1;
      cmasks[order[c]].max_value = (gfloat) cmasks[order[c]].mask;
      cmasks[order[c]].shiftin   = bitsum;
      bitsum += nbits[order[c]];
    }
}

static void
set_info_resolution (BitmapHead *bih, GimpImage *image)
{
  gdouble xresolution;
  gdouble yresolution;

  gimp_image_get_resolution (image, &xresolution, &yresolution);

  if (xresolution > GIMP_MIN_RESOLUTION && yresolution > GIMP_MIN_RESOLUTION)
    {
      /*
       * xresolution and yresolution are in dots per inch.
       * BMP biXPels and biYPels are in pixels per meter.
       */
      bih->biXPels = (long int) (xresolution * 100.0 / 2.54 + 0.5);
      bih->biYPels = (long int) (yresolution * 100.0 / 2.54 + 0.5);
    }
}

static gint
info_header_size (enum BmpInfoVer version)
{
  switch (version)
    {
    case BMPINFO_CORE:
      return 12;
    case BMPINFO_V1:
      return 40;
    case BMPINFO_V2_ADOBE:
      return 52;
    case BMPINFO_V3_ADOBE:
      return 56;
    case BMPINFO_V4:
      return 108;
    case BMPINFO_V5:
      return 124;
    default:
      g_assert_not_reached ();
    }
  return 0;
}

static gboolean
write_image (struct Fileinfo *fi,
             GimpDrawable    *drawable,
             const Babl      *format,
             gint             frontmatter_size)
{
  guchar     *temp, *src = NULL, v;
  gint        xpos, ypos, i, j;
  gsize       rowstride;
  gint        breite, k;
  guchar      n;
  gint        cur_progress;
  gint        max_progress;
  gint        padding;
  GeglBuffer *buffer = NULL;
  gint        tile_height, tile_n = 0;
  gsize       line_offset;

  /* we currently only export 8-bit images; later, bytesperchannel will be
   * properly set according to the actual image precision.
   */
  fi->bytesperchannel = 1;

  tile_height = MIN (gimp_tile_height (), fi->height);

  src = g_new (guchar, (gsize) fi->width * tile_height * fi->channels);

  /* move the following into loop for now, see GEGL#400 */
  /* buffer = gimp_drawable_get_buffer (drawable); */

  rowstride = (gsize) fi->width * fi->channels * fi->bytesperchannel;

  fi->alpha = fi->channels == 4 || fi->channels == 2 ? 1 : 0;

  if (! fi->use_rle)
    {
      padding = fi->bytes_per_row - ((guint64) fi->width * fi->bpp + 7) / 8;
    }
  else
    {
      padding    = 0; /* RLE does its own pixel-based padding */
      fi->row    = g_new (guchar, fi->width / (8 / fi->bpp) + 10);
      fi->chains = g_new (guchar, fi->width / (8 / fi->bpp) + 10);
      fi->length = 0;
    }

  cur_progress = 0;
  max_progress = fi->height;

  for (ypos = fi->height - 1; ypos >= 0; ypos--)
    {
      if (tile_n == 0)
        {
          tile_n = MIN (ypos + 1, tile_height);

          /* getting and unrefing the buffer here each time (vs doing it
           * outside the loop and only calling gegl_buffer_get() here) avoids
           * memory exhaustion for very large images, see GEGL#400
           */
          buffer = gimp_drawable_get_buffer (drawable);
          gegl_buffer_get (buffer,
                           GEGL_RECTANGLE (0, ypos + 1 - tile_n, fi->width, tile_n),
                           1.0, format, src,
                           GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
          g_object_unref (buffer);
          buffer = NULL;
        }

      line_offset = rowstride * --tile_n;

      if (fi->bpp > 8)
        {
          if (! write_rgb (src + line_offset, fi))
            goto abort;
        }
      else /* indexed */
        {
          /* now it gets more difficult */
          if (! fi->use_rle || fi->bpp == 1)
            {
              /* uncompressed 1,4 and 8 bit */

              for (xpos = 0; xpos < fi->width;) /* for each _byte_ */
                {
                  v = 0;
                  for (i = 1;
                       i <= (8 / fi->bpp) && xpos < fi->width;
                       i++, xpos++) /* for each pixel */
                    {
                      temp = src + line_offset + xpos * fi->channels;

                      if (fi->channels > 1 && *(temp + 1) == 0)
                        *temp = 0x0;

                      v = v | ((guchar) *temp << (8 - (i * fi->bpp)));
                    }

                  if (fwrite (&v, 1, 1, fi->file) != 1)
                    goto abort;
                }
            }
          else
            {
              /* Save RLE encoded file, quite difficult */

              /* first copy the pixels to a buffer, making one byte
               * from two 4bit pixels
               */
              j = 0;
              for (xpos = 0; xpos < fi->width;)
                {
                  v = 0;

                  for (i = 1;
                       (i <= (8 / fi->bpp)) && (xpos < fi->width);
                       i++, xpos++)
                    {
                      /* for each pixel */

                      temp = src + line_offset + (xpos * fi->channels);

                      if (fi->channels > 1 && *(temp + 1) == 0)
                        *temp = 0x0;

                      v = v | ((guchar) *temp << (8 - (i * fi->bpp)));
                    }

                  fi->row[j++] = v;
                }

              breite = fi->width / (8 / fi->bpp);
              if (fi->width % (8 / fi->bpp))
                breite++;

              /* then check for strings of equal bytes */
              for (i = 0; i < breite; i += j)
                {
                  j = 0;

                  while ((i + j < breite) &&
                         (j < (255 / (8 / fi->bpp))) &&
                         (fi->row[i + j] == fi->row[i]))
                    j++;

                  fi->chains[i] = j;
                }

              /* then write the strings and the other pixels to the file */
              for (i = 0; i < breite;)
                {
                  if (fi->chains[i] < 3)
                    {
                      /* strings of different pixels ... */

                      j = 0;

                      while ((i + j < breite) &&
                             (j < (255 / (8 / fi->bpp))) &&
                             (fi->chains[i + j] < 3))
                        j += fi->chains[i + j];

                      /* this can only happen if j jumps over the end
                       * with a 2 in chains[i+j]
                       */
                      if (j > (255 / (8 / fi->bpp)))
                        j -= 2;

                      /* 00 01 and 00 02 are reserved */
                      if (j > 2)
                        {
                          n = j * (8 / fi->bpp);
                          if (n + i * (8 / fi->bpp) > fi->width)
                            n--;

                          if (EOF == putc (0, fi->file) || EOF == putc (n, fi->file))
                            goto abort;

                          fi->length += 2;

                          if (fwrite (&fi->row[i], 1, j, fi->file) != j)
                            goto abort;

                          fi->length += j;
                          if ((j) % 2)
                            {
                              if (EOF == putc (0, fi->file))
                                goto abort;
                              fi->length++;
                            }
                        }
                      else
                        {
                          for (k = i; k < i + j; k++)
                            {
                              n = (8 / fi->bpp);
                              if (n + i * (8 / fi->bpp) > fi->width)
                                n--;

                              if (EOF == putc (n, fi->file) || EOF == putc (fi->row[k], fi->file))
                                goto abort;
                              fi->length += 2;
                            }
                        }

                      i += j;
                    }
                  else
                    {
                      /* strings of equal pixels */

                      n = fi->chains[i] * (8 / fi->bpp);
                      if (n + i * (8 / fi->bpp) > fi->width)
                        n--;

                      if (EOF == putc (n, fi->file) || EOF == putc (fi->row[i], fi->file))
                        goto abort;

                      i += fi->chains[i];
                      fi->length += 2;
                    }
                }

              if (EOF == putc (0, fi->file) || EOF == putc (0, fi->file)) /* End of row */
                goto abort;
              fi->length += 2;

            } /* RLE */
        }

      for (int j = 0; j < padding; j++)
        {
          if (EOF == putc (0, fi->file))
            goto abort;
        }

      cur_progress++;
      if ((cur_progress % 5) == 0)
        gimp_progress_update ((gdouble) cur_progress /
                              (gdouble) max_progress);

    }

  /* g_object_unref (buffer); */

  if (fi->use_rle)
    {
      /* Overwrite last End of row with End of file */
      if (fseek (fi->file, -2, SEEK_CUR))
        goto abort;
      if (EOF == putc (0, fi->file) || EOF == putc (1, fi->file))
        goto abort;

      if (fi->length <= UINT32_MAX)
        {
          /* Write length of image data */
          if (fseek (fi->file, 0x22, SEEK_SET))
            goto abort;
          if (! write_u32_le (fi->file, fi->length))
            goto abort;

          if (fi->length <= UINT32_MAX - frontmatter_size)
            {
              /* Write length of file */
              if (fseek (fi->file, 0x02, SEEK_SET))
                goto abort;
              if (! write_u32_le (fi->file, fi->length + frontmatter_size))
                goto abort;
            }
        }
      else
        {
          /* RLE data is too big to record the size in biSizeImage.
           * According to spec, biSizeImage would have to be set for RLE
           * bmps. In reality, it is neither necessary for interpreting
           * the file, nor do readers seem to mind when field is not set,
           * so we just leave it at 0.
           *
           * TODO: Issue a warning when this happens.
           */
        }
    }

  g_free (fi->chains);
  g_free (fi->row);
  g_free (src);

  gimp_progress_update (1.0);
  return TRUE;

abort:
  if (buffer)
    g_object_unref (buffer);
  g_free (fi->chains);
  g_free (fi->row);
  g_free (src);

  return FALSE;
}

static guint32
int32_from_bytes (const guchar *src, gint bytes)
{
  guint32 ret;

  switch (bytes)
    {
    case 1:
      ret = *src;
      break;
    case 2:
      ret = *(guint16 *) src;
      break;
    case 4:
      ret = *(guint32 *) src;
      break;
    default:
      ret = 0;
      g_assert_not_reached ();
    }
  return ret;
}

static gboolean
write_rgb (const guchar *src, struct Fileinfo *fi)
{
  gint    xpos;
  guint32 channel_val[4], px32;

  channel_val[3] = 0xff; /* default alpha = opaque */

  for (xpos = 0; xpos < fi->width; xpos++)
    {
      for (gint c = 0; c < fi->channels - fi->alpha; c++)
        {
          channel_val[c] = int32_from_bytes (src, fi->bytesperchannel);
          src += fi->bytesperchannel;
        }

      if (fi->channels < 3)
        {
          /* fake grayscale */
          channel_val[1] = channel_val[2] = channel_val[0];
        }

      if (fi->alpha > 0)
        {
          channel_val[3] = int32_from_bytes (src, fi->bytesperchannel);
          src += fi->bytesperchannel;
        }

      px32 = 0;
      for (gint c = 0; c < 4; c++)
        {
          gdouble d;

          d = (gdouble) channel_val[c] / ((1 << (fi->bytesperchannel * 8)) - 1);
          d *= fi->cmasks[c].max_value;

          px32 |= (guint32) (d + 0.5) << fi->cmasks[c].shiftin;
        }

      for (gint i = 0; i < fi->bpp; i += 8)
        {
          if (EOF == putc ((px32 >> i) & 0xff, fi->file))
            return FALSE;
        }
    }
  return TRUE;
}

static gboolean
write_little_endian (FILE *file, guint32 u32, gint bytes)
{
  gint i;

  for (i = 0; i < bytes; i++)
    {
      if (EOF == putc ((u32 >> (8 * i)) & 0xff, file))
        return FALSE;
    }
  return TRUE;
}

static gboolean
write_u16_le (FILE *file, guint16 u16)
{
  return write_little_endian (file, (guint32) u16, 2);
}

static gboolean
write_u32_le (FILE *file, guint32 u32)
{
  return write_little_endian (file, u32, 4);
}

static gboolean
write_s32_le (FILE *file, gint32 s32)
{
  return write_little_endian (file, (guint32) s32, 4);
}

static gboolean
write_file_header (FILE *file, BitmapFileHead *bfh)
{
  if (fwrite (bfh->zzMagic, 1, 2, file) != 2 ||
      ! write_u32_le (file, bfh->bfSize)     ||
      ! write_u16_le (file, bfh->zzHotX)     ||
      ! write_u16_le (file, bfh->zzHotY)     ||
      ! write_u32_le (file, bfh->bfOffs))
    {
      return FALSE;
    }
  return TRUE;
}

static gboolean
write_info_header (FILE *file, BitmapHead *bih, enum BmpInfoVer version)
{
  guint32 u32;

  g_assert (version >= BMPINFO_V1 && version <= BMPINFO_V5);

  /* write at least 40-byte BITMAPINFOHEADER */

  if (! write_u32_le (file, bih->biSize)    ||
      ! write_s32_le (file, bih->biWidth)   ||
      ! write_s32_le (file, bih->biHeight)  ||
      ! write_u16_le (file, bih->biPlanes)  ||
      ! write_u16_le (file, bih->biBitCnt)  ||
      ! write_u32_le (file, bih->biCompr)   ||
      ! write_u32_le (file, bih->biSizeIm)  ||
      ! write_s32_le (file, bih->biXPels)   ||
      ! write_s32_le (file, bih->biYPels)   ||
      ! write_u32_le (file, bih->biClrUsed) ||
      ! write_u32_le (file, bih->biClrImp))
    {
      return FALSE;
    }

  if (version <= BMPINFO_V1 && bih->biCompr != BI_BITFIELDS)
    return TRUE;

  /* continue writing v2+ header or masks for v1 bitfields */

  for (gint i = 0; i < 3; i++)
    {
      /* write RGB masks, either as part of v2+ header, or after v1 header */
      if (! write_u32_le (file, bih->masks[i]))
        return FALSE;
    }

  if (version <= BMPINFO_V2_ADOBE)
    return TRUE;

  /* alpha mask only as part of v3+ header */
  if (! write_u32_le (file, bih->masks[3]))
    return FALSE;

  if (version <= BMPINFO_V3_ADOBE)
    return TRUE;

  if (! write_u32_le (file, bih->bV4CSType))
    return FALSE;

  for (gint i = 0; i < 9; i++)
    {
      /* endpoints are written as 2.30 fixed point */
      u32 = (guint32) (bih->bV4Endpoints[i] * 0x40000000L + 0.5);
      if (! write_u32_le (file, u32))
        return FALSE;
    }

  /* gamma is written as 16.16 fixed point */
  if (! write_u32_le (file, (guint32) (bih->bV4GammaRed   * 65535.0 + 0.5)) ||
      ! write_u32_le (file, (guint32) (bih->bV4GammaGreen * 65535.0 + 0.5)) ||
      ! write_u32_le (file, (guint32) (bih->bV4GammaBlue  * 65535.0 + 0.5)))
    {
      return FALSE;
    }

  if (version <= BMPINFO_V4)
    return TRUE;

  /* continue writing BITMAPV5HEADER */

  if (! write_u32_le (file, (guint32) bih->bV5Intent)      ||
      ! write_u32_le (file, (guint32) bih->bV5ProfileData) ||
      ! write_u32_le (file, (guint32) bih->bV5ProfileSize) ||
      ! write_u32_le (file, (guint32) bih->bV5Reserved))
    {
      return FALSE;
    }

  return TRUE;
}

static void
config_notify (GObject          *config,
               const GParamSpec *pspec,
               gpointer          data)
{
  gint    allow_alpha = GPOINTER_TO_INT (data);
  RGBMode format;

  format = gimp_procedure_config_get_choice_id (GIMP_PROCEDURE_CONFIG (config),
                                                "rgb-format");

  switch (format)
    {
    case RGBA_5551:
    case RGBA_8888:
      if (! allow_alpha)
        {
          g_signal_handlers_block_by_func (config, config_notify, data);

          if (format == RGBA_5551)
            g_object_set (config, "rgb-format", "rgb-565", NULL);
          else if (format == RGBA_8888)
            g_object_set (config, "rgb-format", "rgb-888", NULL);

          g_signal_handlers_unblock_by_func (config, config_notify, data);
        }
      break;

    default:
      break;
    };
}

static gboolean
save_dialog (GimpProcedure *procedure,
             GObject       *config,
             GimpImage     *image,
             gboolean       indexed,
             gboolean       allow_alpha,
             gboolean       allow_rle)
{
  GtkWidget  *dialog;
  GtkWidget  *toggle;
  GtkWidget  *vbox;
  GtkWidget  *combo;
  GParamSpec *cspec;
  GimpChoice *choice;
  gboolean    run;

  dialog = gimp_export_procedure_dialog_new (GIMP_EXPORT_PROCEDURE (procedure),
                                             GIMP_PROCEDURE_CONFIG (config),
                                             image);

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  /* Run-Length Encoded */
  gimp_procedure_dialog_set_sensitive (GIMP_PROCEDURE_DIALOG (dialog),
                                       "use-rle",
                                       allow_rle,
                                       NULL, NULL, FALSE);

  /* Compatibility Options */
  gimp_procedure_dialog_get_label (GIMP_PROCEDURE_DIALOG (dialog),
                                   "color-space-title", _("Compatibility"),
                                   FALSE, FALSE);
  toggle = gimp_procedure_dialog_get_widget (GIMP_PROCEDURE_DIALOG (dialog),
                                             "write-color-space",
                                             GTK_TYPE_CHECK_BUTTON);
  gimp_help_set_help_data (toggle,
                           _("Some applications can not read BMP images that "
                             "include color space information. GIMP writes "
                             "color space information by default. Disabling "
                             "this option will cause GIMP to not write color "
                             "space information to the file."),
                           NULL);
  gimp_procedure_dialog_fill_frame (GIMP_PROCEDURE_DIALOG (dialog),
                                    "color-space-frame", "color-space-title",
                                    FALSE, "write-color-space");

  /* RGB Encoding Options */
  combo = gimp_procedure_dialog_get_widget (GIMP_PROCEDURE_DIALOG (dialog),
                                            "rgb-format", G_TYPE_NONE);
  g_object_set (combo, "margin", 12, NULL);

  cspec  = g_object_class_find_property (G_OBJECT_GET_CLASS (config), "rgb-format");
  choice = gimp_param_spec_choice_get_choice (cspec);

  gimp_choice_set_sensitive (choice, "rgba-5551", allow_alpha);
  gimp_choice_set_sensitive (choice, "rgba-8888", allow_alpha);

  gimp_procedure_dialog_set_sensitive (GIMP_PROCEDURE_DIALOG (dialog),
                                       "rgb-format",
                                       ! indexed,
                                       NULL, NULL, FALSE);

  /* Formatting the dialog */
  vbox = gimp_procedure_dialog_fill_box (GIMP_PROCEDURE_DIALOG (dialog),
                                         "bmp-save-vbox",
                                         "use-rle",
                                         "color-space-frame",
                                         "rgb-format",
                                         NULL);
  gtk_box_set_spacing (GTK_BOX (vbox), 12);

  gimp_procedure_dialog_fill (GIMP_PROCEDURE_DIALOG (dialog),
                              "bmp-save-vbox",
                              NULL);

  gtk_widget_show (dialog);

  g_signal_connect (config, "notify::rgb-format",
                    G_CALLBACK (config_notify),
                    GINT_TO_POINTER (allow_alpha));

  run = gimp_procedure_dialog_run (GIMP_PROCEDURE_DIALOG (dialog));

  g_signal_handlers_disconnect_by_func (config,
                                        config_notify,
                                        GINT_TO_POINTER (allow_alpha));

  gtk_widget_destroy (dialog);

  return run;
}
