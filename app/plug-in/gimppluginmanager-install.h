

void     gimp_plug_in_manager_install_plugin (GimpPlugInManager *manager,
                                              GimpContext       *context,
                                              GFile             *file);
gboolean gimp_plug_in_manager_remove_plugin  (GimpPlugInManager *manager,
                                              GFile             *file);

GFile*   gimp_plug_in_manager_get_file_by_menu_label (GimpPlugInManager *manager,
                                                      const gchar       *menu_label);

GStrv    gimp_plug_in_manager_get_user_menu_labels   (GimpPlugInManager *manager);