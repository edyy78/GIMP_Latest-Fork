/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimppropwidgets.c
 * Copyright (C) 2023 Jehan
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

#include "config.h"

#include "libgimp/gimpui.h"

#include "libgimp-intl.h"

/*
 * This is a complement of libgimpwidgets/gimppropwidgets.c
 * These are property functions for types from libgimp, such as
 * [class@Gimp.Resource] or [class@Gimp.Item] subclasses.
 */


typedef GtkWidget* (*GimpResourceWidgetCreator) (const gchar  *title,
                                                 const gchar  *label,
                                                 GimpResource *initial_resource);


/*  utility function prototypes  */

static GtkWidget  * gimp_prop_resource_chooser_factory   (GimpResourceWidgetCreator  widget_creator_func,
                                                          GObject                   *config,
                                                          const gchar               *property_name,
                                                          const gchar               *chooser_title);
static gchar      * gimp_utils_make_canonical_menu_label (const gchar               *path);

static void         gimp_bind_props_convert_ID_to_object (GObject                   *config,
                                                          const gchar               *config_property_name,
                                                          GtkWidget                 *prop_widget,
                                                          const gchar               *widget_property_name);


/**
 * gimp_prop_brush_chooser_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of a [class@Gimp.Brush] property.
 * @chooser_title: (nullable): title for the poppable dialog.
 *
 * Creates a [class@GimpUi.BrushChooser] controlled by the specified property.
 *
 * Returns: (transfer full): A new [class@GimpUi.BrushChooser].
 *
 * Since: 3.0
 */
GtkWidget *
gimp_prop_brush_chooser_new (GObject     *config,
                             const gchar *property_name,
                             const gchar *chooser_title)
{
  return gimp_prop_resource_chooser_factory ((GimpResourceWidgetCreator) gimp_brush_chooser_new,
                                             config, property_name, chooser_title);
}

/**
 * gimp_prop_font_chooser_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of a [class@Gimp.Font] property.
 * @chooser_title: (nullable): title for the poppable dialog.
 *
 * Creates a [class@GimpUi.FontChooser] controlled by the specified property.
 *
 * Returns: (transfer full): A new [class@GimpUi.FontChooser].
 *
 * Since: 3.0
 */
GtkWidget *
gimp_prop_font_chooser_new (GObject     *config,
                            const gchar *property_name,
                            const gchar *chooser_title)
{
  return gimp_prop_resource_chooser_factory ((GimpResourceWidgetCreator) gimp_font_chooser_new,
                                             config, property_name, chooser_title);
}

/**
 * gimp_prop_gradient_chooser_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of a [class@Gimp.Gradient] property.
 * @chooser_title: (nullable): title for the poppable dialog.
 *
 * Creates a [class@GimpUi.GradientChooser] controlled by the specified property.
 *
 * Returns: (transfer full): A new [class@GimpUi.GradientChooser].
 *
 * Since: 3.0
 */
GtkWidget *
gimp_prop_gradient_chooser_new (GObject     *config,
                                const gchar *property_name,
                                const gchar *chooser_title)
{
  return gimp_prop_resource_chooser_factory ((GimpResourceWidgetCreator) gimp_gradient_chooser_new,
                                             config, property_name, chooser_title);
}

/**
 * gimp_prop_palette_chooser_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of a [class@Gimp.Palette] property.
 * @chooser_title: (nullable): title for the poppable dialog.
 *
 * Creates a [class@GimpUi.PaletteChooser] controlled by the specified property.
 *
 * Returns: (transfer full): A new [class@GimpUi.PaletteChooser].
 *
 * Since: 3.0
 */
GtkWidget *
gimp_prop_palette_chooser_new (GObject     *config,
                               const gchar *property_name,
                               const gchar *chooser_title)
{
  return gimp_prop_resource_chooser_factory ((GimpResourceWidgetCreator) gimp_palette_chooser_new,
                                             config, property_name, chooser_title);
}

/**
 * gimp_prop_pattern_chooser_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of a [class@Gimp.Pattern] property.
 * @chooser_title: (nullable): title for the poppable dialog.
 *
 * Creates a [class@GimpUi.PatternChooser] controlled by the specified property.
 *
 * Returns: (transfer full): A new [class@GimpUi.PatternChooser].
 *
 * Since: 3.0
 */
GtkWidget *
gimp_prop_pattern_chooser_new (GObject     *config,
                               const gchar *property_name,
                               const gchar *chooser_title)
{
  return gimp_prop_resource_chooser_factory ((GimpResourceWidgetCreator) gimp_pattern_chooser_new,
                                             config, property_name, chooser_title);
}

/**
 * gimp_prop_drawable_chooser_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of a [class@Gimp.Drawable] property.
 * @chooser_title: (nullable): title for the poppable dialog.
 *
 * Creates a [class@GimpUi.DrawableChooser] controlled by the specified property.
 *
 * Returns: (transfer full): A new [class@GimpUi.DrawableChooser].
 *
 * Since: 3.0
 */
GtkWidget *
gimp_prop_drawable_chooser_new (GObject     *config,
                                const gchar *property_name,
                                const gchar *chooser_title)
{
  GParamSpec   *param_spec;
  GtkWidget    *prop_chooser;
  GimpDrawable *initial_drawable = NULL;
  gchar        *title            = NULL;
  const gchar  *label;

  param_spec = g_object_class_find_property (G_OBJECT_GET_CLASS (config),
                                             property_name);

  g_return_val_if_fail (param_spec != NULL, NULL);
  g_return_val_if_fail (g_type_is_a (G_TYPE_FROM_INSTANCE (param_spec), G_TYPE_PARAM_OBJECT) &&
                        g_type_is_a (param_spec->value_type, GIMP_TYPE_DRAWABLE), NULL);

  g_object_get (config,
                property_name, &initial_drawable,
                NULL);

  label = g_param_spec_get_nick (param_spec);

  if (chooser_title == NULL)
    {
      gchar *canonical;

      canonical = gimp_utils_make_canonical_menu_label (label);
      if (g_type_is_a (param_spec->value_type, GIMP_TYPE_LAYER))
        title = g_strdup_printf (_("Choose layer: %s"), canonical);
      if (g_type_is_a (param_spec->value_type, GIMP_TYPE_CHANNEL))
        title = g_strdup_printf (_("Choose channel: %s"), canonical);
      else
        title = g_strdup_printf (_("Choose drawable: %s"), canonical);
      g_free (canonical);
    }
  else
    {
      title = g_strdup (chooser_title);
    }

  prop_chooser = gimp_drawable_chooser_new (title, label, param_spec->value_type, initial_drawable);
  g_clear_object (&initial_drawable);
  g_free (title);

  g_object_bind_property (prop_chooser, "drawable",
                          config,       property_name,
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  return prop_chooser;
}

/* A special function to hide complexity, but not generally useful.
 * Not needed if GimpImageComboBox behaved differently.
 *
 * The binding of properties initializes the widget from the config.
 * When the config is NULL, and exists no open images, the widget shows "(None)"
 * When the config is NULL, and exists an open image,
 * the widget will show an arbitrary active image that the user has not selected,
 * but doesn't set its property (thus no update config) and doesn't emit "changed".
 * So initialize the config from the widget.
 * Must be called after the widget has created its model.
 */
static void
init_config_property_from_image_ID_widget (GObject          *config,
                                           const gchar      *config_property_name,
                                           GimpIntComboBox  *prop_widget)
{
  gint image_ID;

  /* Call super of GimpImageComboBox. */
  gimp_int_combo_box_get_active (prop_widget, &image_ID);
  g_object_set (config,
                config_property_name, gimp_image_get_by_id (image_ID),
                NULL);
}

/**
 * gimp_prop_image_combo_box_new:
 * @config:        Object to which property is attached.
 * @property_name: Name of a property of @config, of type [class@Gimp.Image]
 *
 * Creates a [class@GimpUi.GimpImageComboBox] controlled by the @config property.
 * Decorates the widget to have trait PropWidget.
 *
 * Returns: (transfer full): A new [class@GimpUi.GimpImageComboBox].
 *
 * Since: 3.0
 */

/* FIXME: Initial value, from config to widget.
 *
 * It is not clear that we should let an OtherImage argument have a default.
 * 1. An image is not currently serializable, to the Config/Settings.
 * 2. There is no way to sensitize a generic Procedure by whether some image is open.
 * 3. ImageComboBox doesn't select "(None)" when there is an open image.
 *
 * It is not clear that binding properties will initialize the prop widget
 * and make the default active.
 * E.G. when the default is NULL, the prop widget seems to select
 * some arbitrary open image,
 * and doesn't show (None) when at least one image is open.
 */

/* FIXME: Reset
 *
 * Reset the config (e.g. in the dialog, "Reset>Initial value") does not work.
 * Probably because Image is not now serializable.
 * A serialization by the name of the image might work within the GIMP session.
 * We could assume that any OtherImage argument was read only,
 * and not worry that a user would unintentionally destroy images.
 */

/* FIXME: synced with GIMP app.
 *
 * A GimpImageComboBox is on the libgimp side, and asks the GIMP app once
 * for the model/store of open images.
 * The model can go stale, when a user opens/closes images in GIMP app.
 * This is unlike Resource Chooser widgets, which are on the core side
 * and are synced with user changes to resources.
 */

GtkWidget *
gimp_prop_image_combo_box_new (GObject     *config,
                               const gchar *property_name)
{
  GParamSpec   *param_spec;
  GtkWidget    *prop_widget;

  param_spec = g_object_class_find_property (G_OBJECT_GET_CLASS (config),
                                             property_name);

  g_return_val_if_fail (param_spec != NULL, NULL);
  g_return_val_if_fail (g_type_is_a (G_TYPE_FROM_INSTANCE (param_spec), G_TYPE_PARAM_OBJECT) &&
                        g_type_is_a (param_spec->value_type, GIMP_TYPE_IMAGE), NULL);

  /* Not filter images in the model. */
  prop_widget = gimp_image_combo_box_new (NULL, NULL, NULL);

  /* Decorate with trait PropWidget by binding properties with conversions of types.
   * The target property is named "value", is type Int, and on GimpIntComboBox.
   * The property is an image ID.
   */
  gimp_bind_props_convert_ID_to_object (config,      property_name,
                                        prop_widget, "value");

  /* The binding inits the widget property.
   * But the widget may then set an arbitrary image active.
   * Ensure the config is init from that arbitrary choice.
   */
  init_config_property_from_image_ID_widget (config, property_name,
                                             (GimpIntComboBox*) prop_widget);

  return prop_widget;
}

/*******************************/
/*  private utility functions  */
/*******************************/

static GtkWidget *
gimp_prop_resource_chooser_factory (GimpResourceWidgetCreator  widget_creator_func,
                                    GObject                   *config,
                                    const gchar               *property_name,
                                    const gchar               *chooser_title)
{
  GParamSpec   *param_spec;
  GtkWidget    *prop_chooser;
  GimpResource *initial_resource;
  const gchar  *label;

  param_spec = g_object_class_find_property (G_OBJECT_GET_CLASS (config),
                                             property_name);

  g_return_val_if_fail (param_spec != NULL, NULL);
  g_return_val_if_fail (g_type_is_a (G_TYPE_FROM_INSTANCE (param_spec), G_TYPE_PARAM_OBJECT) &&
                        g_type_is_a (param_spec->value_type, GIMP_TYPE_RESOURCE), NULL);

  g_object_get (config,
                property_name, &initial_resource,
                NULL);

  label = g_param_spec_get_nick (param_spec);

  /* Create the wrapped widget. For example, call gimp_font_chooser_new.
   * When initial_resource is NULL, the widget creator will set it's resource
   * property from context.
   */
  prop_chooser = widget_creator_func (chooser_title, label, initial_resource);
  g_clear_object (&initial_resource);

  g_object_bind_property (prop_chooser, "resource",
                          config,       property_name,
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  return prop_chooser;
}

/* This is a copy of the similarly-named function in app/widgets/gimpwidgets-utils.c
 * I hesitated to put this maybe in libgimpwidgets/gimpwidgetsutils.h but for
 * now, let's not. If it's useful to more people, it's always easier to move the
 * function in rather than deprecating it.
 */
static gchar *
gimp_utils_make_canonical_menu_label (const gchar *path)
{
  gchar **split_path;
  gchar  *canon_path;

  /* The first underscore of each path item is a mnemonic. */
  split_path = g_strsplit (path, "_", 2);
  canon_path = g_strjoinv ("", split_path);
  g_strfreev (split_path);

  return canon_path;
}


/* GValue transformation functions.
 * Type is GBindingTransformFunc.
 * Transforms GValues.  Side effect on the to_value.
 * The transformed to_value may be None value: NULL for object*, or 0 for int ID.
 * Functions never return FALSE.
 */

static gboolean
transform_image_ID_to_object (GBinding*     binding,  /* not used. */
                              const GValue *from_value,
                              GValue       *to_value,
                              gpointer      user_data) /* not used. */
{
  /* Require to_value already holds GIMP_TYPE_IMAGE. */
  g_value_set_object (to_value,
                      (GObject*) gimp_image_get_by_id (
                        g_value_get_int (from_value)));

  return TRUE;
}

static gboolean
transform_image_object_to_ID (GBinding*     binding,  /* not used. */
                              const GValue *from_value,
                              GValue       *to_value,
                              gpointer      user_data) /* not used. */
{
  /* Require to_value already holds G_TYPE_INT */
  g_value_set_int (to_value,
                   gimp_image_get_id (g_value_get_object (from_value)));
  return TRUE;
}


/* Bind two properties bidirectionally with conversion.
 * First  property belongs to Config and is type GObject* / GimpImage*
 * Second property belongs to Widget and is type Int i.e. an ID
 *
 * Order is important because on create, initializes second from first.
 */
static void
gimp_bind_props_convert_ID_to_object (GObject     *config,
                                      const gchar *config_property_name,
                                      GtkWidget   *prop_widget,
                                      const gchar *widget_property_name)
{
  g_object_bind_property_full (
    config,       config_property_name,
    prop_widget,  widget_property_name,
    G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
    transform_image_object_to_ID,
    transform_image_ID_to_object,
    NULL,   /* user_data */
    NULL);  /* GDestroyNotify */
}
