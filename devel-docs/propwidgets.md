# Developer Guide to PropWidgets

PropWidget is short for Property Widget.

## Why PropWidgets exist

PropWidgets in libgimp (in plugins) are used primarily
with GimpProcedureDialog and GimpProcedureConfig.

A PropWidget syncs values from widgets in a GimpProcedureDialog
with properties in a GimpProcedureConfig.
A GimpProcedureConfig is a serialized settings
(named, saveable/loadable, and persistent.)
A GimpProcedureDialog is a dialog for a plugin.

The syncing is bidirectional:

1.  user choices in the dialog update the settings
2.  user changes to the settings (via a *Reset* or *Load Settings* button) update the dialog

Ultimately, the user chooses OK, a plugin is run,
and the settings are saved for use in a next session of the dialog,
even in a new session of GIMP app.

## PropWidget is a trait

PropWidget is a trait, not a class.
It is a trait on instances of class Widget.

You instantiate a PropWidget by a constructor function
that decorates and returns an instance of Widget.

For example,
```
GimpIntComboBox* gimp_prop_int_combo_box_new (GimpConfig *, gchar* property_name, ...)
```
returns a decorated instance
of GimpIntComboBox.
The decoration comprises a mechanism for syncing the value of the GimpIntComboBox
to the named property of the GimpConfig.

## Mechanisms of decoration

There are several mechanisms to decorate a widget
so it has trait PropWidget:

1.  simple bi-directional property binding using GObject method g_bind_property().
When the widget has a property of the same type as the Config,
and the property setter of the widget correctly updates the view of the widget.

2. bi-directional property binding using GObject method g_bind_property_full().
When the widget has a property of a different type than the Config.
Using GTransformFuncs to convert between the types.

3. using signals and callbacks.
When the widget has no property but has signals and setters.
For example, in libgimp gimp_prop_file_chooser_new()

4. using an intermediate container widget.
*This mechanism is not used in GIMP.*
When the widget has no properties but has signals and setters.
The intermediate container widget has a property
that binds to the value of the contained widget,
using signals and callbacks.
And the decoration in turn binds the config to the property of the intermediate widget, using property binding.

## Labels and PropWidgets

Most widgets have descriptive labels that tell the purpose of the chosen value, i.e. how it will be used in some algorithm.

Especially in GimpProcedureDialog,
the labels are usually to the left of the widget.
The labels are in a left column and the widgets in a right column.
(An exception is ResourceChooserButton.)

Some PropWidgets already have separated labels (e.g. to the left)  in the decorated widget.
When you put the widget in GimpProcedureDialog,
you ask the decorated widget for its label,
and layout the label in a size group of the dialog.

Other PropWidgets don't have labels at all.
In this case, you can use LabeledPropWidget.
LabeledPropWidget is a container Widget
that contains a Label and an inner PropWidget.
(This is a different kind of decoration,
a visual decoration with a label.)
The LabeledPropWidget itself has no properties,
and does not have trait PropWidget.
The inner PropWidget has properties and the trait PropWidget.
So the LabeledPropWidget syncs with a Config,
but only via its contained widget.

```
GimpLabeledPropWidget *gimp_labeled_prop_widget_new (
                                         gchar     *label_text,
                                         GtkWidget *inner_widget)
```
The inner_widget should already be decorated with trait PropWidget.
Returns an instance of GimpLabeledPropWidget.
From which you can get the contained label widget or inner widget.
Typically you get the contained label and layout into a size group
of a GimpProcedureDialog.
You might also get the contained inner widget and connect its signals
to affect other widgets in the GimpProcedureDialog,
for example for a widget choosing Units for other widgets.

PropWidgets for ResourceChooserButton have labels, but embedded
in the widget, and not separated.

## Location of the source

Both core/app and libgimp have PropWidgets.

```
/app/widgets/gimppropwidgets.c
/libgimpwidgets/gimpropwidgets.c   PropWidgets on primitive types
/libgimp/gimppropwidgets.c         PropWidgets on GIMP types
```