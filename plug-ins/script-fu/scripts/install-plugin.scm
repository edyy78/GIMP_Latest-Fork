; install a plugin

; This is somewhat a mockup.
; It might need to move to Python to get better default source directory
; and for more user friendliness:
; the user should not be able to pick from any .config/GIMP directory,
; that is misguided, those plugins will already be installed.

; The default does not need to be a source file nested in another directory.

(define (script-fu-install-plugin inFile)
  (gimp-plug-in-install inFile)
)

; not a filter

(script-fu-register "script-fu-install-plugin"
  "Install plugin..."
  "Installs a binary or interpretable plugin into GIMP"
  "lkk"
  "lkk"
  "2024"
  ""

  ; FIXME: default folder should be "Downloads"
  SF-FILENAME   "Plugin source file" "/work/myPlugin/myPlugin.scm"
)

(script-fu-menu-register "script-fu-install-plugin" "<Image>/Filters/Development")
