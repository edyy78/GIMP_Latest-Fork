
#include "config.h"

#include <glib-object.h>

#include "gimp-type-module.h"

/* A GTypeModule (See GLib docs: a dynamically loaded type)
 * Specialized to create a GEnum type.
 *
 * Dynamic means at runtime, say by an interpreted plugin.
 * At compile time of GIMP, the enum is unknown.
 *
 * The dynamic type's lifetime is the same as the GIMP app:
 * we don't unload plugins, so we don't unload their types.
 *
 * The parameters of the type (passed to the new function) are:
 *     name
 *     array of strings naming the enum's values
 *
 * The name must be globally unique, to avoid name clashes.
 *
 * The enum value names don't need to be unique, even amongst themselves.
 * The order of enum value names is important.
 * Not a parameter: the starting integer for the enum values,
 * you can't control the numbering of the enum values, they are consecutive ints.
 */

/* Overrides of virtual methods. */
static gboolean	gimp_type_module_enum_load     (GTypeModule *module);
static void	    gimp_type_module_enum_unload   (GTypeModule	*module);

static void	    gimp_type_module_enum_finalize (GObject 	  *object);


/* This defines many types and methods. */
G_DEFINE_TYPE(GimpTypeModuleEnum, gimp_type_module_enum, G_TYPE_TYPE_MODULE)


/* Class initializer method. */
static void
gimp_type_module_enum_class_init (GimpTypeModuleEnumClass *klass)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS(klass);
  GObjectClass     *object_class = G_OBJECT_CLASS(klass);

  /* Install overridden virtual methods.. */
  module_class->load = gimp_type_module_enum_load;
  module_class->unload = gimp_type_module_enum_unload;

  object_class->finalize = gimp_type_module_enum_finalize;
}

/* Instance initializer method. */
static void
gimp_type_module_enum_init (GimpTypeModuleEnum *module)
{
  g_debug("gimp_type_module_enum_init");

  /* We don't need to initialize completely.
   * This is called by _new, which goes on to set enum_name and enum_values.
   */
  module->enum_name = NULL;
}


/* Overridden virtual method.
 * Register dynamic types into GType runtime system.
 */
static gboolean
gimp_type_module_enum_load (GTypeModule	*module)
{
  /* Cast */
  GimpTypeModuleEnum *enum_module = GIMP_TYPE_MODULE_ENUM(module);
  GType               agtype; // a TypeID

  g_debug("gimp_type_module_enum_load");

  /* Form unique name for a dynamic enum.
   * Prepend prefix to avoid clash outside.
   */
  #ifdef LKK
  GString* enum_name;
  enum_name = g_string_new("SFEnumFoo");
  enum_name = g_string_append (enum_name, priv->enum_name);
  #endif

  /* Require enum_name is globally unique. */
  /* Require enum_values is null terminated array. */

  /* register in GType system the GEnum this GTypeModule dynamically creates. */
  agtype = g_type_module_register_enum (
    module,                   // GTypeModule*
    enum_module->enum_name,   // gchar *
    enum_module->enum_values  // const GEnumValue* const_static_values
    );

  /* Check sanity of enum. */
  if (agtype == G_TYPE_INVALID)
    g_debug("Failed create dynamic enum type");

  g_debug( "Dynamic type name: %s", g_type_name(agtype));
  // g_string_free(enum_name, TRUE);

  return TRUE;

}

/* Override virtual method.
 * We don't expect it to be called.
 */
static void
gimp_type_module_enum_unload (GTypeModule *module)
{
  g_warning ("Unimplemented gimp_type_module_enum_unload" );
  /* TODO free any allocated enum_values, etc. ??? */
}

static void
gimp_type_module_enum_finalize (GObject *object)
{
  G_OBJECT_CLASS(gimp_type_module_enum_parent_class)->finalize (object);
}


/* public */

GimpTypeModuleEnum*
gimp_type_module_enum_new (const gchar *enum_name,
                           const gchar *first_value_name) // TODO array
{
  GimpTypeModuleEnum *enum_module;

  g_debug("gimp_type_module_enum_new");

  /* Require enum_name is fully qualified (globally unique)
   * and meets requirements for a type name (no spaces, etc.)
   * Typically <plugin name><property name>
   */

  enum_module= g_object_new (GIMP_TYPE_TYPE_MODULE_ENUM, NULL);
  /* Initializer was called. */

  if (enum_name)
    {
      enum_module->enum_name = g_strdup (enum_name);

      // TODO an array
      enum_module->enum_values[0].value_name = g_strdup (first_value_name);
      enum_module->enum_values[0].value_nick = g_strdup (first_value_name);
      enum_module->enum_values[0].value = 1;

      enum_module->enum_values[1].value = 0;
      enum_module->enum_values[1].value_name = NULL;
      enum_module->enum_values[1].value_nick = NULL;
    }

  return enum_module;
}
