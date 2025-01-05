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




static gboolean write_image                  (FILE           *f,
                                              guchar         *src,
                                              gint            width,
                                              gint            height,
                                              gboolean        use_run_length_encoding,
                                              gint            channels,
                                              gint            bpp,
                                              gsize           bytes_per_row,
                                              gint            ncolors,
                                              BitmapChannel  *cmasks,
                                              gint            frontmatter_size);

static gboolean save_dialog                  (GimpProcedure  *procedure,
                                              GObject        *config,
                                              GimpImage      *image,
                                              gboolean        indexed,
                                              gboolean        allow_alpha,
                                              gboolean        allow_rle);

static void     calc_masks_from_bits         (BitmapChannel  *cmasks,
                                              gint            r,
                                              gint            g,
                                              gint            b,
                                              gint            a);

static gint     calc_bitsperpixel_from_masks (BitmapChannel  *cmasks);

static gboolean are_masks_well_known         (BitmapChannel  *cmasks,
                                              gint            bpp);

static gboolean are_masks_v1_standard        (BitmapChannel  *cmasks,
                                              gint            bpp);

static void     set_info_resolution          (BitmapHead     *bih,
                                              GimpImage      *image);

static gint     info_header_size             (enum BmpInfoVer version);

static gboolean write_u16_le                 (FILE           *file,
                                              guint16         u16);

static gboolean write_u32_le                 (FILE           *file,
                                              guint32         u32);

static gboolean write_s32_le                 (FILE           *file,
                                              gint32          s32);

static gboolean write_file_header            (FILE           *file,
                                              BitmapFileHead *bfh);

static gboolean write_info_header            (FILE           *file,
                                              BitmapHead     *bih,
                                              enum BmpInfoVer version);

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
export_image (GFile         *file,
              GimpImage     *image,
              GimpDrawable  *drawable,
              GimpRunMode    run_mode,
              GimpProcedure *procedure,
              GObject       *config,
              GError       **error)
{
  GimpPDBStatusType ret = GIMP_PDB_EXECUTION_ERROR;
  FILE             *outfile = NULL;
  BitmapFileHead    bitmap_file_head;
  BitmapHead        bitmap_head;
  guchar           *cmap = NULL;
  gint              channels;
  gsize             bytes_per_row;
  BitmapChannel     cmasks[4];
  gint              ncolors = 0;
  guchar           *pixels = NULL;
  GeglBuffer       *buffer;
  const Babl       *format;
  GimpImageType     drawable_type;
  gint              width, height;
  gint              i;
  gboolean          use_rle;
  gboolean          write_color_space;
  RGBMode           rgb_format   = RGB_888;
  enum BmpInfoVer   info_version = BMPINFO_V1;
  gint              frontmatter_size;
  gboolean          indexed_bmp = FALSE;
  gboolean          allow_alpha = FALSE;
  gboolean          allow_rle   = FALSE;

  memset (&bitmap_file_head, 0, sizeof bitmap_file_head);
  memset (&bitmap_head, 0, sizeof bitmap_head);
  memset (&cmasks, 0, sizeof cmasks);

  /* WINDOWS_COLOR_SPACE is the most "don't care" option available for V4+
   * headers, which seems a reasonable default.
   * Microsoft chose to make 0 the value for CALIBRATED_RGB, which would
   * require specifying gamma and endpoints.
   */
  bitmap_head.bV4CSType = V4CS_WINDOWS_COLOR_SPACE;

  drawable_type = gimp_drawable_type (drawable);
  width         = gimp_drawable_get_width (drawable);
  height        = gimp_drawable_get_height (drawable);

  switch (drawable_type)
    {
    case GIMP_RGBA_IMAGE:
      format       = babl_format ("R'G'B'A u8");
      channels     = 4;
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
      channels     = 3;

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
          format   = babl_format ("Y'A u8");
          channels = 2;
        }
      else
        {
          format   = babl_format ("Y' u8");
          channels = 1;
        }

      indexed_bmp = TRUE;
      ncolors     = 256;

      /* create a gray-scale color map */
      cmap = g_malloc (ncolors * 3);
      for (i = 0; i < ncolors; i++)
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
                                            babl_format ("R'G'B' u8"), &ncolors, NULL);

      if (drawable_type == GIMP_INDEXEDA_IMAGE)
        channels = 2;
      else
        channels = 1;

      indexed_bmp = TRUE;
      break;

    default:
      g_assert_not_reached ();
    }

  if (indexed_bmp)
    {
      if (ncolors > 2)
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
                "use-rle",           &use_rle,
                "write-color-space", &write_color_space,
                NULL);

  rgb_format = gimp_procedure_config_get_choice_id (GIMP_PROCEDURE_CONFIG (config),
                                                    "rgb-format");

  if (indexed_bmp)
    {
      if (ncolors > 16)
        {
          bitmap_head.biBitCnt = 8;
        }
      else if (ncolors > 2)
        {
          bitmap_head.biBitCnt = 4;
        }
      else
        {
          g_assert (! use_rle);
          bitmap_head.biBitCnt = 1;
        }

      bitmap_head.biClrUsed = ncolors;
      bitmap_head.biClrImp  = ncolors;

      if (use_rle)
        bitmap_head.biCompr = bitmap_head.biBitCnt == 8 ? BI_RLE8 : BI_RLE4;
      else
        bitmap_head.biCompr = BI_RGB;
    }
  else
    {
      switch (rgb_format)
        {
        case RGB_888:
          calc_masks_from_bits (cmasks, 8, 8, 8, 0);
          break;
        case RGBA_8888:
          calc_masks_from_bits (cmasks, 8, 8, 8, 8);
          break;
        case RGBX_8888:
          calc_masks_from_bits (cmasks, 8, 8, 8, 0);
          break;
        case RGB_565:
          calc_masks_from_bits (cmasks, 5, 6, 5, 0);
          break;
        case RGBA_5551:
          calc_masks_from_bits (cmasks, 5, 5, 5, 1);
          break;
        case RGB_555:
          calc_masks_from_bits (cmasks, 5, 5, 5, 0);
          break;
        default:
          g_return_val_if_reached (GIMP_PDB_EXECUTION_ERROR);
        }
      bitmap_head.biBitCnt = calc_bitsperpixel_from_masks (cmasks);

      /* pointless, but it exists: */
      if (bitmap_head.biBitCnt == 24 && rgb_format == RGBX_8888)
        bitmap_head.biBitCnt = 32;

      if (are_masks_well_known (cmasks, bitmap_head.biBitCnt) &&
          (bitmap_head.biBitCnt == 24 || ! comp_16_and_32_only_as_bitfields))
        {
          bitmap_head.biCompr = BI_RGB;
        }
      else
        {
          bitmap_head.biCompr = BI_BITFIELDS;
          for (gint c = 0; c < 4; c++)
            bitmap_head.masks[c] = cmasks[c].mask << cmasks[c].shiftin;

          info_version = MAX (comp_min_header_for_bitfields, info_version);

          if (cmasks[3].mask) /* have alpha channel, need at least v3 */
            info_version = MAX (BMPINFO_V3_ADOBE, info_version);

          if (! are_masks_v1_standard (cmasks, bitmap_head.biBitCnt))
            info_version = MAX (comp_min_header_for_non_standard_masks, info_version);
        }
    }

  gimp_progress_init_printf (_("Exporting '%s'"),
                             gimp_file_get_utf8_name (file));

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

  /* fetch the image */
  pixels = g_new (guchar, (gsize) width * height * channels);
  buffer = gimp_drawable_get_buffer (drawable);

  gegl_buffer_get (buffer,
                   GEGL_RECTANGLE (0, 0, width, height), 1.0,
                   format, pixels,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

  g_object_unref (buffer);

  /* We should consider rejecting any width > (INT32_MAX - 31) / BitsPerPixel,
   * as the resulting BMP will likely cause integer overflow in other
   * readers.(Currently, GIMP's limit is way lower, anyway)
   */
  g_assert (width <= (G_MAXSIZE - 31) / bitmap_head.biBitCnt);

  bytes_per_row = (((guint64) width * bitmap_head.biBitCnt + 31) / 32) * 4;

  bitmap_head.biSize = info_header_size (info_version);
  frontmatter_size   = 14 + bitmap_head.biSize + 4 * ncolors;

  if (info_version < BMPINFO_V2_ADOBE && bitmap_head.biCompr == BI_BITFIELDS)
    frontmatter_size += 12; /* V1 header stores RGB masks outside header */

  bitmap_file_head.bfOffs = frontmatter_size;

  if (use_rle || (guint64) bytes_per_row * height + frontmatter_size > UINT32_MAX)
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
      bitmap_file_head.bfSize = frontmatter_size + (bytes_per_row * height);
      bitmap_head.biSizeIm    = bytes_per_row * height;
    }

  bitmap_head.biWidth  = width;
  bitmap_head.biHeight = height;
  bitmap_head.biPlanes = 1;

  set_info_resolution (&bitmap_head, image);

  outfile = g_fopen (g_file_peek_path (file), "wb");
  if (! outfile)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Could not open '%s' for writing: %s"),
                   gimp_file_get_utf8_name (file), g_strerror (errno));
      goto abort;
    }

  bitmap_file_head.zzMagic[0] = 'B';
  bitmap_file_head.zzMagic[1] = 'M';
  if (! write_file_header (outfile, &bitmap_file_head))
    goto abort_with_standard_message;

  if (! write_info_header (outfile, &bitmap_head, info_version))
    goto abort_with_standard_message;

  if (ncolors && ! write_color_map (outfile, cmap, ncolors))
    goto abort_with_standard_message;

  if (! write_image (outfile,
                     pixels, width, height,
                     use_rle,
                     channels, bitmap_head.biBitCnt, bytes_per_row,
                     ncolors, cmasks,
                     frontmatter_size))
    goto abort_with_standard_message;

  fclose (outfile);
  g_free (pixels);
  g_free (cmap);

  return GIMP_PDB_SUCCESS;

abort_with_standard_message:
  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
               _("Error writing to file."));

  /* fall through to abort */

abort:
  if (outfile)
    fclose (outfile);
  g_free (pixels);
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
write_image (FILE          *f,
             guchar        *src,
             gint           width,
             gint           height,
             gboolean       use_run_length_encoding,
             gint           channels,
             gint           bpp,
             gsize          bytes_per_row,
             gint           ncolors,
             BitmapChannel *cmasks,
             gint           header_size)
{
  guchar *temp, v;
  guchar *row    = NULL;
  guchar *chains = NULL;
  gint    xpos, ypos, i, j, rowstride;
  guint32 px32;
  guint64 length;
  gint    breite, k;
  guchar  n;
  gint    channel_val[4];
  gint    cur_progress;
  gint    max_progress;
  gint    padding;

  xpos = 0;
  rowstride = width * channels;

  cur_progress = 0;
  max_progress = height;

  /* We'll begin with the 16/24/32 bit Bitmaps, they are easy :-) */

  if (bpp > 8)
    {
      gint alpha = channels == 4 || channels == 2 ? 1 : 0;

      padding        = bytes_per_row - ((gsize) width * (bpp / 8));
      channel_val[3] = 0xff; /* default alpha = opaque */
      for (ypos = height - 1; ypos >= 0; ypos--)
        {
          for (xpos = 0; xpos < width; xpos++)
            {
              temp = src + (ypos * rowstride) + (xpos * channels);
              channel_val[0] = *temp++;
              if (channels > 2)
                {
                  /* RGB */
                  channel_val[1] = *temp++;
                  channel_val[2] = *temp++;
                }
              else
                {
                  /* fake grayscale */
                  channel_val[1] = channel_val[2] = channel_val[0];
                }

              if (alpha)
                channel_val[3] = *temp++;

              px32 = 0;
              for (gint c = 0; c < 4; c++)
                {
                  px32 |= (guint32) (channel_val[c] / 255. * cmasks[c].max_value + 0.5)
                          << cmasks[c].shiftin;
                }

              for (j = 0; j < bpp; j += 8)
                {
                  if (EOF == putc ((px32 >> j) & 0xff, f))
                    goto abort;
                }
            }

          for (int j = 0; j < padding; j++)
            {
              if (EOF == putc (0, f))
                goto abort;
            }

          cur_progress++;
          if ((cur_progress % 5) == 0)
            gimp_progress_update ((gdouble) cur_progress /
                                  (gdouble) max_progress);
        }
    }
  else
    {
      /* now it gets more difficult */
      if (! use_run_length_encoding || bpp == 1)
        {
          /* uncompressed 1,4 and 8 bit */

          padding = bytes_per_row - ((guint64) width * bpp + 7) / 8;

          for (ypos = height - 1; ypos >= 0; ypos--) /* for each row */
            {
              for (xpos = 0; xpos < width;)  /* for each _byte_ */
                {
                  v = 0;
                  for (i = 1;
                       (i <= (8 / bpp)) && (xpos < width);
                       i++, xpos++)  /* for each pixel */
                    {
                      temp = src + (ypos * rowstride) + (xpos * channels);
                      if (channels > 1 && *(temp+1) == 0) *temp = 0x0;
                      v=v | ((guchar) *temp << (8 - (i * bpp)));
                    }

                  if (fwrite (&v, 1, 1, f) != 1)
                    goto abort;
                }

              for (int j = 0; j < padding; j++)
                {
                  if (EOF == putc (0, f))
                    goto abort;
                }

              xpos = 0;

              cur_progress++;
              if ((cur_progress % 5) == 0)
                gimp_progress_update ((gdouble) cur_progress /
                                        (gdouble) max_progress);
            }
        }
      else
        {
          /* Save RLE encoded file, quite difficult */

          length = 0;

          row    = g_new (guchar, width / (8 / bpp) + 10);
          chains = g_new (guchar, width / (8 / bpp) + 10);

          for (ypos = height - 1; ypos >= 0; ypos--)
            {
              /* each row separately */
              j = 0;

              /* first copy the pixels to a buffer, making one byte
               * from two 4bit pixels
               */
              for (xpos = 0; xpos < width;)
                {
                  v = 0;

                  for (i = 1;
                       (i <= (8 / bpp)) && (xpos < width);
                       i++, xpos++)
                    {
                      /* for each pixel */

                      temp = src + (ypos * rowstride) + (xpos * channels);
                      if (channels > 1 && *(temp+1) == 0) *temp = 0x0;
                      v = v | ((guchar) * temp << (8 - (i * bpp)));
                    }

                  row[j++] = v;
                }

              breite = width / (8 / bpp);
              if (width % (8 / bpp))
                breite++;

              /* then check for strings of equal bytes */
              for (i = 0; i < breite; i += j)
                {
                  j = 0;

                  while ((i + j < breite) &&
                         (j < (255 / (8 / bpp))) &&
                         (row[i + j] == row[i]))
                    j++;

                  chains[i] = j;
                }

              /* then write the strings and the other pixels to the file */
              for (i = 0; i < breite;)
                {
                  if (chains[i] < 3)
                    {
                      /* strings of different pixels ... */

                      j = 0;

                      while ((i + j < breite) &&
                             (j < (255 / (8 / bpp))) &&
                             (chains[i + j] < 3))
                        j += chains[i + j];

                      /* this can only happen if j jumps over the end
                       * with a 2 in chains[i+j]
                       */
                      if (j > (255 / (8 / bpp)))
                        j -= 2;

                      /* 00 01 and 00 02 are reserved */
                      if (j > 2)
                        {
                          n = j * (8 / bpp);
                          if (n + i * (8 / bpp) > width)
                            n--;

                          if (EOF == putc (0, f) || EOF == putc (n, f))
                            goto abort;

                          length += 2;

                          if (fwrite (&row[i], 1, j, f) != j)
                            goto abort;

                          length += j;
                          if ((j) % 2)
                            {
                              if (EOF == putc (0, f))
                                goto abort;
                              length++;
                            }
                        }
                      else
                        {
                          for (k = i; k < i + j; k++)
                            {
                              n = (8 / bpp);
                              if (n + i * (8 / bpp) > width)
                                n--;

                              if (EOF == putc (n, f) || EOF == putc (row[k], f))
                                goto abort;
                              length += 2;
                            }
                        }

                      i += j;
                    }
                  else
                    {
                      /* strings of equal pixels */

                      n = chains[i] * (8 / bpp);
                      if (n + i * (8 / bpp) > width)
                        n--;

                      if (EOF == putc (n, f) || EOF == putc (row[i], f))
                        goto abort;

                      i += chains[i];
                      length += 2;
                    }
                }

              if (EOF == putc (0, f) || EOF == putc (0, f))  /* End of row */
                goto abort;
              length += 2;

              cur_progress++;
              if ((cur_progress % 5) == 0)
                gimp_progress_update ((gdouble) cur_progress /
                                      (gdouble) max_progress);
            }

          if (fseek (f, -2, SEEK_CUR))                  /* Overwrite last End of row ... */
            goto abort;
          if (EOF == putc (0, f) || EOF == putc (1, f))   /* ... with End of file */
            goto abort;

          if (length <= UINT32_MAX)
            {
              /* Write length of image data */
              if (fseek (f, 0x22, SEEK_SET))
                goto abort;
              if (! write_u32_le (f, length))
                goto abort;

              /* Write length of file */
              if (fseek (f, 0x02, SEEK_SET))
                goto abort;
              if (! write_u32_le (f, length + header_size))
                goto abort;
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

          g_free (chains);
          g_free (row);
        }
    }

  gimp_progress_update (1.0);
  return TRUE;

abort:
  g_free (chains);
  g_free (row);

  return FALSE;
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
