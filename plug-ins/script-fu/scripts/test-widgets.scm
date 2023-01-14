#!/usr/bin/env gimp-script-fu-interpreter-3.0

; A script to test widgets of GimpProcedureDialog
; See also test-sphere which has parameters unused.
; This accesses all parameters
;

(define (script-fu-test-widgets image drawables image2 layer2 vectors)
  (let* (
      (foo image2)
      )
    (gimp-message "Starting")
    (if (= 1 (car (gimp-image-id-is-valid image2)))
      (gimp-message "Image valid")
      (gimp-message "Image invalid"))
    (if (= 1 (car (gimp-item-id-is-valid layer2)))
      (gimp-message "layer valid")
      (gimp-message "layer invalid"))
  )
)

(script-fu-register-filter "script-fu-test-widgets"
  "Test widgets..."
  "Expect dialog with widgets. After OK, a message from each widget"
  "lkk"
  "lkk"
  "2022"
  "*"  ; requires any image type
  SF-ONE-OR-MORE-DRAWABLE
  SF-IMAGE      "Image"              -1
  SF-LAYER      "Layer"              -1
  SF-VECTORS    "Vectors"            -1
)

(script-fu-menu-register "script-fu-test-widgets" "<Image>/Test")
