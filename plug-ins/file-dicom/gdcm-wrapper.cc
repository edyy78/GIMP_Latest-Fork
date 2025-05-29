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

#include "config.h"

#include <iostream>
#include <fstream>

#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpmodule/gimpmodule.h"
#include "libgimpthumb/gimpthumb.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "gdcm-3.0/gdcmImage.h"
#include "gdcm-3.0/gdcmImageChangePhotometricInterpretation.h"
#include "gdcm-3.0/gdcmImageChangePlanarConfiguration.h"
#include "gdcm-3.0/gdcmImageReader.h"
#include "gdcm-3.0/gdcmPixmap.h"
#include "gdcm-3.0/gdcmPixmapReader.h"
#include "gdcm-3.0/gdcmStringFilter.h"
#include "gdcm-3.0/gdcmSystem.h"
#include "gdcm-3.0/gdcmVR.h"

#include "gdcm-wrapper.h"

struct _GDCMLoader
{
  _GDCMLoader (const char* filename)
  {
    is_initialized_ = 0;

    reader_.SetFileName (filename);
    pix_reader_.SetFileName (filename);
    if (! reader_.Read() || ! pix_reader_.Read())
      return;

    ir_ = reader_.GetImage();

    pixeltype_ = ir_.GetPixelFormat();
    pi_        = ir_.GetPhotometricInterpretation();

    dataset_ = reader_.GetFile().GetDataSet();

    refcount_       = 1;
    is_initialized_ = 1;
  }

  int get_initialized () const
    {
      return is_initialized_;
    }

  int get_width( ) const
    {
      return ir_.GetDimension (0);
    }

  int get_height () const
    {
      return ir_.GetDimension (1);
    }

  GDCMScalarType get_precision () const
    {
      GDCMScalarType precision;

      precision = (GDCMScalarType) pixeltype_.GetScalarType();

      return precision;
    }

  GDCMPIType get_image_type () const
    {
      GDCMPIType type;

      type = (GDCMPIType) pi_.GetType ();

      return type;
    }

  unsigned long get_palette_size () const
    {
      const gdcm::LookupTable &lut = ir_.GetLUT();

      return lut.GetLUTLength (gdcm::LookupTable::RED);
    }

  int get_palette (guchar *palette) const
    {
      const gdcm::LookupTable &lut = ir_.GetLUT();

      lut.GetBufferAsRGBA (palette);

      return 0;
    }

  unsigned long get_buffer_size () const
    {
      return ir_.GetBufferLength ();
    }

  int get_buffer (char *pixel) const
    {
      const gdcm::Pixmap &pr_ = pix_reader_.GetPixmap();

      /* Standardize monochrome input to black == 0, white == 1.0 */
      if (pi_.GetType () == gdcm::PhotometricInterpretation::MONOCHROME1)
        {
          gdcm::ImageChangePhotometricInterpretation pifilt;
          gdcm::PhotometricInterpretation            pi_temp (gdcm::PhotometricInterpretation::MONOCHROME2);

          pifilt.SetInput (pr_);
          pifilt.SetPhotometricInterpretation (pi_temp);
          pifilt.Change ();
        }

      /* Make sure planar configuration is set to non-planar (0) */
      if (ir_.GetPlanarConfiguration () != 0)
        {
          gdcm::ImageChangePlanarConfiguration  plafilt;

          plafilt.SetInput (pr_);
          plafilt.SetPlanarConfiguration (0);
          plafilt.Change ();
        }

      pr_.GetBuffer (pixel);

      return 0;
    }

  int get_dataset_size () const
    {
      gdcm::DataSet::ConstIterator iter;
      int                          dataset_size = 0;

      iter = dataset_.Begin();
      for(; iter != dataset_.End (); ++iter)
        {
          const gdcm::DataElement &ref = *iter;

          if (ref.IsEmpty ())
            continue;

          dataset_size++;
        }

      return dataset_size;
    }

  GDCMTag * get_tag (int index) const
    {
      gdcm::StringFilter           sf    = gdcm::StringFilter();
      gdcm::DataSet::ConstIterator iter;
      int                          count = 0;

      sf.SetFile (reader_.GetFile ());
      iter = dataset_.Begin();
      for(; iter != dataset_.End (); ++iter)
        {
          const gdcm::DataElement &ref = *iter;

          if (ref.IsEmpty ())
            {
              continue;
            }
          else if (index == count)
            {
              GDCMTag            *dcm_tag;
              const gdcm::Tag    &tag = ref.GetTag ();
              const gdcm::VR     &vr  = ref.GetVR ();
              std::string         value;
              char                buffer[1000];
              int                 size;

              value = &(sf.ToString (tag))[0];

              if (value.empty())
                continue;

              size = snprintf (buffer, 1000, "dcm/%04x-%04x-%s",
                               tag.GetGroup (), tag.GetElement (),
                               gdcm::VR::GetVRString (vr));

              if (size >= 0 && size < 999)
                {
                  buffer[size] = '\0';
                  size++;

                  dcm_tag = g_new (GDCMTag, 1);

                  dcm_tag->parasite_name  = (gchar *) g_memdup2 (buffer, size);
                  dcm_tag->element_length = value.size();
                  dcm_tag->value          = (gchar *) g_memdup2 (&value[0], value.size());

                  return dcm_tag;
                }
              else
                {
                  return NULL;
                }
            }

            count++;
        }

      return NULL;
    }

  gdcm::DataSet                   dataset_;
  gdcm::ImageReader               reader_;
  gdcm::Image                     ir_;
  gdcm::PixmapReader              pix_reader_;
  gdcm::PixelFormat               pixeltype_;
  gdcm::PhotometricInterpretation pi_;
  int                             is_initialized_;
  size_t                          refcount_;
};

/* Public Functions */
GDCMLoader *
gdcm_loader_new (const char *filename)
{
  GDCMLoader *file;

  try
    {
      file = new GDCMLoader (filename);
    }
  catch (...)
    {
      file = NULL;
    }

  return file;
}

void
gdcm_loader_unref (GDCMLoader *loader)
{
  if (--loader->refcount_ == 0)
    {
      delete loader;
    }
}

int
gdcm_loader_get_initialized (GDCMLoader *loader)
{
  return loader->get_initialized ();
}

int
gdcm_loader_get_width (GDCMLoader *loader)
{
  int width;

  try
    {
      width = loader->get_width ();
    }
  catch (...)
    {
      width = -1;
    }

  return width;
}

int
gdcm_loader_get_height (GDCMLoader *loader)
{
  int height;

  try
    {
      height = loader->get_height ();
    }
  catch (...)
    {
      height = -1;
    }

  return height;
}

GDCMScalarType
gdcm_loader_get_precision (GDCMLoader *loader)
{
  return loader->get_precision();
}

GDCMPIType
gdcm_loader_get_image_type (GDCMLoader *loader)
{
  return loader->get_image_type();
}

unsigned long
gdcm_loader_get_palette_size (GDCMLoader *loader)
{
  unsigned long palette_size;

  try
    {
       palette_size = loader->get_palette_size ();
    }
  catch (...)
    {
      palette_size = -1;
    }

  return palette_size;
}

int
gdcm_loader_get_palette (GDCMLoader *loader,
                         guchar     *palette)
{
  int retval = -1;

  try
    {
      retval = loader->get_palette (palette);
    }
  catch (...)
    {
      retval = -1;
    }

  return retval;
}

unsigned long
gdcm_loader_get_buffer_size (GDCMLoader *loader)
{
  unsigned long buffer_size;

  try
    {
       buffer_size = loader->get_buffer_size ();
    }
  catch (...)
    {
      buffer_size = -1;
    }

  return buffer_size;
}

int
gdcm_loader_get_buffer (GDCMLoader *loader,
                        char       *pixels)
{
  int retval = -1;

  try
    {
      retval = loader->get_buffer (pixels);
    }
  catch (...)
    {
      retval = -1;
    }

  return retval;
}

int
gdcm_loader_get_dataset_size (GDCMLoader *loader)
{
  return loader->get_dataset_size ();
}

GDCMTag *
gdcm_loader_get_tag (GDCMLoader *loader,
                     int         index)
{
  return loader->get_tag (index);
}
