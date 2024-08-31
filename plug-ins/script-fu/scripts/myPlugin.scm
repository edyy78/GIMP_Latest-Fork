#!/usr/bin/env gimp-script-fu-interpreter-3.0
;!# Close comment started on first line. Needed by gettext.


; This is just a demo and should not be permanently in the repo
; Just a user installable plugin not ordinarily already installed.
; User would ordinarily choose a plugin from "Downloads" and not the repo

(define (script-fu-my-plugin)
  (gimp-message "My plugin works")
)

; not a filter
; no arguments and no GUI

(script-fu-register "script-fu-my-plugin"
  "My plugin"
  ""
  "lkk"
  "lkk"
  "2024"
  ""
)

(script-fu-menu-register "script-fu-my-plugin" "<Image>/Filters/Development/Plug-In Examples")