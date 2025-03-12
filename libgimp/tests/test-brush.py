#!/usr/bin/env/ python3

brush_default = Gimp.context_get_brush()
gimp_assert("Verify default brush.", brush_default.get_name() == "Clipboard Image")

gimp_assert("Verify properties.", not brush_default.is_generated() and
            (not brush_default.is_editable()))

success, width, height, mask_bpp, color_bpp = brush_default.get_info()
gimp_assert("Verify info.", success and
            width == 17 and
            height == 17 and
            mask_bpp == 1 and
            color_bpp == 0)

spacing = brush_default.get_spacing()
gimp_assert("Verify spacing", spacing == 20)

# shape, radius, spikes, hardness, aspect_ratio, angle
success, shape = brush_default.get_shape()
gimp_assert("Verify getter fail", not success)

success, radius = brush_default.get_radius()
gimp_assert("Verify getter fail", not success)

success, spikes = brush_default.get_spikes()
gimp_assert("Verify getter fail", not success)

success, hardness = brush_default.get_hardness()
gimp_assert("Verify getter fail", not success)

success, aspect_ratio = brush_default.get_aspect_ratio()
gimp_assert("Verify getter fail", not success)

success, angle = brush_default.get_angle()
gimp_assert("Verify getter fail", not success)

success = brush_default.set_spacing(1)
gimp_assert("Verify spacing does not change", not success)

success, returned_shape = brush_default.set_shape(Gimp.BrushGeneratedShape.DIAMOND)
gimp_assert("Verify set does not work", not success)

success, returned_radius = brush_default.set_radius(1.0)
print(returned_radius)
gimp_assert("Verify set does not work", not success)

success, returned_spikes = brush_default.set_spikes(1)
gimp_assert("Verify set does not work", not success)

success, returned_hardness = brush_default.set_hardness(1.0)
gimp_assert("Verify set does not work", not success)

success, returned_aspect_ratio = brush_default.set_aspect_ratio(1.0)
gimp_assert("Verify set does not work", not success)

success, returned_angle = brush_default.set_angle(1.0)
gimp_assert("Verify set does not work", not success)



brush_new = Gimp.Brush.new("New Brush")
gimp_assert("Verify state.", brush_new.is_generated() and brush_new.is_editable())

success = brush_new.set_spacing(20)
spacing = brush_new.get_spacing()
gimp_assert("Verify succes of both get and set", success and spacing == 20)

s_set, returned_shape = brush_new.set_shape(Gimp.BrushGeneratedShape.DIAMOND)
s_get, shape = brush_new.get_shape()
gimp_assert("Verify success of both get and set", s_get and
            s_set and
            shape == Gimp.BrushGeneratedShape.DIAMOND and
            shape == returned_shape)

s_set, returned_radius = brush_new.set_radius(4.0)
s_get, radius = brush_new.get_radius()
gimp_assert("Verify success of both get and set", s_get and
            s_set and
            radius == 4.0 and
            radius == returned_radius)

s_set, returned_spikes = brush_new.set_spikes(2)
g_get, spikes = brush_new.get_spikes()
gimp_assert("Verify success of both get and set", s_get and
            s_set and
            spikes == 2 and
            radius == returned_radius)

s_set, returned_hardness = brush_new.set_hardness(0.5)
s_get, hardness = brush_new.get_hardness()
gimp_assert("Verify success f both get and set", s_get and
            s_set and
            hardness == 0.5 and
            hardness == returned_hardness)

s_set, returned_aspect_ratio = brush_new.set_aspect_ratio(5.0)
s_get, aspect_ratio = brush_new.get_aspect_ratio()
gimp_assert("Verify success of both get and set", s_get and
            s_set and
            aspect_ratio == 5.0 and
            aspect_ratio == returned_aspect_ratio)

s_set, returned_angle = brush_new.set_angle(90.0)
s_get, angle = brush_new.get_angle()
gimp_assert("Verify success of both get and set", s_get and
            s_set and
            angle == 90.0 and
            angle == returned_angle)


"""
Radius upper limit causes some bug.
success, returned_radius = brush_new.set_radius(40000)
success, radius = brush_new.get_radius()
gimp_assert("Verify upper limits of properties", radius == 32767.0)
"""

success, returned_spikes = brush_new.set_spikes(22)
success, spikes = brush_new.get_spikes()
gimp_assert("Verify upper limits of properties", spikes == 20)

success, returned_hardness = brush_new.set_hardness(2.0)
success, hardness = brush_new.get_hardness()
gimp_assert("Verify upper limits of properties", hardness == 1.0)

success, returned_aspect_ratio = brush_new.set_aspect_ratio(2000)
success, aspect_ratio = brush_new.get_aspect_ratio()
gimp_assert("Verify upper limits of properties", aspect_ratio == 1000.0)

success, returned_angle = brush_new.set_angle(270)
success, angle = brush_new.get_angle()
gimp_assert("Verify upper limits of properties", angle == 90)

success = brush_new.delete()
gimp_assert("Verify brush is deleted.", success)
