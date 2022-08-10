//#include <gmodule.h>

#include <glib-object.h>

#include "script-fu-type-module.h"

/* Originally a GTypeModule that loaded a GModule.  But why? */

struct _GimpTypeModuleEnumPrivate
{
  gchar * enum_name;
  // TODO allocate this?
  GEnumValue enum_values[2];
};


static gboolean	animal_module_load		(GTypeModule 	*type_module);
static void	animal_module_unload		(GTypeModule	*type_module);
static void	animal_module_finalize		(GObject 	*object);


G_DEFINE_TYPE(GimpTypeModuleEnum, animal_module, G_TYPE_TYPE_MODULE)


static void
animal_module_class_init (GimpTypeModuleEnumClass *klass)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  module_class->load = animal_module_load;
  module_class->unload = animal_module_unload;
  object_class->finalize = animal_module_finalize;

  // lkk this compiles but is deprecated
  g_type_class_add_private(object_class, sizeof(GimpTypeModuleEnumPrivate));
  // lkk something like this should replace it, but not working
  // G_ADD_PRIVATE (GimpTypeModuleEnum);
}

/* Init an instance. */
static void
animal_module_init (GimpTypeModuleEnum *animal_module)
{
  GimpTypeModuleEnumPrivate *priv;

  g_debug("animal_module_init");

  /* Allocate private. */
  animal_module->priv = G_TYPE_INSTANCE_GET_PRIVATE(animal_module, ANIMAL_TYPE_MODULE, GimpTypeModuleEnumPrivate);
  priv = animal_module->priv;

  // NULL private
  priv->enum_name = NULL;

  priv->enum_values[1].value = 0;
  priv->enum_values[1].value_name = NULL;
  priv->enum_values[1].value_nick = NULL;
}


/*
 * Load the type module,
 * Register dynamic types.
 */
static gboolean
animal_module_load (GTypeModule	*type_module)
{
  GimpTypeModuleEnum *animal_module = ANIMAL_MODULE(type_module);
  GimpTypeModuleEnumPrivate *priv = animal_module->priv;

  g_debug("animal_module_load");

  #ifdef lkk
  priv->module = g_module_open (priv->path,
				G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

  if (
      //!g_module_symbol (priv->module, "animal_object_module_get_id",
			// (gpointer *)&animal_module->get_id) ||
      !g_module_symbol (priv->module, "animal_object_module_init",
			(gpointer *)&animal_module->init) ||
      !g_module_symbol (priv->module, "animal_object_module_exit",
			(gpointer *)&animal_module->exit)
      // ||
      // !g_module_symbol (priv->module, "animal_object_module_create",
			//(gpointer *)&animal_module->create)
      )
    {
      g_warning ("module error %s", g_module_error());
      g_module_close (priv->module);
      return FALSE;
    }
  #endif

  // lkk why init self again?
  // It crashes here?
  // animal_module->init (type_module);

  /* Form unique name for a dynamic enum.
   * Prepend prefix to avoid clash outside.
   */
  #ifdef LKK
  GString* enum_name;
  enum_name = g_string_new("SFEnumFoo");
  enum_name = g_string_append (enum_name, priv->enum_name);
  #endif

  /* Require enum_values is null terminated array. */

  // register GEnum this GTypeModule dynamically cretes.
  GType agtype; // a TypeID

  agtype = g_type_module_register_enum (
    type_module, // GTypeModule* module,
    priv->enum_name,  // gchar *
    priv->enum_values // const GEnumValue* const_static_values
    );
  // Not use returned gtype, but it is registered.
  /* Check sanity of enum. */
  // have ID, need GType* ???  g_enum_get_value(agtype, 0);

  if (agtype == G_TYPE_INVALID)
    g_debug("Failed create dynamic enum type");
  g_debug( "Dynamic type name: %s", g_type_name(agtype));
  // g_string_free(enum_name, TRUE);

  return TRUE;

}

static void
animal_module_unload (GTypeModule *type_module)
{
  GimpTypeModuleEnum *animal_module = ANIMAL_MODULE(type_module);
  GimpTypeModuleEnumPrivate *priv;

  priv=animal_module->priv;

  // animal_module->exit ();

  // g_module_close (priv->module);

  animal_module->init = NULL;

  // animal_module->exit = NULL;
  //animal_module->get_id = NULL;
  //animal_module->create = NULL;
}

static void
animal_module_finalize (GObject *object)
{
  G_OBJECT_CLASS(animal_module_parent_class)->finalize (object);
}


/* public */


GimpTypeModuleEnum*
animal_module_new (const gchar *enum_name,
                   const gchar *first_value_name)
{
  GimpTypeModuleEnum *animal_module;

  g_debug("animal_module_new");

  /* Require enum_name is fully qualified (globally unique)
   * and meets requirements for a type name (no spaces, etc.)
   * Typically <plugin name><property name>
   */

  animal_module= g_object_new (ANIMAL_TYPE_MODULE, NULL);
  /* Init was called. */

  if (enum_name)
    {
      GimpTypeModuleEnumPrivate *priv = animal_module->priv;

      /* Init private */
      priv->enum_name = g_strdup (enum_name);

      // TODO init [0] and null the next one
      priv->enum_values[0].value_name = g_strdup (first_value_name);
      priv->enum_values[0].value_nick = g_strdup (first_value_name);
      priv->enum_values[0].value = 1;
      // assert [1] is still NULL
    }

  return animal_module;
}
