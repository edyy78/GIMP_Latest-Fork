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
 */

#ifndef __BMP_H__
#define __BMP_H__


#define LOAD_PROC      "file-bmp-load"
#define EXPORT_PROC    "file-bmp-export"
#define PLUG_IN_BINARY "file-bmp"
#define PLUG_IN_ROLE   "gimp-file-bmp"

#define MAXCOLORS 256

#define ReadOK(file,buffer,len)  (fread(buffer, len, 1, file) != 0)

#if !defined(WIN32) || defined(__MINGW32__)
#define BI_RGB            0
#define BI_RLE8           1
#define BI_RLE4           2
#define BI_BITFIELDS      3
#define BI_JPEG           4
#define BI_PNG            5
#define BI_ALPHABITFIELDS 6
#endif
/* The following two are OS/2 BMP compression methods.
 * Their values (3 and 4) clash with MS values for
 * BI_BITFIELDS and BI_JPEG. We make up our own distinct
 * values and assign them as soon as we identify these
 * methods. */
#define BI_OS2_HUFFMAN (100 + BI_BITFIELDS)
#define BI_OS2_RLE24   (100 + BI_JPEG)

/* bV4CSType values */
#define V4CS_CALIBRATED_RGB      0x00000000 /* = use gamma and endpoint values */
#define V4CS_sRGB                0x73524742 /* 'sRGB' */
#define V4CS_WINDOWS_COLOR_SPACE 0x57696e20 /* 'Win ' */
#define V4CS_PROFILE_LINKED      0x4c494e4b /* 'LINK' */
#define V4CS_PROFILE_EMBEDDED    0x4d424544 /* 'MBED' */

typedef struct
{
  gchar    zzMagic[2];  /* 00 "BM" */
  guint32  bfSize;      /* 02 */
  guint16  zzHotX;      /* 06 */
  guint16  zzHotY;      /* 08 */
  guint32  bfOffs;      /* 0A */
} BitmapFileHead;

typedef struct
{
  guint32  biSize;      /* 0E */
  gint32   biWidth;     /* 12 */
  gint32   biHeight;    /* 16 */
  guint16  biPlanes;    /* 1A */
  guint16  biBitCnt;    /* 1C */
  guint32  biCompr;     /* 1E */
  guint32  biSizeIm;    /* 22 */
  guint32  biXPels;     /* 26 */
  guint32  biYPels;     /* 2A */
  guint32  biClrUsed;   /* 2E */
  guint32  biClrImp;    /* 32 */
  guint32  masks[4];    /* 36 */
  guint32  bV4CSType;
  gdouble  bV4Endpoints[9];
  gdouble  bV4GammaRed;
  gdouble  bV4GammaGreen;
  gdouble  bV4GammaBlue;
  guint32  bV5Intent;
  guint32  bV5ProfileData;
  guint32  bV5ProfileSize;
  guint32  bV5Reserved;
} BitmapHead;

typedef struct
{
  guint32 mask;
  guint32 shiftin;
  gfloat  max_value;
  gint    nbits;
} BitmapChannel;

enum BmpInfoVer
{
  /* The only bmp info headers that clearly have a version number attributed
   * to them are BITMAPV4HEADER and BITMAPV5HEADER.
   * BITMAPINFOHEADER is sometimes referred to as v1 and sometimes as v3. MS
   * themselves never seemed to give it a version number, but according to
   * Adobe, the 52- and 56-byte extensions were versioned v2 and v3 by MS.
   * The association of the number 3 with the BITMAPINFOHEADER might stem from
   * the fact that it was originally kown as the Windows 3 bitmap.
   * Anyway, v1 seems to make sense, so let's call it that for our purposes.
   */

  BMPINFO_NONE,     /* not specified */
  BMPINFO_CORE,     /* BITMAPCOREHEADER, aka OS21XBITMAPHEADER */
  BMPINFO_OS22X,    /* OS22XBITMAPHEADER (actually named BITMAPINFOHEADER2 in OS/2) */
  BMPINFO_V1,       /* BITMAPINFOHEADER (Windows 3.x, updated by Win95 / NT 4.0) */
  BMPINFO_V2_ADOBE, /* BITMAPINFOHEADER + RGB masks */
  BMPINFO_V3_ADOBE, /* BITMAPINFOHEADER + RGBA masks */
  BMPINFO_V4,       /* BITMAPV4HEADER   (Windows 95 / NT 4.0) */
  BMPINFO_V5,       /* BITMAPV5HEADER   (Windows 98 / NT 5.0) */
  BMPINFO_FUTURE    /* future, yet unknown, headers */
};


#endif /* __BMP_H__ */
