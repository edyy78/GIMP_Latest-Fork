/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-1999 Spencer Kimball and Peter Mattis
 *
 * gimpfilleditor.c
 * Copyright (C) 2008 Michael Natterer <mitch@gimp.org>
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

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"


#include "gimpcolorpanel.h"
#include "gimpfilleditor.h"
#include "gimppropwidgets.h"
#include "gimpviewablebox.h"
#include "gimpwidgets-utils.h"

#include "gimp-intl.h"


enum
{
  PROP_0,
  PROP_OPTIONS,
  PROP_EDIT_CONTEXT,
  PROP_USE_CUSTOM_STYLE
};

struct _GimpFillEditorPrivate
{
  GtkStack *stack;
};

static void   gimp_fill_editor_constructed  (GObject      *object);
static void   gimp_fill_editor_finalize     (GObject      *object);
static void   gimp_fill_editor_set_property (GObject      *object,
                                             guint         property_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static void   gimp_fill_editor_get_property (GObject      *object,
                                             guint         property_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);
static void   gimp_fill_editor_switcher_notify
                                            (GObject      *object,
                                             GParamSpec   *pspec,
                                             gpointer      data);


G_DEFINE_TYPE_WITH_PRIVATE (GimpFillEditor, gimp_fill_editor, GTK_TYPE_BOX)

#define parent_class gimp_fill_editor_parent_class


static void
gimp_fill_editor_class_init (GimpFillEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = gimp_fill_editor_constructed;
  object_class->finalize     = gimp_fill_editor_finalize;
  object_class->set_property = gimp_fill_editor_set_property;
  object_class->get_property = gimp_fill_editor_get_property;

  g_object_class_install_property (object_class, PROP_OPTIONS,
                                   g_param_spec_object ("options",
                                                        NULL, NULL,
                                                        GIMP_TYPE_FILL_OPTIONS,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_EDIT_CONTEXT,
                                   g_param_spec_boolean ("edit-context",
                                                         NULL, NULL,
                                                         FALSE,
                                                         GIMP_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_USE_CUSTOM_STYLE,
                                   g_param_spec_boolean ("use-custom-style",
                                                         NULL, NULL,
                                                         FALSE,
                                                         GIMP_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY));
}

static void
gimp_fill_editor_init (GimpFillEditor *editor)
{
  GimpFillEditorPrivate *priv;

  g_return_if_fail (GIMP_IS_FILL_EDITOR (editor));

  priv = gimp_fill_editor_get_instance_private (editor);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (editor),
                                  GTK_ORIENTATION_VERTICAL);

  gtk_box_set_spacing (GTK_BOX (editor), 6);

  editor->private = priv;
}

static void
gimp_fill_editor_constructed (GObject *object)
{
  GimpFillEditor *editor = GIMP_FILL_EDITOR (object);
  GtkWidget      *button;

  G_OBJECT_CLASS (parent_class)->constructed (object);

  gimp_assert (GIMP_IS_FILL_OPTIONS (editor->options));

  if (editor->edit_context)
    {
      GtkWidget *color_box;
      GtkWidget *pattern_box;
      GtkWidget *switcher;
      GtkWidget *stack;

      switcher = gtk_stack_switcher_new ();
      stack    = gtk_stack_new ();

      gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));
      gtk_box_pack_start (GTK_BOX (editor), switcher, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (editor), stack, FALSE, FALSE, 0);
      gtk_widget_show (switcher);
      gtk_widget_show (stack);

      color_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

      button = gimp_prop_color_button_new (G_OBJECT (editor->options),
                                           "foreground",
                                           _("Fill Color"),
                                           1, 24,
                                           GIMP_COLOR_AREA_SMALL_CHECKS);
      gimp_color_button_set_update (GIMP_COLOR_BUTTON (button), TRUE);
      gimp_color_panel_set_context (GIMP_COLOR_PANEL (button),
                                    GIMP_CONTEXT (editor->options));
      gtk_box_pack_start (GTK_BOX (color_box), button, FALSE, FALSE, 0);

      gtk_stack_add_titled (GTK_STACK (stack), color_box, "color-fg",
                            _("Solid color"));
      gtk_widget_show (color_box);

      if (! editor->use_custom_style)
        {
          color_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

          button = gimp_prop_color_button_new (G_OBJECT (editor->options),
                                               "background",
                                               _("Fill BG Color"),
                                               1, 24,
                                               GIMP_COLOR_AREA_SMALL_CHECKS);
          gimp_color_button_set_update (GIMP_COLOR_BUTTON (button), TRUE);
          gimp_color_panel_set_context (GIMP_COLOR_PANEL (button),
                                        GIMP_CONTEXT (editor->options));
          gtk_box_pack_start (GTK_BOX (color_box), button, FALSE, FALSE, 0);
          gtk_widget_show (button);

          gtk_stack_add_titled (GTK_STACK (stack), color_box, "color-bg",
                                _("Solid BG color"));
          gtk_widget_show (color_box);
        }

      pattern_box = gimp_prop_pattern_box_new (NULL,
                                               GIMP_CONTEXT (editor->options),
                                               NULL, 2,
                                               "pattern-view-type",
                                               "pattern-view-size");
      gtk_stack_add_titled (GTK_STACK (stack), pattern_box, "pattern",
                            _("Pattern"));
      gtk_widget_show (pattern_box);

      g_signal_connect_object (stack,
                               "notify::visible-child-name",
                               G_CALLBACK (gimp_fill_editor_switcher_notify),
                               editor, 0);

      editor->private->stack = GTK_STACK (stack);
    }
  else
    {
      GtkWidget *box;

      if (editor->use_custom_style)
        box = gimp_prop_enum_radio_box_new (G_OBJECT (editor->options),
                                            "custom-style", 0, 0);
      else
        box = gimp_prop_enum_radio_box_new (G_OBJECT (editor->options), "style",
                                            0, 0);

      gtk_box_pack_start (GTK_BOX (editor), box, FALSE, FALSE, 0);
    }

  button = gimp_prop_check_button_new (G_OBJECT (editor->options),
                                       "antialias",
                                       _("_Antialiasing"));
  gtk_box_pack_start (GTK_BOX (editor), button, FALSE, FALSE, 0);
}

static void
gimp_fill_editor_finalize (GObject *object)
{
  GimpFillEditor *editor = GIMP_FILL_EDITOR (object);

  g_clear_object (&editor->options);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_fill_editor_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GimpFillEditor *editor = GIMP_FILL_EDITOR (object);

  switch (property_id)
    {
    case PROP_OPTIONS:
      editor->options = g_value_dup_object (value);
      break;

    case PROP_EDIT_CONTEXT:
      editor->edit_context = g_value_get_boolean (value);
      break;

    case PROP_USE_CUSTOM_STYLE:
      editor->use_custom_style = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_fill_editor_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GimpFillEditor *editor = GIMP_FILL_EDITOR (object);

  switch (property_id)
    {
    case PROP_OPTIONS:
      g_value_set_object (value, editor->options);
      break;

    case PROP_EDIT_CONTEXT:
      g_value_set_boolean (value, editor->edit_context);
      break;

    case PROP_USE_CUSTOM_STYLE:
      g_value_set_boolean (value, editor->use_custom_style);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_fill_editor_switcher_notify (GObject    *object,
                                  GParamSpec *pspec,
                                  gpointer    data)
{
  GimpFillEditor *editor = GIMP_FILL_EDITOR (data);
  const gchar    *name   = gtk_stack_get_visible_child_name (GTK_STACK (object));

  if (g_strcmp0 (name, "color-fg") == 0)
    g_object_set (editor->options,
                  editor->use_custom_style ? "custom-style" : "style",
                  (editor->use_custom_style ?
                   GIMP_CUSTOM_STYLE_SOLID_COLOR :
                   GIMP_FILL_STYLE_FG_COLOR),
                  NULL);
  else if (g_strcmp0 (name, "color-bg") == 0)
    g_object_set (editor->options,
                  editor->use_custom_style ? "custom-style" : "style",
                  (editor->use_custom_style ?
                   GIMP_CUSTOM_STYLE_SOLID_COLOR :
                   GIMP_FILL_STYLE_BG_COLOR),
                  NULL);
  else if (g_strcmp0 (name, "pattern") == 0)
    g_object_set (editor->options,
                  editor->use_custom_style ? "custom-style" : "style",
                  (editor->use_custom_style ?
                   GIMP_CUSTOM_STYLE_PATTERN :
                   GIMP_FILL_STYLE_PATTERN),
                  NULL);
}

GtkWidget *
gimp_fill_editor_new (GimpFillOptions *options,
                      gboolean         edit_context,
                      gboolean         use_custom_style)
{
  g_return_val_if_fail (GIMP_IS_FILL_OPTIONS (options), NULL);

  return g_object_new (GIMP_TYPE_FILL_EDITOR,
                       "options",          options,
                       "edit-context",     edit_context ? TRUE : FALSE,
                       "use-custom-style", use_custom_style ? TRUE : FALSE,
                       NULL);
}

void
gimp_fill_editor_outline_style_changed (GimpFillEditor *editor,
                                        const gchar    *style)
{
  g_return_if_fail (GIMP_IS_FILL_EDITOR (editor));

  if (gtk_stack_get_child_by_name (editor->private->stack, style))
    gtk_stack_set_visible_child_name (editor->private->stack, style);
}
