#!/usr/bin/env python3
#
# GIMP - The GNU Image Manipulation Program
# Copyright (C) 1995 Spencer Kimball and Peter Mattis
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

r"""
Framework for running test files written as a simple text file.

This script parses the simple test descriptions of the form:

```
title:crop example
author:Jehan
action:image-new
keyboard:Enter
toolbox:crop
canvas:click(10,10)
canvas:drag(30,40)
keyboard:Enter
test:image.width == 20 and image.height == 30
```

(This is "app/tests/tools/crop_canvas.txt".)

We pass these files as a list to meson.build which
allows the meson test command to run them into this script like so:

    ./tests.py tools/crop_canvas.txt

then collate the results.

Currently, this only works on GNU/Linux with X11.
"""

__copyright__ = "Copyright (C) 1995-1997 Spencer Kimball and Peter Mattis"
__license__ = "GPL"

import os
import sys
import time
import pyatspi
import gi
gi.require_version("Atspi", "2.0")
from gi.repository import Atspi
from PIL import Image

output_fname = "testing.log"

def log(s):
    r"""
    Record output to output_fname and timestamp it.
    """
    global output_fname
    with open(output_fname, "a") as f:
        f.write("%s | %s\n" % (time.strftime("%H:%M:%S"), s))

def load_running_gimp(exe_name="gimp-2.99"):
    r"""
    Find the first running instance of GIMP in the pyatspi
    registry or return 0 if it's not found.
    """
    desktop = pyatspi.Registry.getDesktop(0)
    for app in desktop:
        if app.name == exe_name:
            return app
    return 0

def load_gimp(exe_name="gimp-2.99", startup_time=5):
    r"""
    Load GIMP pyatspi object if it's running.
    Otherwise start GIMP, wait `startup_time` and then load it.
    """
    log("Finding running development version of GIMP.")
    gimp = load_running_gimp(exe_name)

    if gimp == 0:
        log("Not found, starting a new instance.")
        os.system(exe_name + " &")
        time.sleep(startup_time)
        gimp = load_running_gimp(exe_name)

        if gimp == 0:
            log("ERROR: %s is not running." % (exe_name))
            exit(1)

    log("GIMP found.")
    return gimp

def find_element(obj, label, role_name, level=0, max_level=10):
    r"""
    Recursively search for label of the given role under the assumption
    that it's unique.

    If not found before getting 10 levels deep in the tree,
    then return None.
    """
    if level > max_level:
        return None
    for i in range(obj.childCount):
        a = obj.getChildAtIndex(i)
        if a is None:
            continue
        if a.getRoleName() == role_name:
            if a.name == label:
                return a
        result = find_element(a, label, role_name, level+1)
        if result:
            return result
    return None

def load_menubar(obj):
    r"""
    TODO: currently incomplete.

    Creates a dictionary of UI elements in the menu bar.
    """
    menu_bar = find_element(obj, "", "menu bar")
    menu = {}
    for i in range(menu_bar.childCount):
        a = menu_bar.getChildAtIndex(i)
        if a is None:
            continue
        if a.getRoleName() == "menu":
            menu[a.name] = {
                "object": a,
                "children": []
            }

    log(menu)
    return menu

def mouse_function(action="", position=""):
    r"""
    TODO: System dependent, should check for what backends are available.

    Parses a command of the form
        click --canvas 10 100
    then calls xdotool and asks it to click there.
    """
    log("mouse " + action + " " + str(position))
    if position != "":
        if "," in position:
            x,y = position.split(",")
            os.system("xdotool mousemove %d %d" % (int(x), int(y)))

    if action == "left":
        os.system("xdotool click 1")
    elif action == "middle":
        os.system("xdotool click 2")
    elif action == "right":
        os.system("xdotool click 3")

def keyboard_function(command):
    r"""
    TODO: System dependent, should check for what backends are available.

    Given a key combination of the form "ctrl+n" this calls xdotool
    and asks it to press it.
    """
    log("key press " + command)
    if command == "Enter":
        command = "Return"
    os.system("xdotool key " + command)

def action_function(command):
    r"""
    Runs macros that represent basic actions, ideally they shouldn't
    rely on anything that can change like specific pixel locations.

    For now, uses default keyboard shortcuts.
    """
    if command == "image-new":
        keyboard_function("ctrl+n")

def toolbox_function(command):
    r"""
    For actions in toolbox, so repeated labels in other widgets
    are disambiguated.
    """
    if command == "crop":
        keyboard_function("shift+c")

def test_function(command):
    r"""
    Works on the saved output to the file named `Untitled.png` in the
    current directory.
    """
    keys = "ctrl+shift+e,Return,Return".split(",")
    for key in keys:
        keyboard_function(key)
        time.sleep(1)
    # Here we load the saved file, so sleep first to allow it to save.
    time.sleep(2)
    # bootchk: This is an unsolved part. I don't want to rely on PIL here.
    I = Image.open("Untitled.png")
    print(I)

def run_test(fname, pause_length=0.5):
    r"""
    Run the lines in the file named `fname`.
    """
    raise_window()

    log("Getting pyatspi handles...")
    gimp = load_gimp()
    menu = load_menubar(gimp)

    with open(fname, "r") as script:
        for line in script.readlines():
            log(line)
            if ":" not in line:
                continue
            line = line.replace("\n", "")
            line = line.replace("\r", "")
            tokens = line.split(":")
            if len(tokens) < 2:
                log(line)
            elif tokens[0] == "log":
                log(tokens[1])
            elif tokens[0] == "mouse":
                mouse_function(action=tokens[1])
            elif tokens[0] == "mouse_move":
                mouse_function(position=tokens[1])
            elif tokens[0] == "keyboard":
                keyboard_function(tokens[1])
            elif tokens[0] == "action":
                action_function(tokens[1])
            elif tokens[0] == "toolbox":
                toolbox_function(tokens[1])
            elif tokens[0] == "test":
                test_function(tokens[1])
            # Pause to allow the interface to keep up at each step.
            time.sleep(pause_length)

def raise_window():
    r"""
    TODO: System dependent, should check for what backends are available.
    """
    log("Bringing GIMP window to front...")
    os.system(r"""
WINDOWS=`xdotool search --name "GNU Image Manipulation Program"`
for f in $WINDOWS
do
xdotool windowactivate $f
done
""")


if __name__ == "__main__":
    log("GIMP Test")
    log(time.ctime())
    if sys.argv[-1] != "tests.py":
        log("Running test: %s." % (sys.argv[-1]))
        run_test(sys.argv[-1])
    else:
        log("ERROR: no test file supplied.")
        exit(1)

