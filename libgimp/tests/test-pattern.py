#!/usr/bin/env python3

pattern = Gimp.context_get_pattern()
gimp_assert('Verify Default pattern', pattern.get_name() == "Clipboard Image")

success, width, height, bpp = pattern.get_info()
gimp_assert("Verify info", success == True and
            width == 16 and
            height == 16 and
            bpp == 3)

