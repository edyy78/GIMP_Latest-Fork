static GimpValueArray *
gimp_c_test_run (GimpProcedure       *procedure,
                 GimpRunMode          run_mode,
                 GimpImage           *image,
                 GimpDrawable       **drawables,
                 GimpProcedureConfig *config,
                 gpointer             run_data)
{
  GimpFont *font_default;
  gchar    *font_name;

  font_default = gimp_context_get_font ();

  GIMP_TEST_START ("Verify Default font.");
  font_name = gimp_resource_get_name (((GimpResource *) font_default));
  GIMP_TEST_END (g_strcmp0 (font_name, "Sans-serif") == 0);

  GIMP_TEST_RETURN
}
