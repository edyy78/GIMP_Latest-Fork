/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimplabeledpropwidget.h
 * Copyright (C) 2024 lloyd konneker
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#if !defined (__GIMP_WIDGETS_H_INSIDE__) && !defined (GIMP_WIDGETS_COMPILATION)
#error "Only <libgimpwidgets/gimpwidgets.h> can be included directly."
#endif

#ifndef __GIMP_LABELED_PROP_WIDGET_H__
#define __GIMP_LABELED_PROP_WIDGET_H__

#include <libgimpwidgets/gimplabeled.h>

G_BEGIN_DECLS


#define GIMP_TYPE_LABELED_PROP_WIDGET (gimp_labeled_prop_widget_get_type ())
G_DECLARE_FINAL_TYPE (GimpLabeledPropWidget, gimp_labeled_prop_widget,
                      GIMP, LABELED_PROP_WIDGET, GimpLabeled)

GtkWidget *gimp_labeled_prop_widget_new              (const gchar           *label_text,
                                                      GtkWidget             *inner_widget);

GtkWidget *gimp_labeled_prop_widget_get_inner_widget (GimpLabeledPropWidget *widget);


G_END_DECLS

#endif /* __GIMP_LABELED_PROP_WIDGET_H__ */
