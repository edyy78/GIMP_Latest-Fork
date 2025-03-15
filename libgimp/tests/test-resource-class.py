#!/usr/bin/env python3

brush    = Gimp.context_get_brush()
font     = Gimp.context_get_font()
gradient = Gimp.context_get_gradient()
palette  = Gimp.context_get_palette()
pattern  = Gimp.context_get_pattern()

"""
Test is_valid method
Expect True
"""
gimp_assert("Verify valid brush", brush.is_valid())
gimp_assert("Verify valid font", font.is_valid())
gimp_assert("Verify valid gradient", gradient.is_valid())
gimp_assert("Verify valid palette", palette.is_valid())
gimp_assert("Verify valid pattern", pattern.is_valid())

"""
Test get_name method
Expect string
"""

gimp_assert("Verify default brush", brush.get_name()    == 'Clipboard Image')
gimp_assert("Verify default brush", font.get_name()     == 'Sans-serif')
gimp_assert("Verify default brush", gradient.get_name() == 'FG to BG (RGB)')
gimp_assert("Verify default brush", palette.get_name()  == 'Color History')
gimp_assert("Verify default brush", pattern.get_name()  == 'Clipboard Image')

"""
Test resource as an arg
Pass the resource back to the context
Expect no exceptions
"""
gimp_assert("Verify set_brush", Gimp.context_set_brush(brush))
gimp_assert("Verify set_font", Gimp.context_set_font(font))
gimp_assert("Verify set_gradient", Gimp.context_set_gradient(gradient))
gimp_assert("Verify set_palette", Gimp.context_set_palette(palette))
gimp_assert("Verify set_pattern", Gimp.context_set_pattern(pattern))

"""
Test new() methods

Not Font, Pattern.
Pass desired name.
"""
brush_new = Gimp.Brush.new("New Brush")
gimp_assert("Verify new brush validity", brush_new.is_valid())
gimp_assert("Verify name of new brush", brush_new.get_name()=="New Brush")

gradient_new = Gimp.Gradient.new("New Gradient")
gimp_assert("Verify new gradient validity", gradient_new.is_valid())
gimp_assert("Verify new gradient name", gradient_new.get_name()=="New Gradient")

palette_new = Gimp.Palette.new("New Palette")
gimp_assert("Verify new palette validity", palette_new.is_valid())
gimp_assert("Verify new palette name", palette_new.get_name()=="New Palette")

"""
Test delete methods.
Delete the new resources we just made
After deletion, reference is invalid
"""
gimp_assert("Verify deletion of brush", brush_new.delete())
gimp_assert("Verify brush invalidity after deletion",not brush_new.is_valid())

gimp_assert("Verify deletion of gradient", gradient_new.delete())
gimp_assert("Verify gradient invalidity after deletion",not gradient_new.is_valid())

gimp_assert("Verify deletion of palette", palette_new.delete())
gimp_assert("Verify palette invalidity after deletion", not palette_new.is_valid())

"""
Test copy methods.

Copy the original resources from context.
Assert that GIMP adds " copy"
"""
brush_copy = brush.duplicate()
gimp_assert("Verify duplicate brush", brush_copy.is_valid())
print(brush_copy.get_name())
gimp_assert("Verify duplicate brush name", brush_copy.get_name()=='Clipboard Image copy')

gradient_copy = gradient.duplicate()
gimp_assert("Verify duplicate gradient", gradient_copy.is_valid())
gimp_assert("Verify duplicate gradient name", gradient_copy.get_name()=='FG to BG (RGB) copy')

palette_copy = palette.duplicate()
gimp_assert("Verify duplicate palette", palette_copy.is_valid())
gimp_assert("Verify duplicate palette name", palette_copy.get_name()== 'Color History copy')


"""
Test rename methods.

Renaming returns a reference that must be kept. !!!

Rename existing copy to a desired name.
!!! assign to a new named var
"""
brush_renamed = brush_copy.rename("Renamed Brush")
gimp_assert("Verify validity of renamed brush", brush_renamed)
gimp_assert("Verify name of renamed brush", brush_copy.get_name()=="Renamed Brush")

gradient_renamed = gradient_copy.rename("Renamed Gradient")
gimp_assert("Verify validity of renamed gradient", gradient_renamed)
gimp_assert("Verify name of renamed gradient", gradient_copy.get_name()=="Renamed Gradient")

palette_renamed = palette_copy.rename("Renamed Palette")
gimp_assert("Verify validity of renamed palette", palette_renamed)
gimp_assert("Verify name of renamed palette", palette_copy.get_name()=="Renamed Palette")


"""
Test renaming when name is already in use.
If "Renamed Brush" is already in use, then the name will be
"Renamed Brush copy #1"
that is, renaming added suffix "#1"

This is due to core behavior, not libgimp resource class per se,
so we don't test it here.
"""

"""
Test delete methods
Delete the renamed copies we just made
After deletion, reference is invalid
"""
brush_copy.delete()
gimp_assert("Verify deletion of renamed brush",not brush_copy.is_valid())

gradient_copy.delete()
gimp_assert("Verify deletion of renamed gradient",not gradient_copy.is_valid())

palette_copy.delete()
gimp_assert("Verify deletion of renamed palette",not palette_copy.is_valid())


'''
Test class method

!!! Expect no error dialog in GIMP.
The app code generated from foo.pdb should clear the GError.
'''
gimp_assert("Verify invalidity of invalid brush",    not Gimp.Brush.id_is_valid    (-1))
gimp_assert("Verify invalidity of invalid font",     not Gimp.Font.id_is_valid     (-1))
gimp_assert("Verify invalidity of invalid gradient", not Gimp.Gradient.id_is_valid (-1))
gimp_assert("Verify invalidity of invalid palette",  not Gimp.Palette.id_is_valid  (-1))
gimp_assert("Verify invalidity of invalid pattern",  not Gimp.Pattern.id_is_valid  (-1))
