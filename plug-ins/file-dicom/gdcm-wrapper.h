/* GIMP - The GNU Image Manipulation Program
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

#ifndef __GDCM_WRAPPER_H__
#define __GDCM_WRAPPER_H__

G_BEGIN_DECLS

/* This is fully opaque on purpose, as the calling C code must not be
 * exposed to more than this.
 */
typedef struct _GDCMLoader GDCMLoader;

typedef struct
{
  gchar *parasite_name;
  gint   element_length;
  gchar *value;
} GDCMTag;

typedef enum
{
  UINT8,
  INT8,
  UINT12,
  INT12,
  UINT16,
  INT16,
  UINT32,
  INT32,
  FLOAT16,
  FLOAT32,
  FLOAT64,
  SINGLEBIT,
  UNKNOWN
} GDCMScalarType;

typedef enum
{
  GDCM_UNKNOW = 0,
  GDCM_MONOCHROME1,
  GDCM_MONOCHROME2,
  GDCM_PALETTE_COLOR,
  GDCM_RGB,
  GDCM_HSV,
  GDCM_ARGB,
  GDCM_CMYK,
  GDCM_YBR_FULL,
  GDCM_YBR_FULL_422,
  GDCM_YBR_PARTIAL_422,
  GDCM_YBR_PARTIAL_420,
  GDCM_YBR_ICT,
  GDCM_YBR_RCT,
  GDCM_PI_END
} GDCMPIType;

GDCMLoader *   gdcm_loader_new              (const char *filename);

void           gdcm_loader_unref            (GDCMLoader *loader);
int            gdcm_loader_get_initialized  (GDCMLoader *loader);

int            gdcm_loader_get_width        (GDCMLoader *loader);
int            gdcm_loader_get_height       (GDCMLoader *loader);

GDCMScalarType gdcm_loader_get_precision    (GDCMLoader *loader);
GDCMPIType     gdcm_loader_get_image_type   (GDCMLoader *loader);

unsigned long  gdcm_loader_get_palette_size (GDCMLoader *loader);
int            gdcm_loader_get_palette      (GDCMLoader *loader,
                                             guchar     *palette);

unsigned long  gdcm_loader_get_buffer_size  (GDCMLoader *loader);
int            gdcm_loader_get_buffer       (GDCMLoader *loader,
                                             char       *pixels);

int            gdcm_loader_get_dataset_size (GDCMLoader *loader);
GDCMTag *      gdcm_loader_get_tag          (GDCMLoader *loader,
                                             int         index);

G_END_DECLS

#endif /* __GDCM_WRAPPER_H__ */
