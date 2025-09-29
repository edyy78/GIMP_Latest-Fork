/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpoperationnormalmode.c
 * Copyright (C) 2012 Michael Natterer <mitch@gimp.org>
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

#include <gio/gio.h>
#include <gegl-plugin.h>

#include "libgimpbase/gimpbase.h"
#include "opencl/gegl-cl.h"

#include "../operations-types.h"
#include "app/operations/opencl/layer-mode-normal.cl.h"

#include "gimpoperationnormal.h"


G_DEFINE_TYPE (GimpOperationNormal, gimp_operation_normal,
               GIMP_TYPE_OPERATION_LAYER_MODE)


static const gchar* reference_xml = "<?xml version='1.0' encoding='UTF-8'?>"
"<gegl>"
"<node operation='gimp:normal'>"
"  <node operation='gegl:load'>"
"    <params>"
"      <param name='path'>blending-test-B.png</param>"
"    </params>"
"  </node>"
"</node>"
"<node operation='gegl:load'>"
"  <params>"
"    <param name='path'>blending-test-A.png</param>"
"  </params>"
"</node>"
"</gegl>";


static void
gimp_operation_normal_class_init (GimpOperationNormalClass *klass)
{
  GeglOperationClass          *operation_class  = GEGL_OPERATION_CLASS (klass);
  GimpOperationLayerModeClass *layer_mode_class = GIMP_OPERATION_LAYER_MODE_CLASS (klass);

  gegl_operation_class_set_keys (operation_class,
                                 "name",                  "gimp:normal",
                                 "description",           "GIMP normal mode operation",
                                 "reference-image",       "normal-mode.png",
                                 "reference-composition", reference_xml,
                                 NULL);

  layer_mode_class->process    = gimp_operation_normal_process;
  layer_mode_class->cl_process = gimp_operation_normal_cl_process;

#if COMPILE_SSE2_INTRINISICS
  if (gimp_cpu_accel_get_support() & GIMP_CPU_ACCEL_X86_SSE2)
    layer_mode_class->process = gimp_operation_normal_process_sse2;
#endif /* COMPILE_SSE2_INTRINISICS */

#if COMPILE_SSE4_1_INTRINISICS
  if (gimp_cpu_accel_get_support() & GIMP_CPU_ACCEL_X86_SSE4_1)
    layer_mode_class->process = gimp_operation_normal_process_sse4;
#endif /* COMPILE_SSE4_1_INTRINISICS */
}

static void
gimp_operation_normal_init (GimpOperationNormal *self)
{
}

gboolean
gimp_operation_normal_cl_process (GeglOperation *operation,
                                  cl_mem in_tex,
                                  cl_mem layer_tex,
                                  cl_mem mask_tex,
                                  cl_mem out_tex,
                                  size_t global_worksize,
                                  const GeglRectangle *roi,
                                  gint level)
{
  static GeglClRunData   *cl_data    = NULL;
  GimpOperationLayerMode *layer_mode = GIMP_OPERATION_LAYER_MODE (operation);
  /* layer_mode->opacity needs to be cast down to float to properly fit into cl_float */
  float                  opacity    = (float)layer_mode->opacity;
  cl_int                  cl_err     = 0;
  gint                    kernel     = 0;

  if (!cl_data)
    {
      const char *kernel_names[] = { "kernel_gimp_operation_normal_union_with_mask",
                                     "kernel_gimp_operation_normal_union",
                                     "kernel_gimp_operation_normal_clip_to_backdrop_with_mask",
                                     "kernel_gimp_operation_normal_clip_to_backdrop",
                                     "kernel_gimp_operation_normal_clip_to_layer_with_mask",
                                     "kernel_gimp_operation_normal_clip_to_layer",
                                     "kernel_gimp_operation_normal_intersection_with_mask",
                                     "kernel_gimp_operation_normal_intersection", NULL };
      cl_data = gegl_cl_compile_and_build (layer_mode_normal_cl_source, kernel_names);
      if (!cl_data)
        return TRUE;
    }

  switch (layer_mode->composite_mode)
    {
    case GIMP_LAYER_COMPOSITE_UNION:
    case GIMP_LAYER_COMPOSITE_AUTO:
      kernel = 0;
      break;
    case GIMP_LAYER_COMPOSITE_CLIP_TO_BACKDROP:
      kernel = 2;
      break;
    case GIMP_LAYER_COMPOSITE_CLIP_TO_LAYER:
      kernel = 4;
      break;
    case GIMP_LAYER_COMPOSITE_INTERSECTION:
      kernel = 6;
      break;
    }

  if (mask_tex != NULL)
    {
      cl_err = gegl_cl_set_kernel_args (cl_data->kernel[kernel],
                                        sizeof(cl_mem),   &in_tex,
                                        sizeof(cl_mem),   &layer_tex,
                                        sizeof(cl_mem),   &mask_tex,
                                        sizeof(cl_mem),   &out_tex,
                                        sizeof(cl_float), &opacity,
                                        NULL);
      CL_CHECK;
    }
  else
    {
      kernel += 1;
      cl_err = gegl_cl_set_kernel_args (cl_data->kernel[kernel],
                                        sizeof(cl_mem),   &in_tex,
                                        sizeof(cl_mem),   &layer_tex,
                                        sizeof(cl_mem),   &out_tex,
                                        sizeof(cl_float), &opacity,
                                        NULL);
      CL_CHECK;
    }

  cl_err = gegl_clEnqueueNDRangeKernel (gegl_cl_get_command_queue (),
                                        cl_data->kernel[kernel], 1,
                                        NULL, &global_worksize, NULL,
                                        0, NULL, NULL);
  CL_CHECK;

  return FALSE;

error:
  return TRUE;
}

gboolean
gimp_operation_normal_process (GeglOperation       *op,
                               void                *in_p,
                               void                *layer_p,
                               void                *mask_p,
                               void                *out_p,
                               glong                samples,
                               const GeglRectangle *roi,
                               gint                 level)
{
  GimpOperationLayerMode *layer_mode = (gpointer) op;
  gfloat                 *in         = in_p;
  gfloat                 *out        = out_p;
  gfloat                 *layer      = layer_p;
  gfloat                 *mask       = mask_p;
  gfloat                  opacity    = layer_mode->opacity;
  const gboolean          has_mask   = mask != NULL;

  switch (layer_mode->composite_mode)
    {
    case GIMP_LAYER_COMPOSITE_UNION:
    case GIMP_LAYER_COMPOSITE_AUTO:
      while (samples--)
        {
          gfloat layer_alpha;

          layer_alpha = layer[ALPHA] * opacity;
          if (has_mask)
            layer_alpha *= *mask;

          out[ALPHA] = layer_alpha + in[ALPHA] - layer_alpha * in[ALPHA];

          if (out[ALPHA])
            {
              gfloat layer_weight = layer_alpha / out[ALPHA];
              gfloat in_weight    = 1.0f - layer_weight;
              gint   b;

              for (b = RED; b < ALPHA; b++)
                {
                  out[b] = layer[b] * layer_weight + in[b] * in_weight;
                }
            }
          else
            {
              gint b;

              for (b = RED; b < ALPHA; b++)
                {
                  out[b] = in[b];
                }
            }

          in    += 4;
          layer += 4;
          out   += 4;

          if (has_mask)
            mask++;
        }
      break;

    case GIMP_LAYER_COMPOSITE_CLIP_TO_BACKDROP:
      while (samples--)
        {
          gfloat layer_alpha;

          layer_alpha = layer[ALPHA] * opacity;
          if (has_mask)
            layer_alpha *= *mask;

          out[ALPHA] = in[ALPHA];

          if (out[ALPHA])
            {
              gint b;

              for (b = RED; b < ALPHA; b++)
                {
                  out[b] = in[b] + (layer[b] - in[b]) * layer_alpha;
                }
            }
          else
            {
              gint b;

              for (b = RED; b < ALPHA; b++)
                {
                  out[b] = in[b];
                }
            }

          in    += 4;
          layer += 4;
          out   += 4;

          if (has_mask)
            mask++;
        }
      break;

    case GIMP_LAYER_COMPOSITE_CLIP_TO_LAYER:
      while (samples--)
        {
          gfloat layer_alpha;

          layer_alpha = layer[ALPHA] * opacity;
          if (has_mask)
            layer_alpha *= *mask;

          out[ALPHA] = layer_alpha;

          if (out[ALPHA])
            {
              gint b;

              for (b = RED; b < ALPHA; b++)
                {
                  out[b] = layer[b];
                }
            }
          else
            {
              gint b;

              for (b = RED; b < ALPHA; b++)
                {
                  out[b] = in[b];
                }
            }

          in    += 4;
          layer += 4;
          out   += 4;

          if (has_mask)
            mask++;
        }
      break;

    case GIMP_LAYER_COMPOSITE_INTERSECTION:
      while (samples--)
        {
          gfloat layer_alpha;

          layer_alpha = layer[ALPHA] * opacity;
          if (has_mask)
            layer_alpha *= *mask;

          out[ALPHA] = in[ALPHA] * layer_alpha;

          if (out[ALPHA])
            {
              gint b;

              for (b = RED; b < ALPHA; b++)
                {
                  out[b] = layer[b];
                }
            }
          else
            {
              gint b;

              for (b = RED; b < ALPHA; b++)
                {
                  out[b] = in[b];
                }
            }

          in    += 4;
          layer += 4;
          out   += 4;

          if (has_mask)
            mask++;
        }
      break;
    }

  return TRUE;
}
