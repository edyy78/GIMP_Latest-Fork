;; -*-scheme-*-

;; Alan Horkan 2004.  No copyright.  Public Domain.

(define (script-fu-guide-new-percent image
                                     drawables
                                     direction
                                     position)

  (define HORIZONTAL_ONLY                      0)
  (define VERTICAL_ONLY                        1)
  (define HORIZONTAL_AND_VERTICAL              2)
  (define HORIZONTAL_MIRRORED                  3)
  (define VERTICAL_MIRRORED                    4)
  (define HORIZONTAL_AND_VERTICAL_MIRRORED     5)
  (define HORIZONTAL_REPEATING                 6)
  (define VERTICAL_REPEATING                   7)
  (define HORIZONTAL_AND_VERTICAL_REPEATING    8)
  (define HORIZONTAL_REPEAT_INCL0              9)
  (define VERTICAL_REPEAT_INCL0                10)
  (define HORIZONTAL_AND_VERTICAL_REPEAT_INCL0 11)

  (script-fu-use-v3)

  (let* (
        (width        (gimp-image-get-width image))
      	(height       (gimp-image-get-height image))
      	(position_h   (/ (* height position) 100))
	    (position_v   (/ (* width position) 100))
        (mirrored     0)
        (repeated     0)
        (repeat_at_0  0)
        (not_finished 1)
        (offset       0)
        )

    (gimp-image-undo-group-start image)

    (if (>= direction HORIZONTAL_REPEAT_INCL0)
      (begin                  ; repeated with a guide at 0
        (set! direction (- direction HORIZONTAL_REPEAT_INCL0))
        (set! repeated 1)
        (set! repeat_at_0 1)
      )
    )

    (if (>= direction HORIZONTAL_REPEATING)
      (begin                  ; repeated without a guide at 0
        (set! direction (- direction HORIZONTAL_REPEATING))
        (set! repeated 1)
      )
    )

    (if (>= direction HORIZONTAL_MIRRORED)
      (begin                  ; mirrored
        (set! direction (- direction HORIZONTAL_MIRRORED))
        (set! mirrored 1)
      )
    )

    (if (or (= direction HORIZONTAL_ONLY) (= direction HORIZONTAL_AND_VERTICAL))
      (begin
        (gimp-image-add-hguide image position_h)

        (if (and (= mirrored 1) (not (= position 50.0)))
          (begin
            (set! position_h (/ (* height (- 100.0 position)) 100))
            (gimp-image-add-hguide image position_h)
          )
        )

        (if (and (= repeated 1) (> position 0.0))
          (begin
            (if (= repeat_at_0 1)
              (gimp-image-add-hguide image 0)
            )
            (set! not_finished 1)
            (set! offset position_h)
            (while (= not_finished 1)
              (set! not_finished 0)
              (set! position_h (+ position_h offset))
              (if (<= position_h (+ height 1))    ; +1 to allow for rounding errors
                                                  ;  preventing (for instance) a guide
                                                  ;  at 100% when repeating a 10%
                                                  ;  setting
                (begin
                  (gimp-image-add-hguide image position_h)
                  (set! not_finished 1)
                )
              )
            )
          )
        )
      )
    )

    (if (or (= direction VERTICAL_ONLY) (= direction HORIZONTAL_AND_VERTICAL))
      (begin
        (gimp-image-add-vguide image position_v)

        (if (and (= mirrored 1) (not (= position 50.0)))
          (begin
            (set! position_v (/ (* width (- 100.0 position)) 100))
            (gimp-image-add-vguide image position_v)
          )
        )

        (if (and (= repeated 1) (> position 0.0))
          (begin
            (if (= repeat_at_0 1)
              (gimp-image-add-vguide image 0)
            )
            (set! not_finished 1)
            (set! offset position_v)
            (while (= not_finished 1)
              (set! not_finished 0)
              (set! position_v (+ position_v offset))
              (if (<= position_v (+ width 1))     ; +1 to allow for rounding errors
                                                  ;  preventing (for instance) a guide
                                                  ;  at 100% when repeating a 10%
                                                  ;  setting

                (begin
                  (gimp-image-add-vguide image position_v)
                  (set! not_finished 1)
                )
              )
            )
          )
        )
      )
    )

    (gimp-image-undo-group-end image)
    (gimp-displays-flush)
  )
)

(script-fu-register-filter "script-fu-guide-new-percent"
  _"New Guide (by _Percent)..."
  _"Add a guide at the position specified as a percentage of the image size"
  "Alan Horkan"
  "Alan Horkan, 2004"
  "April 2004"
  "*"
  SF-ONE-OR-MORE-DRAWABLE  ; doesn't matter how many drawables are selected
  SF-OPTION     _"_Direction"       '(_"Horizontal"
                                      _"Vertical"
                                      _"Both axes"
                                      _"Horizontal mirrored"
                                      _"Vertical mirrored"
                                      _"Both axes mirrored"
                                      _"Horizontal repeating"
                                      _"Vertical repeating"
                                      _"Both axes repeating"
                                      _"Horizontal repeating including 0"
                                      _"Vertical repeating including 0"
                                      _"Both axes repeating including 0")
  SF-ADJUSTMENT _"_Position (in %)" '(50 0 100 1 10 2 1)
)

(script-fu-menu-register "script-fu-guide-new-percent"
                         "<Image>/Image/Guides")
