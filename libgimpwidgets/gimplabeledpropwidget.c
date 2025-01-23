/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimplabeledpropwidget.c
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
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"

#include "gimplabeledpropwidget.h"


/**
 * SECTION: gimplabeledpropwidget
 * @title: GimpLabeledPropWidget
 * @short_description: Container widget containing a label widget
 * and an inner widget that has trait PropWidget.
 * The label is in the left column, the inner widget in the right.
 *
 * Only the contained widget has trait PropWidget; self does not.
 * Trait PropWidget: has a value synced to a config's property.
 *
 * Has property "inner-widget". Inherits property "label". Both are type Widget.
 * You might get "inner-widget" to connect its signals.
 * You might get "label" to put it in a size_group.
 * Convenience methods:
 *    super gimp_labeled_get_label()
 *    gimp_labeled_prop_widget_get_inner_widget()
 *
 * This widget is a subclass of #GimpLabeled, abstract.
 * Implements the virtual method of superclass Labeled:populate.
 * Specialized to set spacing and layout appropriate for GimpProcedureDialog:
 * label on the left, prop widget on the right, spacing 6.
 **/

/* Other implementation notes:
 * Has no signals itself, although the inner widget typically does.
 * Has no finalize method: does not ref any object.
 * Has no constructed methods: no reason to override super constructed.
 */

enum
{
  PROP_0,
  PROP_INNER_WIDGET,
};

struct _GimpLabeledPropWidget
{
  GimpLabeledClass   parent_instance;

  GtkWidget         *inner_widget;
};

static void        gimp_labeled_prop_widget_set_property      (GObject       *object,
                                                               guint          property_id,
                                                               const GValue  *value,
                                                               GParamSpec    *pspec);
static void        gimp_labeled_prop_widget_get_property      (GObject       *object,
                                                               guint          property_id,
                                                               GValue        *value,
                                                               GParamSpec    *pspec);

static GtkWidget * gimp_labeled_prop_widget_populate          (GimpLabeled   *widget,
                                                               gint          *x,
                                                               gint          *y,
                                                               gint          *width,
                                                               gint          *height);


G_DEFINE_TYPE (GimpLabeledPropWidget, gimp_labeled_prop_widget, GIMP_TYPE_LABELED)

#define parent_class gimp_labeled_prop_widget_parent_class

static void
gimp_labeled_prop_widget_class_init (GimpLabeledPropWidgetClass *klass)
{
  GObjectClass     *object_class  = G_OBJECT_CLASS (klass);
  GimpLabeledClass *labeled_class = GIMP_LABELED_CLASS (klass);

  object_class->set_property = gimp_labeled_prop_widget_set_property;
  object_class->get_property = gimp_labeled_prop_widget_get_property;

  /* Implement virtual method of super GimpLabeled. */
  labeled_class->populate    = gimp_labeled_prop_widget_populate;

  /**
   * GimpLabeledPropWidget:widget:
   *
   * The inner widget having trait PropWidget.
   *
   * Since: 3.0
   **/
  g_object_class_install_property (object_class, PROP_INNER_WIDGET,
                                   g_param_spec_object ("inner-widget", NULL,
                                                        "inner widget",
                                                        GTK_TYPE_WIDGET,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}

static void
gimp_labeled_prop_widget_init (GimpLabeledPropWidget *widget)
{
}

static void
gimp_labeled_prop_widget_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GimpLabeledPropWidget *self = GIMP_LABELED_PROP_WIDGET (object);

  switch (property_id)
    {
    case PROP_INNER_WIDGET:
      self->inner_widget = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_labeled_prop_widget_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GimpLabeledPropWidget *self = GIMP_LABELED_PROP_WIDGET (object);

  switch (property_id)
    {
    case PROP_INNER_WIDGET:
      g_value_set_object (value, self->inner_widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

/* Fills self with the inner widget.
 * Called by super:constructed.
 * Returns in OUT args any overridden position of the label widget.
 */
static GtkWidget *
gimp_labeled_prop_widget_populate (GimpLabeled *labeled,
                                   gint        *x,    /* OUT */
                                   gint        *y,
                                   gint        *width,
                                   gint        *height)
{
  GimpLabeledPropWidget *self = GIMP_LABELED_PROP_WIDGET (labeled);

  g_return_val_if_fail (self->inner_widget != NULL, NULL);

  /* Set spacing to usual for GimpProcedureDialog. */
  gtk_grid_set_column_spacing (GTK_GRID (self), 6);
  gtk_grid_set_row_spacing (GTK_GRID (self), 6);

  /* Inner widget in second column, i.e. right of label. */
  gtk_grid_attach (GTK_GRID (self), self->inner_widget, 1, 0, 1, 1);
  gtk_widget_show (self->inner_widget);

  /* By not assigning to the OUT arguments, leaves label in default position. */
  return self->inner_widget;
}


/* Public Functions */

/**
 * gimp_labeled_prop_widget_new:
 * @label_text:   The text for the #GtkLabel.
 * @inner_widget: (transfer full): The #GtkWidget to wrap.
 *
 * Returns a new #GimpLabeledPropWidget containing @inner_widget.
 * @inner_widget should have trait PropWidget, but this does not check.
 *
 * Returns: (transfer full): The new #GimpLabeledPropWidget.
 **/
GtkWidget *
gimp_labeled_prop_widget_new (const gchar *label_text,
                              GtkWidget   *inner_widget)
{
  GtkWidget *result_widget;

  g_return_val_if_fail (GTK_IS_WIDGET (inner_widget), NULL);

  result_widget = g_object_new (GIMP_TYPE_LABELED_PROP_WIDGET,
                                "label",        label_text,   /* super's property */
                                "inner_widget", inner_widget, /* self's property */
                                NULL);
  /* The above only sets the "inner_widget" property.
   * Super calls populate() to add the inner widget to self.
   */

  return result_widget;
}

/**
 * gimp_labeled_prop_widget_get_inner_widget:
 * @widget: the #GimpLabeledPropWidget.
 *
 * Returns: (transfer none): The new #GtkWidget packed next to the label.
 * It will have trait PropWidget.
 **/
GtkWidget *
gimp_labeled_prop_widget_get_inner_widget (GimpLabeledPropWidget *self)
{
  g_return_val_if_fail (GIMP_IS_LABELED_PROP_WIDGET (self), NULL);

  return self->inner_widget;
}
