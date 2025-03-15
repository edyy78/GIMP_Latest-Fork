static GimpValueArray *
gimp_c_test_run (GimpProcedure       *procedure,
                 GimpRunMode          run_mode,
                 GimpImage           *image,
                 GimpDrawable       **drawables,
                 GimpProcedureConfig *config,
                 gpointer             run_data)
{
  gchar       *pattern_name;
  gboolean     success;
  GimpPattern *pattern;
  gint         height;
  gint         width;
  gint         bpp;

  pattern = gimp_context_get_pattern ();

  GIMP_TEST_START ("Verify default pattern.");
  pattern_name = gimp_resource_get_name (((GimpResource *) pattern));
  GIMP_TEST_END (g_strcmp0 (pattern_name, "Clipboard Image") == 0);

  GIMP_TEST_START ("Verify getting info.");
  success = gimp_pattern_get_info (pattern, &width, &height, &bpp);
  GIMP_TEST_END (success && width == 16 && height == 16 && bpp == 3);

  GIMP_TEST_RETURN
}
