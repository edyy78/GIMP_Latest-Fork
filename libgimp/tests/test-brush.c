static GimpValueArray *
gimp_c_test_run (GimpProcedure       *procedure,
                 GimpRunMode          run_mode,
                 GimpImage           *image,
                 GimpDrawable       **drawables,
                 GimpProcedureConfig *config,
                 gpointer             run_data)
{
  GimpBrush              *brush_default;
  GimpBrush              *brush_new;
  gchar                  *brush_name;
  gint                    width;
  gint                    height;
  gint                    mask_bpp;
  gint                    color_bpp;
  gint                    spacing;
  gboolean                s_set;
  gboolean                s_get;
  GimpBrushGeneratedShape shape;
  GimpBrushGeneratedShape returned_shape;
  gdouble                 radius;
  gdouble                 returned_radius;
  gint                    spikes;
  gint                    returned_spikes;
  gdouble                 hardness;
  gdouble                 returned_hardness;
  gdouble                 aspect_ratio;
  gdouble                 returned_aspect_ratio;
  gdouble                 angle;
  gdouble                 returned_angle;
  gboolean                generated;
  gboolean                editable;
  gboolean                delete;

  brush_default = gimp_context_get_brush ();

  GIMP_TEST_START ("Verify default brush");
  brush_name = gimp_resource_get_name ((GimpResource *) brush_default);
  GIMP_TEST_END (g_strcmp0 (brush_name, "Clipboard Image") == 0);

  GIMP_TEST_START ("Verify properties");
  generated = gimp_brush_is_generated (brush_default);
  editable  = gimp_resource_is_editable ((GimpResource *) brush_default);
  GIMP_TEST_END (! generated && ! editable);

  GIMP_TEST_START ("Verify Info");
  gimp_brush_get_info (brush_default, &width, &height, &mask_bpp, &color_bpp);
  GIMP_TEST_END (width == 17 && height == 17 && mask_bpp == 1 && color_bpp == 0);

  GIMP_TEST_START ("Verify Spacing");
  spacing = gimp_brush_get_spacing (brush_default);
  GIMP_TEST_END (spacing == 20);

  GIMP_TEST_START ("Verify get and set fail for shape");
  s_set = gimp_brush_set_shape (brush_default, GIMP_BRUSH_GENERATED_DIAMOND, &returned_shape);
  s_get = gimp_brush_get_shape (brush_default, &shape);
  GIMP_TEST_END (! s_set && ! s_get && returned_shape == GIMP_BRUSH_GENERATED_CIRCLE);

  GIMP_TEST_START ("Verify get and set fail for radius");
  s_set = gimp_brush_set_radius (brush_default, 1.0, &returned_radius);
  s_get = gimp_brush_get_radius (brush_default, &radius);
  GIMP_TEST_END (! s_set && ! s_get);

  GIMP_TEST_START ("Verify get and set fail for spikes");
  s_set = gimp_brush_set_spikes (brush_default, 1, &returned_spikes);
  s_get = gimp_brush_get_spikes (brush_default, &spikes);
  GIMP_TEST_END (! s_set && ! s_get);

  GIMP_TEST_START ("Verify get and set fail for hardness");
  s_set = gimp_brush_set_hardness (brush_default, 1.0, &returned_hardness);
  s_get = gimp_brush_get_hardness (brush_default, &hardness);
  GIMP_TEST_END (! s_set && ! s_get);

  GIMP_TEST_START ("Verify get and set fail for aspect_ratio");
  s_set = gimp_brush_set_aspect_ratio (brush_default, 1.0, &returned_aspect_ratio);
  s_get = gimp_brush_get_aspect_ratio (brush_default, &aspect_ratio);
  GIMP_TEST_END (! s_set && ! s_get);

  GIMP_TEST_START ("Verify get and set fail for angle");
  s_set = gimp_brush_set_angle (brush_default, 90, &returned_angle);
  s_get = gimp_brush_get_angle (brush_default, &angle);
  GIMP_TEST_END (! s_set && ! s_get);

  GIMP_TEST_START ("Verify set fail for spacing");
  s_set = gimp_brush_set_spacing (brush_default, 1);
  GIMP_TEST_END (! s_set);

  /* Test a parametric and editable brush */

  brush_new = gimp_brush_new ("New Brush");

  GIMP_TEST_START ("Verify state");
  generated = gimp_brush_is_generated (brush_new);
  editable  = gimp_resource_is_editable ((GimpResource *) brush_new);
  GIMP_TEST_END (generated && editable);

  GIMP_TEST_START ("Verify get and set success for spacing");
  s_set   = gimp_brush_set_spacing (brush_new, 20);
  spacing = gimp_brush_get_spacing (brush_new);
  GIMP_TEST_END (s_set && spacing == 20);

  GIMP_TEST_START ("Verify get and set success for shape");
  s_set = gimp_brush_set_shape (brush_new, GIMP_BRUSH_GENERATED_DIAMOND, &returned_shape);
  s_get = gimp_brush_get_shape (brush_new, &shape);
  GIMP_TEST_END ((s_set && s_get) &&
                 shape == GIMP_BRUSH_GENERATED_DIAMOND &&
                 shape == returned_shape);

  GIMP_TEST_START ("Verify get and set success for radius");
  s_set = gimp_brush_set_radius (brush_new, 4.0, &returned_radius);
  s_get = gimp_brush_get_radius (brush_new, &radius);
  GIMP_TEST_END ((s_set && s_get) && radius == 4.0 && radius == returned_radius);

  GIMP_TEST_START ("Verify get and set success for hardness");
  s_set = gimp_brush_set_hardness (brush_new, 0.5, &returned_hardness);
  s_get = gimp_brush_get_hardness (brush_new, &hardness);
  GIMP_TEST_END ((s_set && s_get) && hardness == 0.5 && hardness == returned_hardness);

  GIMP_TEST_START ("Verify get and set success for spikes");
  s_set = gimp_brush_set_spikes (brush_new, 2, &returned_spikes);
  s_get = gimp_brush_get_spikes (brush_new, &spikes);
  GIMP_TEST_END ((s_set && s_get) && spikes == 2 && spikes == returned_spikes);

  GIMP_TEST_START ("Verify get and set success for aspect_ratio");
  s_set = gimp_brush_set_aspect_ratio (brush_new, 5.0, &returned_aspect_ratio);
  s_get = gimp_brush_get_aspect_ratio (brush_new, &aspect_ratio);
  GIMP_TEST_END ((s_set && s_get) && aspect_ratio == 5.0 && aspect_ratio == returned_aspect_ratio);

  GIMP_TEST_START ("Verify get and set success for angle");
  s_set = gimp_brush_set_angle (brush_new, 20, &returned_angle);
  s_get = gimp_brush_get_angle (brush_new, &angle);
  GIMP_TEST_END ((s_set && s_get) && angle == 20 && angle == returned_angle);

  /* Test the limits */

  GIMP_TEST_START ("Verify upper limits for radius");
  s_set = gimp_brush_set_radius (brush_new, 40000, &returned_radius);
  s_get = gimp_brush_get_radius (brush_new, &radius);
  GIMP_TEST_END ((s_set && s_get) && radius == 4000.0 && radius == returned_radius);

  GIMP_TEST_START ("Verify upper limits of hardness");
  s_set = gimp_brush_set_hardness (brush_new, 2.0, &returned_hardness);
  s_get = gimp_brush_get_hardness (brush_new, &hardness);
  GIMP_TEST_END ((s_set && s_get) && hardness == 1.0 && hardness == returned_hardness);

  GIMP_TEST_START ("Verify upper limits of spikes");
  s_set = gimp_brush_set_spikes (brush_new, 22, &returned_spikes);
  s_get = gimp_brush_get_spikes (brush_new, &spikes);
  GIMP_TEST_END ((s_set && s_get) && spikes == 20 && spikes == returned_spikes);

  GIMP_TEST_START ("Verify upper limits of aspect ratio");
  s_set = gimp_brush_set_aspect_ratio (brush_new, 2000.0, &returned_aspect_ratio);
  s_get = gimp_brush_get_aspect_ratio (brush_new, &aspect_ratio);
  GIMP_TEST_END ((s_set && s_get) && aspect_ratio == 1000.0 && aspect_ratio == returned_aspect_ratio);

  GIMP_TEST_START ("Verify upper limits of angle");
  s_set = gimp_brush_set_angle (brush_new, 270, &returned_angle);
  s_get = gimp_brush_get_angle (brush_new, &angle);
  GIMP_TEST_END ((s_set && s_get) && angle == 90 && angle == returned_angle);

  GIMP_TEST_START ("Verify deletion of brush");
  delete = gimp_resource_delete ((GimpResource *) brush_new);
  GIMP_TEST_END (delete);

  GIMP_TEST_RETURN
}
