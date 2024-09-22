

/* Code that includes this also must avoid a warning from gimppluginmanager-call.h */
#define __YES_I_NEED_GIMP_PLUG_IN_MANAGER_CALL__

GFile   *gimp_install_plug_in_file (GFile *plug_in_source_file);
gboolean gimp_remove_plug_in_file  (GFile *plug_in_source_file);