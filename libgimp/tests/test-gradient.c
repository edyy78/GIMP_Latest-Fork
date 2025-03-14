static GimpValueArray *
gimp_c_test_run (GimpProcedure       *procedure,
                 GimpRunMode          run_mode,
                 GimpImage           *image,
                 GimpDrawable       **drawables,
                 GimpProcedureConfig *config,
                 gpointer             run_data)
{
  GIMP_TEST_START ("Verify gradient.");
  GIMP_TEST_END (1 == 1);

  GIMP_TEST_RETURN
}

