
/* GIMP Wheel ColorSelector
 * Copyright (C) 2008  Michael Natterer <mitch@gimp.org>
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

#include <glib-object.h>

//#include <stdlib.h>
//#include <string.h>

//#include <gegl.h>
// This affects mostly script-fu-dynamic-enum.h  !!!!
//#include <gtk/gtk.h>

//#include "libgimpcolor/gimpcolor.h"
//#include "libgimpmath/gimpmath.h"
//#include "libgimpmodule/gimpmodule.h"

// Defines GimpColorSelector, the parent type
// More specifically gimpcolorselector.h
//#include "libgimpwidgets/gimpwidgets.h"

//#include "gimpcolorwheel.h"
//#include "script-fu-dynamic-enum.h"

//#include "libgimp/libgimp-intl.h"

/* In the GObject naming convention:
 * Prefix is COLORSEL to avoid clashes
 * Object is WHEEL
 */

#define COLORSEL_TYPE_WHEEL            (colorsel_wheel_get_type ())
#define COLORSEL_WHEEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), COLORSEL_TYPE_WHEEL, ColorselWheel))
#define COLORSEL_WHEEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), COLORSEL_TYPE_WHEEL, ColorselWheelClass))
#define COLORSEL_IS_WHEEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), COLORSEL_TYPE_WHEEL))
#define COLORSEL_IS_WHEEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), COLORSEL_TYPE_WHEEL))


typedef struct _ColorselWheel      ColorselWheel;
typedef struct _ColorselWheelClass ColorselWheelClass;

struct _ColorselWheel
{
  // GimpColorSelector  parent_instance;
  // GEnum  parent_instance;
  // G_TYPE_ENUM  parent_instance;
  // lkk an instance does not have a parent_instance?
  // is a fundamental type?

  // GtkWidget         *hsv;
};

struct _ColorselWheelClass
{
  // GimpColorSelectorClass  parent_class;
  GEnumClass parent_class;
};


static GType  colorsel_wheel_get_type      (void);

/*
#ifdef LKK
static void   colorsel_wheel_set_color     (GimpColorSelector *selector,
                                            const GimpRGB     *rgb,
                                            const GimpHSV     *hsv);
static void   colorsel_wheel_set_config    (GimpColorSelector *selector,
                                            GimpColorConfig   *config);
static void   colorsel_wheel_changed       (GimpColorWheel    *hsv,
                                            GimpColorSelector *selector);
#endif
*/

#ifdef LKK
static const GimpModuleInfo colorsel_wheel_info =
{
  GIMP_MODULE_ABI_VERSION,
  // N_("HSV color wheel"),
  "HSV color wheel",
  "Michael Natterer <mitch@gimp.org>",
  "v1.0",
  "(c) 2008, released under the GPL",
  "08 Aug 2008"
};
#endif

// G_DEFINE_DYNAMIC_TYPE (ColorselWheel, colorsel_wheel,
//                       GIMP_TYPE_COLOR_SELECTOR)
G_DEFINE_DYNAMIC_TYPE (ColorselWheel, colorsel_wheel,
                      G_TYPE_ENUM  )

#ifdef LKK
G_MODULE_EXPORT const GimpModuleInfo *
gimp_module_query (GTypeModule *module)
{
  return &colorsel_wheel_info;
}

G_MODULE_EXPORT gboolean
gimp_module_register (GTypeModule *module)
{
  // color_wheel_register_type (module);
  colorsel_wheel_register_type (module);

  return TRUE;
}
#endif

static void
colorsel_wheel_class_init (ColorselWheelClass *klass)
{
  // GimpColorSelectorClass *selector_class = GIMP_COLOR_SELECTOR_CLASS (klass);
  GEnumClass *selector_class = G_ENUM_CLASS (klass);

  // selector_class->name       = "Wheel"; // _("Wheel");
  selector_class->minimum = 1;

  //selector_class->help_id    = "gimp-colorselector-triangle";
  //selector_class->icon_name  = GIMP_ICON_COLOR_SELECTOR_TRIANGLE;
  //selector_class->set_color  = colorsel_wheel_set_color;
  //selector_class->set_config = colorsel_wheel_set_config;

  //gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "ColorselWheel");
}

static void
colorsel_wheel_class_finalize (ColorselWheelClass *klass)
{
}

static void
colorsel_wheel_init (ColorselWheel *wheel)
{
#ifdef LKK
  wheel->hsv = gimp_color_wheel_new ();
  g_object_add_weak_pointer (G_OBJECT (wheel->hsv),
                             (gpointer) &wheel->hsv);
  gtk_box_pack_start (GTK_BOX (wheel), wheel->hsv, TRUE, TRUE, 0);
  gtk_widget_show (wheel->hsv);

  g_signal_connect (wheel->hsv, "changed",
                    G_CALLBACK (colorsel_wheel_changed),
                    wheel);
#endif
}





#ifdef LKK
eliminate all the local methods

static void
colorsel_wheel_set_color (GimpColorSelector *selector,
                          const GimpRGB     *rgb,
                          const GimpHSV     *hsv)
{
#ifdef LKK
  ColorselWheel *wheel = COLORSEL_WHEEL (selector);

  gimp_color_wheel_set_color (GIMP_COLOR_WHEEL (wheel->hsv),
                              hsv->h, hsv->s, hsv->v);
#endif
}

static void
colorsel_wheel_set_config (GimpColorSelector *selector,
                           GimpColorConfig   *config)
{
#ifdef LKK
  ColorselWheel *wheel = COLORSEL_WHEEL (selector);

  if (wheel->hsv)
    gimp_color_wheel_set_color_config (GIMP_COLOR_WHEEL (wheel->hsv), config);
#endif
}

static void
colorsel_wheel_changed (GimpColorWheel    *hsv,
                        GimpColorSelector *selector)
{
#ifdef LKK
  gimp_color_wheel_get_color (hsv,
                              &selector->hsv.h,
                              &selector->hsv.s,
                              &selector->hsv.v);
  gimp_hsv_to_rgb (&selector->hsv, &selector->rgb);

  gimp_color_selector_emit_color_changed (selector);
#endif
}

#endif
