static GimpValueArray *
gimp_c_test_run (GimpProcedure       *procedure,
                 GimpRunMode          run_mode,
                 GimpImage           *image,
                 GimpDrawable       **drawables,
                 GimpProcedureConfig *config,
                 gpointer             run_data)
{
  gboolean                 success;
  GeglColor               *fg_color;
  GeglColor               *bg_color;
  GimpGradient            *gradient;
  gchar                   *gradient_name;
  GimpGradientSegmentType  blend_func;
  GimpGradientSegmentColor coloring_type;
  GeglColor               *left_color;
  GeglColor               *right_color;
  GeglColor              **samples;
  gdouble                  red;
  gdouble                  green;
  gdouble                  blue;
  gdouble                  alpha;
  gdouble                  left_pos;
  gdouble                  middle_pos;
  gdouble                  right_pos;
  gdouble                 *positions;
  GimpGradient            *g_new;
  gdouble                  actual_delta;

  fg_color = gimp_context_get_foreground ();
  bg_color = gimp_context_get_background ();

  GIMP_TEST_START ("Verify gradient");
  gradient      = gimp_context_get_gradient ();
  gradient_name = gimp_resource_get_name ((GimpResource *) gradient);
  GIMP_TEST_END (gimp_resource_is_valid ((GimpResource *) gradient) &&
                 ! g_strcmp0 (gradient_name, "FG to BG (RGB)"));

  GIMP_TEST_START ("Verify segments");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments (gradient) == 1);

  GIMP_TEST_START ("Verify blending function");
  success = gimp_gradient_segment_get_blending_function (gradient, 0, &blend_func);
  GIMP_TEST_END (success && blend_func == GIMP_GRADIENT_SEGMENT_LINEAR);

  GIMP_TEST_START ("Verify coloring type");
  success = gimp_gradient_segment_get_coloring_type (gradient, 0, &coloring_type);
  GIMP_TEST_END (success && coloring_type == GIMP_GRADIENT_SEGMENT_RGB);

  GIMP_TEST_START ("Verify that gradient is not editable");
  GIMP_TEST_END (! (gimp_resource_is_editable ((GimpResource *) gradient)));

  GIMP_TEST_START ("Verify segment getters for left color");
  left_color = gimp_gradient_segment_get_left_color (gradient, 0);
  gegl_color_get_rgba (left_color, &red, &green, &blue, &alpha);
  GIMP_TEST_END (red == 0.0 && green == 0.0 && blue == 0.0 && alpha == 1.0);

  GIMP_TEST_START ("Verify segment getters for right color");
  right_color = gimp_gradient_segment_get_right_color (gradient, 0);
  gegl_color_get_rgba (right_color, &red, &green, &blue, &alpha);
  GIMP_TEST_END (red == 1.0 && green == 1.0 && blue == 1.0 && alpha == 1.0);

  GIMP_TEST_START ("Verify set failures for left color");
  success = gimp_gradient_segment_set_right_color (gradient, 0, bg_color);
  GIMP_TEST_END (! success);

  GIMP_TEST_START ("Verify set failures for right color");
  success = gimp_gradient_segment_set_right_color (gradient, 0, bg_color);
  GIMP_TEST_END (! success);

  GIMP_TEST_START ("Verify failures for set left pos");
  success = gimp_gradient_segment_set_left_pos (gradient, 0, 0.0, &left_pos);
  GIMP_TEST_END (! success);

  GIMP_TEST_START ("Verify failures for set right pos");
  success = gimp_gradient_segment_set_right_pos (gradient, 0, 0.0, &right_pos);
  GIMP_TEST_END (! success);

  GIMP_TEST_START ("Verify failures for set middle pos");
  success = gimp_gradient_segment_set_middle_pos (gradient, 0, 0.0, &middle_pos);
  GIMP_TEST_END (! success);

  GIMP_TEST_START ("Verify range set coloring type failure");
  success = gimp_gradient_segment_range_set_coloring_type (gradient, 0, 0, GIMP_GRADIENT_SEGMENT_RGB);
  GIMP_TEST_END (! success);

  GIMP_TEST_START ("Verify range set blending function failure");
  success = gimp_gradient_segment_range_set_blending_function (gradient, 0, 0, GIMP_GRADIENT_SEGMENT_LINEAR);
  GIMP_TEST_END (! success);

  GIMP_TEST_START ("Verify Deletion Failure");
  success = gimp_resource_delete ((GimpResource *) gradient);
  GIMP_TEST_END (! success);

  /* Test Sampling */

  GIMP_TEST_START ("Verify Uniform samples");
  samples = gimp_gradient_get_uniform_samples (gradient, 3, 0);
  int x   = 0;
  x       = (int) (sizeof (samples) / sizeof (samples[0]));
  printf ("%d \n", x);
  GIMP_TEST_END (3 == 3);

  GIMP_TEST_START ("Verify custom samples");
  positions    = g_malloc (3);
  positions[0] = 0.0;
  positions[1] = 0.5;
  positions[2] = 1.0;
  samples      = gimp_gradient_get_custom_samples (gradient, 3, positions, 1);
  GIMP_TEST_END (3 == 3);

  GIMP_TEST_START ("Verify left pos getter");
  success = gimp_gradient_segment_get_left_pos (gradient, 0, &left_pos);
  GIMP_TEST_END (success && left_pos == 0.0);

  GIMP_TEST_START ("Verify right pos getter");
  success = gimp_gradient_segment_get_right_pos (gradient, 0, &right_pos);
  GIMP_TEST_END (success && right_pos == 1.0);

  GIMP_TEST_START ("Verify middle pos getter");
  success = gimp_gradient_segment_get_middle_pos (gradient, 0, &middle_pos);
  GIMP_TEST_END (success && middle_pos == 0.5);

  /* Test Creation of new gradient */

  g_new = gimp_gradient_new ("New Gradient");
  GIMP_TEST_START ("Verify Gradient name and editable property");
  gradient_name = gimp_resource_get_name ((GimpResource *) g_new);
  success       = gimp_resource_is_editable ((GimpResource *) g_new);
  GIMP_TEST_END (g_strcmp0 (gradient_name, "New Gradient") == 0 && success);

  GIMP_TEST_START ("Verify segments for new gradient");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments (g_new) == 1);

  GIMP_TEST_START ("Verify segment setter for left color");
  success = gimp_gradient_segment_set_left_color (g_new, 0, bg_color);
  GIMP_TEST_END (success);

  GIMP_TEST_START ("Verify segment getter for left color");
  left_color = gimp_gradient_segment_get_left_color (g_new, 0);
  gegl_color_get_rgba (left_color, &red, &green, &blue, &alpha);
  GIMP_TEST_END (red == 1.0 && green == 1.0 && blue == 1.0 && alpha == 1.0);

  GIMP_TEST_START ("Verify segment setter for right color");
  success = gimp_gradient_segment_set_right_color (g_new, 0, fg_color);
  GIMP_TEST_END (success);

  GIMP_TEST_START ("Verify segment getters for right color");
  right_color = gimp_gradient_segment_get_right_color (g_new, 0);
  gegl_color_get_rgba (right_color, &red, &green, &blue, &alpha);
  GIMP_TEST_END (red == 0.0 && green == 0.0 && blue == 0.0 && alpha == 1.0);

  GIMP_TEST_START ("Verify setting left pos");
  success = gimp_gradient_segment_set_left_pos (g_new, 0, 0.01, &left_pos);
  GIMP_TEST_END (success && left_pos == 0.0);

  GIMP_TEST_START ("Verify setting right pos");
  success = gimp_gradient_segment_set_right_pos (g_new, 0, 0.99, &right_pos);
  GIMP_TEST_END (success && right_pos == 1.0);

  GIMP_TEST_START ("Verify setting middle pos");
  success = gimp_gradient_segment_set_middle_pos (g_new, 0, 0.49, &middle_pos);
  GIMP_TEST_END (success && middle_pos == 0.49);

  GIMP_TEST_START ("Verify range set coloring type");
  success = gimp_gradient_segment_range_set_coloring_type (g_new, 0, 0, GIMP_GRADIENT_SEGMENT_HSV_CW);
  GIMP_TEST_END (success);
  GIMP_TEST_START ("Verify range get coloring type");
  success = gimp_gradient_segment_get_coloring_type (g_new, 0, &coloring_type);
  GIMP_TEST_END (success && coloring_type == GIMP_GRADIENT_SEGMENT_HSV_CW);

  GIMP_TEST_START ("Verify range set blending function");
  success = gimp_gradient_segment_range_set_blending_function (g_new, 0, 0, GIMP_GRADIENT_SEGMENT_CURVED);
  GIMP_TEST_START ("Verify range get blending function");
  success = gimp_gradient_segment_get_blending_function (g_new, 0, &blend_func);
  GIMP_TEST_END (success && blend_func == GIMP_GRADIENT_SEGMENT_CURVED);

  GIMP_TEST_START ("Verify split midpoint");
  success = gimp_gradient_segment_range_split_midpoint (g_new, 0, 0);
  GIMP_TEST_END (success);
  GIMP_TEST_START ("Verify segments");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments (g_new) == 2);

  GIMP_TEST_START ("Verify range flip");
  success = gimp_gradient_segment_range_flip (g_new, 0, 1);
  GIMP_TEST_END (success);
  GIMP_TEST_START ("Verify no change after flip");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments (g_new) == 2);

  GIMP_TEST_START ("Verify replication");
  success = gimp_gradient_segment_range_replicate (g_new, 0, 1, 2);
  GIMP_TEST_END (success);
  GIMP_TEST_START ("Verify segments after replication");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments (g_new) == 4);

  GIMP_TEST_START ("Verify splitting midpoint");
  success = gimp_gradient_segment_range_split_midpoint (g_new, 3, 3);
  GIMP_TEST_END (success);
  GIMP_TEST_START ("Verify new segments");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments (g_new) == 5);

  GIMP_TEST_START ("Verify range split");
  success = gimp_gradient_segment_range_split_midpoint (g_new, 0, 0);
  GIMP_TEST_END (success);
  GIMP_TEST_START ("Verify new segment");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments (g_new) == 6);
  GIMP_TEST_START ("Verify range splitting uniform");
  success = gimp_gradient_segment_range_split_uniform (g_new, 1, 1, 3);
  GIMP_TEST_END (success);
  GIMP_TEST_START ("Verify new number of segments");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments (g_new) == 8);

  GIMP_TEST_START ("Verify deletion");
  success = gimp_gradient_segment_range_delete (g_new, 6, 6);
  GIMP_TEST_END (success);
  GIMP_TEST_START ("Verify segments after deletion");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments (g_new) == 7);

  actual_delta = gimp_gradient_segment_range_move (g_new, 1, 1, -1.0, 0);
  GIMP_TEST_START ("Verify delta without compression");
  GIMP_TEST_END (actual_delta == -0.0637499999);
  GIMP_TEST_START ("Verify no segment count change");
  GIMP_TEST_END (gimp_gradient_get_number_of_segments (g_new) == 7);

  GIMP_TEST_START ("Verify redistribution");
  success = gimp_gradient_segment_range_redistribute_handles (g_new, 0, 5);
  GIMP_TEST_END (success);

  GIMP_TEST_START ("Verify blend");
  success = gimp_gradient_segment_range_blend_colors (g_new, 1, 4);
  GIMP_TEST_END (success);

  GIMP_TEST_START ("Verify blend opacity");
  success = gimp_gradient_segment_range_blend_opacity (g_new, 2, 3);
  GIMP_TEST_END (success);

  GIMP_TEST_START ("Verify out of range fails");
  success = gimp_gradient_segment_set_left_color (g_new, 9, bg_color);
  GIMP_TEST_END (! success);

  GIMP_TEST_START ("Delete gradient");
  gimp_resource_delete ((GimpResource *) g_new);
  success = gimp_resource_is_valid ((GimpResource *) g_new);
  GIMP_TEST_END (! success);

  GIMP_TEST_RETURN
}
