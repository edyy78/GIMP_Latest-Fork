;; -*-scheme-*-

;;  No copyright.  Public Domain.

(define (script-fu-guide-rules image
                               drawables
                               rule)
  (let* (
        (width (car (gimp-image-get-width image)))
      	(height (car (gimp-image-get-height image)))
        (SQRT5 2.236067977)
        )

    (gimp-image-undo-group-start image)

    (if (= rule 0)          ; centre lines
        (prog1
            (gimp-image-add-hguide image (/ height 2))
            (gimp-image-add-vguide image (/ width 2))
        )
    )

    (if (= rule 1)          ; rule of thirds
        (prog1
            (gimp-image-add-hguide image (/ height 3))
            (gimp-image-add-vguide image (/ width 3))
            (gimp-image-add-hguide image (/ (* height 2) 3))
            (gimp-image-add-vguide image (/ (* width 2) 3))
        )
    )

    (if (= rule 2)          ; rule of fifths
        (prog1
            (gimp-image-add-hguide image (/ height 5))
            (gimp-image-add-vguide image (/ width 5))
            (gimp-image-add-hguide image (/ (* height 2) 5))
            (gimp-image-add-vguide image (/ (* width 2) 5))
            (gimp-image-add-hguide image (/ (* height 3) 5))
            (gimp-image-add-vguide image (/ (* width 3) 5))
            (gimp-image-add-hguide image (/ (* height 4) 5))
            (gimp-image-add-vguide image (/ (* width 4) 5))
        )
    )

    (if (= rule 3)          ; golden sections
        (prog1
            (gimp-image-add-hguide image (/ (* height (+ 1 SQRT5)) (+ 3 SQRT5)))
            (gimp-image-add-hguide image (/ (* height 2) (+ 3 SQRT5)))
            (gimp-image-add-vguide image (/ (* width (+ 1 SQRT5)) (+ 3 SQRT5)))
            (gimp-image-add-vguide image (/ (* width 2) (+ 3 SQRT5)))
        )
    )

    (gimp-image-undo-group-end image)
    (gimp-displays-flush)
  )
)

(script-fu-register-filter "script-fu-guide-rules"
  _"New Guide - rule of thirds etc."
  _"Add guides according to the selected rule"
  "Richard McLean"
  "Richard McLean, 2024"
  "February 2024"
  "*"
  SF-ONE-OR-MORE-DRAWABLE  ; doesn't matter how many drawables are selected
  SF-OPTION     _"_Rule"       '(_"Center lines"
                                 _"Rule of thirds"
                                 _"Rule of fifths"
                                 _"Golden sections")
)

(script-fu-menu-register "script-fu-guide-rules"
                         "<Image>/Image/Guides")
