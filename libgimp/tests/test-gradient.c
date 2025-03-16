static GimpValueArray *
gimp_c_test_run (GimpProcedure       *procedure,
                 GimpRunMode          run_mode,
                 GimpImage           *image,
                 GimpDrawable       **drawables,
                 GimpProcedureConfig *config,
                 gpointer             run_data)
{
  gboolean                  success;
  GeglColor                *fg_color;
  GeglColor                *bg_color;
  GimpGradient             *gradient;
  gchar                    *gradient_name;
  GimpGradientSegmentType   blend_func;
  GimpGradientSegmentColor  coloring_type;
  GeglColor                *left_color;
  GeglColor                *right_color;
  gdouble                   red;
  gdouble                   green;
  gdouble                   blue;
  gdouble                   alpha;
  gdouble                  *left_pos;
  gdouble                  *middle_pos;
  gdouble                  *right_pos;



  fg_color = gimp_context_get_foreground ();
  bg_color = gimp_context_get_background ();

  GIMP_TEST_START ("Verify gradient");
  gradient = gimp_context_get_gradient ();
  gradient_name = gimp_resource_get_name ((GimpResource *) gradient);
  GIMP_TEST_END (gimp_resource_is_valid ((GimpResource *) gradient) &&
                 g_strcmp0 (gradient_name, "FG to BG (RGB)"));

  GIMP_TEST_START ("Verify segments");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments(gradient) == 1);

  GIMP_TEST_START ("Verify blending function");
  success = gimp_gradient_segment_get_blending_function (gradient, 0, &blend_func);
  GIMP_TEST_END (success && blend_func == GIMP_GRADIENT_SEGMENT_TYPE_LINEAR);

  GIMP_TEST_START ("Verify coloring type");
  success = gimp_gradient_segment_get_coloring_type (gradient, 0, &coloring_type);
  GIMP_TEST_END (success && coloring_type == GIMP_GRADIENT_SEGMENT_COLOR_RGB);

  GIMP_TEST_START ("Verify that gradient is not editable");
  GIMP_TEST_END (!(gimp_resource_is_editable((GimpResource *) gradient)));

  GIMP_TEST_START ("Verify segment getters for left color");
  left_color = gimp_gradient_segment_get_left_color (gradient, 0, &left_color);
  gegl_color_get_rgba (left_color, &red, &green, &blue, &alpha);
  GIMP_TESTS_END (red == 0.0 && green == 0.0 && blue == 0.0 && alpha == 1.0);

  GIMP_TEST_START ("Verify segment getters for right color");
  right_color = gimp_gradient_segment_get_right_color (gradient, 0, &right_color);
  gegl_color_get_rgba (right_color, &red, &green, &blue, &alpha);
  GIMP_TESTS_END (red == 1.0 && green == 1.0 && blue == 1.0 && alpha == 1.0);

  GIMP_TEST_START ("Verify set failures for left color");
  success = gimp_gradient_segment_set_right_color (gradient, 0, bg_color);
  GIMP_TEST_END (!success);

  GIMP_TEST_START ("Verify set failures for right color");
  success = gimp_gradient_segment_set_right_color (gradient, 0, bg_color);
  GIMP_TEST_END (!success);

  GIMP_TEST_START ("Verify failures for set left pos");
  success = gimp_gradient_segment_set_left_pos (gradient, 0, 0.0, &left_pos);
  GIMP_TEST_END (!success);

  GIMP_TEST_START ("Verify failures for set right pos");
  success = gimp_gradient_segment_set_right_pos (gradient, 0, 0.0, &right_pos);
  GIMP_TEST_END (!success);

  GIMP_TEST_START ("Verify failures for set middle pos");
  success = gimp_gradient_segment_set_middle_pos (gradient, 0, 0.0, &middle_pos);
  GIMP_TEST_END (!success);


  GIMP_TEST_RETURN
}

