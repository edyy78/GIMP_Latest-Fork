#!/usr/bin/env python3

font_default = Gimp.context_get_font()
gimp_assert('Verify Default font', font_default.get_name() == "Sans-serif")
