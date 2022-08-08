#include <gmodule.h>

#include "script-fu-type-module.h"


struct _AnimalModulePrivate
{
  // lkk TODO array of GEnumValue
  GEnumValue gevalue;
  
  // GModule *module;
  // gchar *path;
};


static gboolean	animal_module_load		(GTypeModule 	*type_module);
static void	animal_module_unload		(GTypeModule	*type_module);
static void	animal_module_finalize		(GObject 	*object);


G_DEFINE_TYPE(AnimalModule, animal_module, G_TYPE_TYPE_MODULE)


static void
animal_module_class_init (AnimalModuleClass *klass)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  module_class->load = animal_module_load;
  module_class->unload = animal_module_unload;

  object_class->finalize = animal_module_finalize;

  g_type_class_add_private(object_class, sizeof(AnimalModulePrivate));
}

static void
animal_module_init (AnimalModule *animal_module)
{
  AnimalModulePrivate *priv;

  animal_module->priv = G_TYPE_INSTANCE_GET_PRIVATE(animal_module, ANIMAL_TYPE_MODULE, AnimalModulePrivate);

  priv = animal_module->priv;

  priv->path = NULL;
  priv->module = NULL;
}


static gboolean
animal_module_load (GTypeModule	*type_module)
{
  AnimalModule *animal_module = ANIMAL_MODULE(type_module);
  AnimalModulePrivate *priv;

  priv=animal_module->priv;

  priv->module = g_module_open (priv->path,
				G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

  if (!g_module_symbol (priv->module, "animal_object_module_get_id",
			(gpointer *)&animal_module->get_id) ||
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

  animal_module->init (type_module);
  return TRUE;

}

static void
animal_module_unload (GTypeModule *type_module)
{
  AnimalModule *animal_module = ANIMAL_MODULE(type_module);
  AnimalModulePrivate *priv;

  priv=animal_module->priv;

  animal_module->exit ();

  g_module_close (priv->module);

  animal_module->get_id = NULL;
  animal_module->init = NULL;
  animal_module->exit = NULL;
  //animal_module->create = NULL;
}

static void
animal_module_finalize (GObject *object)
{
  G_OBJECT_CLASS(animal_module_parent_class)->finalize (object);
}


/* public APIs */
AnimalModule*
animal_module_new (const gchar *path)
{
  AnimalModule *animal_module;

  animal_module= g_object_new (ANIMAL_TYPE_MODULE, NULL);

  if (path)
    {
      AnimalModulePrivate *priv;

      priv=animal_module->priv;

      priv->path = g_strdup (path);
    }

  return animal_module;
}
