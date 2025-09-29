/* GIMP - The GNU Image Manipulation Program
 *
 * GimpMeasure
 * Copyright (C) 2020  Gregory Reshetniak
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

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"

#include "core-types.h"

#include "gimpmeasure.h"


enum
{
  PROP_0,
  PROP_DISTANCE,
  PROP_ANGLE
};


struct _GimpMeasurePrivate
{
  gint                 distance;
  gint                 angle;
};


static void   gimp_measure_get_property (GObject      *object,
                                         guint         property_id,
                                         GValue       *value,
                                         GParamSpec   *pspec);
static void   gimp_measure_set_property (GObject      *object,
                                         guint         property_id,
                                         const GValue *value,
                                         GParamSpec   *pspec);


G_DEFINE_TYPE_WITH_PRIVATE (GimpMeasure, gimp_measure, GIMP_TYPE_AUX_ITEM)


static void
gimp_measure_class_init (GimpMeasureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gimp_measure_get_property;
  object_class->set_property = gimp_measure_set_property;

  GIMP_CONFIG_PROP_ENUM (object_class, PROP_DISTANCE,
                         "distance",
                         NULL, NULL,
                         GIMP_TYPE_DISTANCE_TYPE,
                         GIMP_DISTANCE_UNKNOWN,
                         0);

  GIMP_CONFIG_PROP_INT (object_class, PROP_ANGLE,
                        "angle",
                        NULL, NULL,
                        GIMP_TYPE_ANGLE_TYPE,
                        GIMP_ANGLE_UNKNOWN,
                        0);
}

static void
gimp_measure_init (GimpMeasure *measure)
{
  measure->priv = gimp_measure_get_instance_private (measure);
}

static void
gimp_measure_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GimpMeasure *measure = GIMP_MEASURE (object);

  switch (property_id)
    {
    case PROP_DISTANCE:
      g_value_set_int (value, measure->priv->distance);
      break;
    case PROP_ANGLE:
      g_value_set_int (value, measure->priv->angle);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_measure_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GimpMeasure *measure = GIMP_MEASURE (object);

  switch (property_id)
    {
    case PROP_DISTANCE:
      measure->priv->distance = g_value_get_int (value);
      break;
    case PROP_ANGLE:
      measure->priv->angle = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

GimpMeasure *
gimp_measure_new (gint distance,
                  gint angle)
{
  return g_object_new (GIMP_TYPE_MEASURE,
                       "distance",    distance,
                       "angle",       angle,
                       NULL);
}

gint
gimp_measure_get_distance (GimpMeasure *measure)
{
  g_return_val_if_fail (GIMP_IS_MEASURE (measure), GIMP_DISTANCE_UNKNOWN);

  return measure->priv->distance;
}

void
gimp_measure_set_distance (GimpMeasure          *measure,
                           gint                  distance)
{
  g_return_if_fail (GIMP_IS_MEASURE (measure));

  measure->priv->distance = distance;

  g_object_notify (G_OBJECT (measure), "distance");
}

gint
gimp_measure_get_angle (GimpMeasure *measure)
{
  g_return_val_if_fail (GIMP_IS_MEASURE (measure), GIMP_MEASURE_ANGLE_UNDEFINED);

  return measure->priv->angle;
}

void
gimp_measure_set_angle (GimpMeasure *measure,
                        gint        angle)
{
  g_return_if_fail (GIMP_IS_MEASURE (measure));

  measure->priv->angle = angle;

  g_object_notify (G_OBJECT (measure), "angle");
}
