;; -*-scheme-*-

;; Alan Horkan 2004.  Public Domain.
;; so long as remove this block of comments from your script
;; feel free to use it for whatever you like.

(define (script-fu-guide-new image
                             drawables
                             direction
                             position)
  (define HORIZONTAL_ONLY                   0)
  (define VERTICAL_ONLY                     1)
  (define HORIZONTAL_AND_VERTICAL           2)
  (define HORIZONTAL_MIRRORED               3)
  (define VERTICAL_MIRRORED                 4)
  (define HORIZONTAL_AND_VERTICAL_MIRRORED  5)

  (script-fu-use-v3)

  (let* (
        (image_max_x         (- (gimp-image-get-width image) 1))
        (image_max_y         (- (gimp-image-get-height image) 1))
        (horizontal_reqd     0)
        (vertical_reqd       0)
        (mirrored            0)
        )

    (gimp-image-undo-group-start image)

    (if (or (= direction HORIZONTAL_ONLY) (= direction HORIZONTAL_AND_VERTICAL)
            (= direction HORIZONTAL_MIRRORED) (= direction HORIZONTAL_AND_VERTICAL_MIRRORED))
      (set! horizontal_reqd 1)
    )

    (if (or (= direction VERTICAL_ONLY) (= direction HORIZONTAL_AND_VERTICAL)
            (= direction VERTICAL_MIRRORED) (= direction HORIZONTAL_AND_VERTICAL_MIRRORED))
      (set! vertical_reqd 1)
    )

    (if (or (= direction HORIZONTAL_MIRRORED) (= direction VERTICAL_MIRRORED)
            (= direction HORIZONTAL_AND_VERTICAL_MIRRORED))
      (set! mirrored 1)
    )

    (if (= horizontal_reqd 1)
      (gimp-image-add-hguide image position)
    )

    (if (= vertical_reqd 1)
       (gimp-image-add-vguide image position)
    )

    (if (= mirrored 1)
      (begin
        (if (and (= horizontal_reqd 1) (not (= position (- image_max_y position))))
          (gimp-image-add-hguide image (- image_max_y position))
        )
        (if (and (= vertical_reqd 1) (not (= position (- image_max_x position))))
          (gimp-image-add-vguide image (- image_max_x position))
        )
      )
    )

    (gimp-image-undo-group-end image)

    (gimp-displays-flush)
  )
)

(script-fu-register-filter "script-fu-guide-new"
  _"New _Guide..."
  _"Add a guide at the orientation and position specified (in pixels)"
  "Alan Horkan"
  "Alan Horkan, 2004.  Public Domain."
  "2004-04-02"
  "*"
  SF-ONE-OR-MORE-DRAWABLE  ; doesn't matter how many drawables are selected
  SF-OPTION     _"_Direction" '(_"Horizontal" _"Vertical" _"Horizontal and vertical"
                _"Horizontal mirrored" _"Vertical mirrored" _"Horizontal and vertical mirrored")
  SF-ADJUSTMENT _"_Position"  (list 0 (* -1 MAX-IMAGE-SIZE) MAX-IMAGE-SIZE 1 10 0 1)
)

(script-fu-menu-register "script-fu-guide-new"
                         "<Image>/Image/Guides")
