(define latched 0)
(define stall-timer 0)

(loopwhile t
  (let ((d (abs (get-duty)))
        (r (abs (get-rpm)))
        (f (get-fault)))

    (if (and (> d 0.01) (< r 500))
      (setq stall-timer (+ stall-timer 1))
      (setq stall-timer 0))

    (if (and (= latched 0) (or (> stall-timer 15) (> f 0)))
      (progn
        (write-line "Stall detected, control disabled")
        (setq latched 1)))

    ; EXECUTE LATCH
    (if (= latched 1)
      (progn
        ; Sets 'App to Use' to 0 (No App)
        ; This prevents the remote/controller from overriding the stop command
        (conf-set 'app-to-use 0)
        (set-current 0)
        (set-brake 0)))

    (sleep 0.1)
  )
)
