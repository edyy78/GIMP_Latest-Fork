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

#ifndef __GIMP_MEASURE_H__
#define __GIMP_MEASURE_H__


#include "gimpauxitem.h"


#define GIMP_MEASURE_DISTANCE_UNDEFINED G_MININT
#define GIMP_MEASURE_ANGLE_UNDEFINED G_MININT


#define GIMP_TYPE_MEASURE            (gimp_measure_get_type ())
#define GIMP_MEASURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_MEASURE, GimpMeasure))
#define GIMP_MEASURE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_MEASURE, GimpMeasureClass))
#define GIMP_IS_MEASURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_MEASURE))
#define GIMP_IS_MEASURE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_MEASURE))
#define GIMP_MEASURE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_MEASURE, GimpMeasureClass))


typedef struct _GimpMeasurePrivate GimpMeasurePrivate;
typedef struct _GimpMeasureClass   GimpMeasureClass;

struct _GimpMeasure
{
  GimpAuxItem       parent_instance;

  GimpMeasurePrivate *priv;
};

struct _GimpMeasureClass
{
  GimpAuxItemClass  parent_class;
};


GType               gimp_measure_get_type         (void) G_GNUC_CONST;

GimpMeasure *       gimp_measure_new              (gint			          distance,
                                                   gint       	          angle);

gint                gimp_measure_get_distance     (GimpMeasure           *measure);
void                gimp_measure_set_distance     (GimpMeasure           *measure,
                                                   gint  				  distance);

gint                gimp_measure_get_angle        (GimpMeasure           *measure);
void                gimp_measure_set_angle        (GimpMeasure           *measure,
                                                   gint                   angle);

#endif /* __GIMP_MEASURE_H__ */
