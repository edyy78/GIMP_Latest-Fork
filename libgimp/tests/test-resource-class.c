static GimpValueArray *
gimp_c_test_run (GimpProcedure       *procedure,
                 GimpRunMode          run_mode,
                 GimpImage           *image,
                 GimpDrawable       **drawables,
                 GimpProcedureConfig *config,
                 gpointer             run_data)
{
  GimpBrush    *brush;
  GimpBrush    *brush_new;
  GimpResource *brush_copy;
  GimpFont     *font;
  GimpGradient *gradient;
  GimpGradient *gradient_new;
  GimpResource *gradient_copy;
  GimpPalette  *palette;
  GimpPalette  *palette_new;
  GimpResource *palette_copy;
  GimpPattern  *pattern;
  gboolean      success;
  gboolean      brush_renamed;
  gboolean      gradient_renamed;
  gboolean      palette_renamed;
  gchar        *brush_name;
  gchar        *font_name;
  gchar        *gradient_name;
  gchar        *palette_name;
  gchar        *pattern_name;

  brush    = gimp_context_get_brush ();
  font     = gimp_context_get_font ();
  gradient = gimp_context_get_gradient ();
  palette  = gimp_context_get_palette ();
  pattern  = gimp_context_get_pattern ();

  GIMP_TEST_START ("Verify valid brush");
  GIMP_TEST_END (gimp_resource_is_valid ((GimpResource *) brush));
  GIMP_TEST_START ("Verify valid font");
  GIMP_TEST_END (gimp_resource_is_valid ((GimpResource *) font));
  GIMP_TEST_START ("Verify valid gradient");
  GIMP_TEST_END (gimp_resource_is_valid ((GimpResource *) gradient));
  GIMP_TEST_START ("Verify valid palette");
  GIMP_TEST_END (gimp_resource_is_valid ((GimpResource *) palette));
  GIMP_TEST_START ("Verify valid pattern");
  GIMP_TEST_END (gimp_resource_is_valid ((GimpResource *) pattern));

  GIMP_TEST_START ("Verify default brush");
  brush_name = gimp_resource_get_name ((GimpResource *) brush);
  GIMP_TEST_END (g_strcmp0 (brush_name, "Clipboard Image") == 0);

  GIMP_TEST_START ("Verify default font");
  font_name = gimp_resource_get_name ((GimpResource *) font);
  GIMP_TEST_END (g_strcmp0 (font_name, "Sans-serif") == 0);

  GIMP_TEST_START ("Verify default gradient");
  gradient_name = gimp_resource_get_name ((GimpResource *) gradient);
  GIMP_TEST_END (g_strcmp0 (gradient_name, "FG to BG (RGB)") == 0);

  GIMP_TEST_START ("Verify default palette");
  palette_name = gimp_resource_get_name ((GimpResource *) palette);
  GIMP_TEST_END (g_strcmp0 (palette_name, "Color History") == 0);

  GIMP_TEST_START ("Verify default pattern");
  pattern_name = gimp_resource_get_name ((GimpResource *) pattern);
  GIMP_TEST_END (g_strcmp0 (pattern_name, "Clipboard Image") == 0);

  GIMP_TEST_START ("Verify set_brush");
  GIMP_TEST_END (gimp_context_set_brush (brush));
  GIMP_TEST_START ("Verify set_font");
  GIMP_TEST_END (gimp_context_set_font (font));
  GIMP_TEST_START ("Verify set_gradient");
  GIMP_TEST_END (gimp_context_set_gradient (gradient));
  GIMP_TEST_START ("Verify set_palette");
  GIMP_TEST_END (gimp_context_set_palette (palette));
  GIMP_TEST_START ("Verify set_pattern");
  GIMP_TEST_END (gimp_context_set_pattern (pattern));

  GIMP_TEST_START ("Verify new brush validity");
  brush_new = gimp_brush_new ("New Brush");
  GIMP_TEST_END (gimp_resource_is_valid ((GimpResource *) brush_new));
  GIMP_TEST_START ("Verify name of new brush");
  brush_name = gimp_resource_get_name ((GimpResource *) brush_new);
  GIMP_TEST_END (g_strcmp0 (brush_name, "New Brush") == 0);

  GIMP_TEST_START ("Verify new gradient validity");
  gradient_new = gimp_gradient_new ("New Gradient");
  GIMP_TEST_END (gimp_resource_is_valid ((GimpResource *) gradient_new));
  GIMP_TEST_START ("Verify new gradient name");
  gradient_name = gimp_resource_get_name ((GimpResource *) gradient_new);
  GIMP_TEST_END (g_strcmp0 (gradient_name, "New Gradient") == 0);

  GIMP_TEST_START ("Verify new palette validity");
  palette_new = gimp_palette_new ("New Palette");
  GIMP_TEST_END (gimp_resource_is_valid ((GimpResource *) palette_new));
  GIMP_TEST_START ("Verify new palette name");
  palette_name = gimp_resource_get_name ((GimpResource *) palette_new);
  GIMP_TEST_END (g_strcmp0 (palette_name, "New Palette") == 0);

  GIMP_TEST_START ("Verify deletion of brush");
  GIMP_TEST_END (gimp_resource_delete ((GimpResource *) brush_new));
  GIMP_TEST_START ("Verify brush invalidity after deletion");
  GIMP_TEST_END (! gimp_resource_is_valid ((GimpResource *) brush_new));

  GIMP_TEST_START ("Verify deletion of gradient");
  GIMP_TEST_END (gimp_resource_delete ((GimpResource *) gradient_new));
  GIMP_TEST_START ("Verify gradient invalidity after deletion");
  GIMP_TEST_END (! gimp_resource_is_valid ((GimpResource *) gradient_new));

  GIMP_TEST_START ("Verify deletion of palette");
  GIMP_TEST_END (gimp_resource_delete ((GimpResource *) palette_new));
  GIMP_TEST_START ("Verify palette invalidity after deletion");
  GIMP_TEST_END (! gimp_resource_is_valid ((GimpResource *) palette_new));

  GIMP_TEST_START ("Verify duplicate brush");
  brush_copy = gimp_resource_duplicate ((GimpResource *) brush);
  GIMP_TEST_END (gimp_resource_is_valid (brush_copy));
  GIMP_TEST_START ("Verify duplicate brush name");
  brush_name = gimp_resource_get_name (brush_copy);
  GIMP_TEST_END (g_strcmp0 (brush_name, "Clipboard Image copy") == 0);

  GIMP_TEST_START ("Verify duplicate gradient");
  gradient_copy = gimp_resource_duplicate ((GimpResource *) gradient);
  GIMP_TEST_END (gimp_resource_is_valid (gradient_copy));
  GIMP_TEST_START ("Verify duplicate gradient name");
  gradient_name = gimp_resource_get_name (gradient_copy);
  GIMP_TEST_END (g_strcmp0 (gradient_name, "FG to BG (RGB) copy") == 0);

  GIMP_TEST_START ("Verify duplicate palette");
  palette_copy = gimp_resource_duplicate ((GimpResource *) palette);
  GIMP_TEST_END (gimp_resource_is_valid (palette_copy));
  GIMP_TEST_START ("Verify duplicate palette name");
  palette_name = gimp_resource_get_name (palette_copy);
  GIMP_TEST_END (g_strcmp0 (palette_name, "Color History copy") == 0);

  GIMP_TEST_START ("Verify validity of renamed brush");
  brush_renamed = gimp_resource_rename (brush_copy, "Renamed Brush");
  GIMP_TEST_END (brush_renamed);
  GIMP_TEST_START ("Verify name of renamed brush");
  brush_name = gimp_resource_get_name (brush_copy);
  GIMP_TEST_END (g_strcmp0 (brush_name, "Renamed Brush") == 0);

  GIMP_TEST_START ("Verify validity of renamed gradient");
  gradient_renamed = gimp_resource_rename (gradient_copy, "Renamed Gradient");
  GIMP_TEST_END (gradient_renamed);
  GIMP_TEST_START ("Verify name of renamed gradient");
  gradient_name = gimp_resource_get_name (gradient_copy);
  GIMP_TEST_END (g_strcmp0 (gradient_name, "Renamed Gradient") == 0);

  GIMP_TEST_START ("Verify validity of renamed palette");
  palette_renamed = gimp_resource_rename (palette_copy, "Renamed Palette");
  GIMP_TEST_END (palette_renamed);
  GIMP_TEST_START ("Verify name of renamed palette");
  palette_name = gimp_resource_get_name (palette_copy);
  GIMP_TEST_END (g_strcmp0 (palette_name, "Renamed Palette") == 0);

  GIMP_TEST_START ("Verify deletion of renamed brush");
  success = gimp_resource_delete (brush_copy);
  GIMP_TEST_END (success && ! gimp_resource_is_valid (brush_copy));

  GIMP_TEST_START ("Verify deletion of renamed gradient");
  success = gimp_resource_delete (gradient_copy);
  GIMP_TEST_END (success && ! gimp_resource_is_valid (gradient_copy));

  GIMP_TEST_START ("Verify deletion of renamed palette");
  success = gimp_resource_delete (palette_copy);
  GIMP_TEST_END (success && ! gimp_resource_is_valid (palette_copy));

  GIMP_TEST_START ("Verify invalidity of invalid resource");
  GIMP_TEST_END (! gimp_resource_id_is_valid (-1));

  GIMP_TEST_RETURN
}
