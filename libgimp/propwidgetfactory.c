
#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

#include "libgimpwidgets/gimpwidgets.h"
#include "propwidgetfactory.h"
// #include "libgimp/gimpimagecombobox.h"

/* gimp_widget_set_bound_property is not public? */
#include "libgimpwidgets/gimpwidgets-private.h"


/* A widget that lets a user choose GIMP objects.
 * And updates a property to that choice.
 *
 * The store are names associated with int ID's.
 * Thus the [view, model] values are [names, ints]
 *
 * The super model is the set of GIMP object (pointers) in the current GIMP context.
 * The widget edits a property of a config, a property having a GIMP object type.
 * The super model holds object pointers.
 * The widget converts from int ID to object pointer when updating the property.

 TODO resolve view/model property confusion

 TODO inconsistent when the model holds on object that the widget doesn't.

 When the super model is empty (no object exists),
 the widgets [view, model] is ["None selected", 0]
 We allow that case and set the super model (property) to NULL.
 E.G. a widget for a GimpVectors object where none exist.
 In most cases, an Image, Layer, etc. will exist,
 else the plugin that is calling this would not be enabled.

 This does not allow the user to choose "None selected."
 That is, the view never includes both valid object names and the choice "None selected."
 TODO if the GParamSpec has "NoneValid", offer the choice.

 Generic on certain GIMP object having this trait:
   having ID's and conversion functions
   having an int_combo_box_widget
   having a defined, specialized GType
   having a GParamSpecObject whose value type is the object's GType

 Parameterized by first class functions passed in:
  specialized int combo box creator func e.g. gimp_image_combo_box_new
  conversion function e.g. gimp_image_get_by_ID
 Require the conversion functions take/return same type as the property holds.
 E.G. gimp_image_combo_box_new, gimp_image_get_id, gimp_image_get_by_id

 Note we don't here understand the sub types.
 For all subtypes of int combo box we use GimpIntComboBox methods.
 For all subtyps of GimpObject (having the trait),
 we pass generic conversion functions, taking/returning gpointer.
 The compiler does not help find errors in calls to this,
 but runtime checks in GIMP will find errors.
 */

/*
 * A PropertyID is a full reference to a property.
 * A tuple: [Object, PropertyName]
 * When we set_bound_property, we set both the object pointer and name pointer.
 */


static void   gimp_prop_widget_changed_callback (GtkWidget  *widget,
                                                 FuncIDToObject func_id_to_ob);




GtkWidget *
gimp_prop_widget_factory (ComboBoxWidgetCreator func,
                          GObject     *config,
                          const gchar *property_name,
                          const gchar *label,
                          FuncIDToObject func_id_to_obj,
                          FuncObjectToID func_obj_to_id)
{
  GtkWidget *widget;
  gpointer *property_value;
  gint model_value;

  g_debug ("gimp_prop_widget_factory.");

  //widget = gimp_image_combo_box_new (NULL, NULL, NULL);
  widget = func (NULL, NULL, NULL);

  // TODO extract to local function
  /* Sync the view and model.
   * The model can be NULL and a view active.
   * For the use case of an initial call to a PDB procedure,
   * the config is the defaults (not previous settings)
   * and the default is often NULL (-1 in some bound languages)
   * meaning "user has not chosen yet.
   * If the view has any active, put it in the model
   * (so if the user chooses OK, the model is correct for the view.)
   */
  g_object_get (config,
                property_name, &property_value,
                NULL);
  if (property_value == NULL)
    {
      g_debug ("setting model.");
      gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (widget), &model_value);
      g_debug ("setting model, model_value: %d.", model_value);

      property_value = func_id_to_obj (model_value);
      if (property_value == NULL) g_debug ("Called back, NULL object");  // TODO warning
      g_object_set (config, property_name, property_value, NULL);
    }
  else
    {
      g_debug ("setting view.");
      // gimp_image_get_id
      model_value = func_obj_to_id (property_value);
      // g_debug ("image name: %s", gimp_image_get_name(property_value));
      gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX (widget), model_value);
      /* If canceled, config is discarded ? */
    }


  /* The widget is int valued but the property is object valued.
   * Thus we can't use g_object_bind_property
   * Instead "bind" to the property using a callback that sets the property,
   * after converting from int value to the property type.
   */
  g_signal_connect (widget, "changed",
                   G_CALLBACK (gimp_prop_widget_changed_callback),
                   func_id_to_obj);

  /* Unlike some other prop widgets, we don't notify of property changes.
   * The binding is one way from widget to property.
   * No other party changes the property while dialog open
   */

  gtk_widget_show (widget);

  /* Unlike other prop widgets, we don't set_param_spec,
   * but only set_bound_property.  That is simpler.
   */
  gimp_widget_set_bound_property (widget, config, property_name);

  /* Wrap in a label widget. */
  // TODO gimp_labeled or gimp_labeled_int ???
  gtk_widget_set_vexpand (widget, FALSE);
  gtk_widget_set_hexpand (widget, TRUE);
  return widget;   // gimp_label_int_widget_new (label, widget);
}



static void
gimp_prop_widget_changed_callback (GtkWidget      *widget,
                                   FuncIDToObject func_id_to_obj)
{
  gint        model_value;
  gpointer    property_value;

  const gchar   *property_name;
  GObject       *config;

  g_debug ("Called back.");

  gimp_widget_get_bound_property (widget, &config, &property_name);

  gimp_int_combo_box_get_active((GimpIntComboBox *)widget, &model_value);
  g_debug ("Called back, active: %d", model_value);

  /* convert model value and store into the property. */
  property_value = func_id_to_obj (model_value);
  if (property_value == NULL) g_debug ("Called back, NULL object");
  g_object_set (config, property_name, property_value, NULL);
}
