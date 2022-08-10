#ifndef __GIMP_TYPE_MODULE_ENUM_H__
#define __GIMP_TYPE_MODULE_ENUM_H__

// TODO should be outside
// #include <glib-object.h>


#define GIMP_TYPE_TYPE_MODULE_ENUM                (gimp_type_module_enum_get_type ())
#define GIMP_TYPE_MODULE_ENUM(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_TYPE_MODULE_ENUM, GimpTypeModuleEnum))
#define GIMP_TYPE_MODULE_ENUM_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass),  GIMP_TYPE_TYPE_MODULE_ENUM, GimpTypeModuleEnumClass))
#define GIMP_IS_TYPE_MODULE_ENUM(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_TYPE_MODULE_ENUM))
#define GIMP_IS_TYPE_MODULE_ENUM_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIMP_TYPE_TYPE_MODULE_ENUM))
#define GIMP_TYPE_MODULE_ENUM_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIMP_TYPE_TYPE_MODULE_ENUM, GimpTypeModuleEnumClass))

typedef struct _GimpTypeModuleEnum      GimpTypeModuleEnum;
typedef struct _GimpTypeModuleEnumClass GimpTypeModuleEnumClass;


struct _GimpTypeModuleEnum
{
  GTypeModule                parent_instance;

  /* Instance members, informally private. */
  gchar      *enum_name;
  GEnumValue  enum_values[2];
};

struct _GimpTypeModuleEnumClass
{
  GTypeModuleClass parent_class;
};

GType                gimp_type_module_enum_get_type (void);
GimpTypeModuleEnum  *gimp_type_module_enum_new      (const gchar *enum_name,
                                                     const gchar *first_value_name);  // TODO string array

#endif  /* __GIMP_TYPE_MODULE_ENUM_H__ */
