#ifndef __ANIMAL_MODULE_H__
#define __ANIMAL_MODULE_H__

#include <glib-object.h>

//#include "animalobject.h"


#define ANIMAL_TYPE_MODULE                (animal_module_get_type ())
#define ANIMAL_MODULE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), ANIMAL_TYPE_MODULE, AnimalModule))
#define ANIMAL_MODULE_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), ANIMAL_TYPE_MODULE, AnimalModuleClass))
#define ANIMAL_IS_MODULE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ANIMAL_TYPE_MODULE))
#define ANIMAL_IS_MODULE_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), ANIMAL_TYPE_MODULE))
#define ANIMAL_MODULE_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), ANIMAL_TYPE_MODULE, AnimalModuleClass))

typedef struct _AnimalModule AnimalModule;
typedef struct _AnimalModulePrivate AnimalModulePrivate;
typedef struct _AnimalModuleClass AnimalModuleClass;


struct _AnimalModule
{
  GTypeModule parent_instance;

  AnimalModulePrivate *priv;

  const gchar*  (*get_id)        (void);
  void          (*init)          (GTypeModule *type_module);
  void          (*exit)          (void);
  // AnimalObject* (*create)        (void);
};

struct _AnimalModuleClass
{
  GTypeModuleClass parent_class;
};

GType                animal_module_get_type          (void);
AnimalModule*        animal_module_new               (const gchar *path);

#endif
