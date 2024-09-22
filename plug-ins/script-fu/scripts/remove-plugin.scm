; remove a plugin by its menu label

; A mock-up.
; FUTURE: a Python plug-in with a menu-label chooser
; calling gimp_plug_in_manager_get_user_menu_labels


(define (script-fu-remove-plugin menu-label)
  (gimp-message "This is just a mockup, enter a menu label.")
  (gimp-plug-in-remove menu-label)
)

; not a filter
; Dialog should let user choose a menu label.

(script-fu-register "script-fu-remove-plugin"
  "Remove plugin..."
  "Removes a user-installed plugin from GIMP, by menu label"
  "lkk"
  "lkk"
  "2024"
  ""

  ; Defaults to menu label of a plugin in the repo used for testing
  ; plug-ins/script-fu/scripts/myPlugin.scm
  SF-STRING   "Plugin menu label"  "My plugin"
)

(script-fu-menu-register "script-fu-remove-plugin" "<Image>/Filters/Development")
