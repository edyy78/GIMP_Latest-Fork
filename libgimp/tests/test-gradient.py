#!/usr/bin/env python3

fg_color = Gimp.context_get_foreground()
bg_color = Gimp.context_get_background()

gradient = Gimp.context_get_gradient()
gimp_assert("Verify gradient", gradient.get_name() == "FG to BG (RGB)" and
            gradient.is_valid())

gimp_assert("Verify segments", gradient.get_number_of_segments() == 1)

success, blending_func = gradient.segment_get_blending_function(0)
gimp_assert("Verify blending func", success and
            blending_func == Gimp.GradientSegmentType.LINEAR)

success, coloring_type = gradient.segment_get_coloring_type(0)
gimp_assert("Verify coloring type", success and
            coloring_type == Gimp.GradientSegmentColor.RGB)

gimp_assert("Verify that gradient is not editable", not gradient.is_editable())

left_color = gradient.segment_get_left_color(0)
r, g, b, a = left_color.get_rgba()
gimp_assert("Verify segment getters for left", r == 0.0 and
            g == 0.0 and
            b == 0.0 and
            a == 1.0)

right_color = gradient.segment_get_right_color(0)
r, g, b, a = right_color.get_rgba()
gimp_assert("Verify segment getters for right", r == 1.0 and
            g == 1.0 and
            b == 1.0 and
            a == 1.0)

gimp_assert("Verify set failure for opacity", not gradient.segment_set_left_color(0, bg_color))

gimp_assert("Verify failure for set right color", not gradient.segment_set_right_color(0, bg_color))

success, position = gradient.segment_set_left_pos(0, 0.0)
gimp_assert("Verify set failure for left_pos", not success)

success, position = gradient.segment_set_right_pos(0, 0.0)
gimp_assert("Verify set failure for right_pos", not success)

success, position = gradient.segment_set_middle_pos(0, 0.0)
gimp_assert("Verify set failure for middle_pos", not success)

gimp_assert("Verify range set coloring typefailure",
            not gradient.segment_range_set_coloring_type (0, 0, Gimp.GradientSegmentColor.RGB))

gimp_assert("Verify range set failure",
            not gradient.segment_range_set_blending_function (0, 0, Gimp.GradientSegmentType.LINEAR))

gimp_assert("Verify deletion failure", not gradient.delete())

# test sampling.

samples = gradient.get_uniform_samples(3, False)
print(type(samples))
gimp_assert("Verify uniform samples", len(samples) == 3)

samples = gradient.get_custom_samples([0.0, 0.5, 1.0], True)
gimp_assert("Verify custom samples", len(samples) == 3)

success, left_pos = gradient.segment_get_left_pos(0)
gimp_assert("Verify pos getters, left", left_pos == 0.0)

success, right_pos = gradient.segment_get_right_pos(0)
gimp_assert("Verify pos getters, right", right_pos == 1.0)

success, middle_pos = gradient.segment_get_middle_pos(0)
gimp_assert("Verify pos getters, middle", middle_pos == 0.5)



gradient_new = Gimp.Gradient.new("New Gradient")
gimp_assert("Verify gradient name", gradient_new.get_name() == "New Gradient")
gimp_assert("Verify editable gradient", gradient_new.is_editable())

gimp_assert("Verify segments", gradient_new.get_number_of_segments() == 1)

gimp_assert("Verify left color set", gradient_new.segment_set_left_color(0, bg_color))
left_color = gradient_new.segment_get_left_color(0)
r, g, b, a = left_color.get_rgba()
gimp_assert("Verify left color get", r == 1.0 and
            g == 1.0 and
            b == 1.0 and
            a == 1.0)

gimp_assert("Verify right color set", gradient_new.segment_set_right_color(0,fg_color))

right_color = gradient_new.segment_get_right_color(0)
r, g, b, a = right_color.get_rgba()
gimp_assert("Verify right color get", r == 0.0 and
            g == 0.0 and
            b == 0.0 and
            a == 1.0)

success, left_pos = gradient_new.segment_set_left_pos(0, 0.01)
gimp_assert("Verify left pos", left_pos == 0.0)

success, right_pos = gradient_new.segment_set_right_pos(0, 0.99)
gimp_assert("Verify right pos", right_pos == 1.0)

success, middle_pos = gradient_new.segment_set_middle_pos(0, 0.49)
gimp_assert("Verify middle pos", middle_pos == 0.49)

success = gradient_new.segment_range_set_coloring_type(0, 0, Gimp.GradientSegmentColor.HSV_CW)
gimp_assert("Verify set coloring type", success)

success, coloring_type = gradient_new.segment_get_coloring_type(0)
gimp_assert("Verify get coloring type", success and
            coloring_type == Gimp.GradientSegmentColor.HSV_CW)

success = gradient_new.segment_range_set_blending_function(0, 0, Gimp.GradientSegmentType.CURVED)
gimp_assert("Verify set blending function", success)

success, blending_func = gradient_new.segment_get_blending_function(0)
gimp_assert("Verify set blending function", success and
            blending_func == Gimp.GradientSegmentType.CURVED)

gimp_assert("Verify split midpoint", gradient_new.segment_range_split_midpoint(0, 0))
gimp_assert("Verify segments", gradient_new.get_number_of_segments() == 2)

gimp_assert("Verify range flip", gradient_new.segment_range_flip(0, 1))
# flipping does not change count of segments
gimp_assert("Verify no change after flip", gradient_new.get_number_of_segments() == 2)

gimp_assert("Verify replication", gradient_new.segment_range_replicate(0, 1, 2))
# replicate two segments, first and second.
# replicate each into two, into four total
gimp_assert("Verify segments after replication", gradient_new.get_number_of_segments() == 4)

gimp_assert("Verify splitting midpoint", gradient_new.segment_range_split_midpoint(3, 3))
# We split last one of four, now have 5
gimp_assert("Verify new segments", gradient_new.get_number_of_segments() == 5)

gimp_assert("Verify range split", gradient_new.segment_range_split_midpoint(0, 0))
# We split first one of five, now have 6
gimp_assert("Verify new segment", gradient_new.get_number_of_segments() == 6)
gimp_assert("Verify range splitting uniform",gradient_new.segment_range_split_uniform(1, 1, 3))
# We split second one of six into 3 parts, now have 8
gimp_assert("Verify new number of segments", gradient_new.get_number_of_segments() == 8)

gimp_assert("Verify deletion",gradient_new.segment_range_delete(6, 6))
# We deleted seventh one of 8, not have seven
gimp_assert("Verify segments after deletion", gradient_new.get_number_of_segments() == 7)

actual_delta = gradient_new.segment_range_move(1, 1, -1.0, False)
gimp_assert("Verify delta without compression", actual_delta == -0.0637499999)
# Moving does not change the count of segments
gimp_assert("Verify no segment count change", gradient_new.get_number_of_segments() == 7)

gimp_assert("Verify redistribution", gradient_new.segment_range_redistribute_handles(0, 5))

gimp_assert("Verify blend",gradient_new.segment_range_blend_colors(1, 4))

gimp_assert("Verify blend opacity",gradient_new.segment_range_blend_opacity(2, 3))

gimp_assert("Verify out of range fails",not gradient_new.segment_set_left_color(9, bg_color))

gradient_new.delete()
gimp_assert("Verify deletion", not gradient_new.is_valid())

