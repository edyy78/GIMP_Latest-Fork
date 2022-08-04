#!/usr/bin/env gimp-script-fu-interpreter-3.0

; A script that aborts

; Expect "Test>Call script-fu-script-abort in the menus
; Expect when chosen, message on GIMP message bar "Aborting"
; Expect an error dialog in GIMP app that requires an OK

(define (script-fu-call-script-abort)
  (script-fu-script-abort "Aborting")
  (gimp-message "Never evaluated")
)

(script-fu-register "script-fu-call-script-abort"
  "Call script-fu-script-abort"
  _"Expect error dialog in Gimp, or PDB execution error when called by another"
  "lkk"
  "lkk"
  "2022"
  ""  ; requires no image
  ; no arguments or dialog
)

(script-fu-menu-register "script-fu-call-script-abort" "<Image>/Test")
