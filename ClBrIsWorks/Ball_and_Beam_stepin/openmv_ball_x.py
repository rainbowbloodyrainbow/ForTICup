import sensor
import image
import time
import pyb
from machine import UART


# OpenMV Cam H7 Plus UART3: P4=TX, P5=RX.
uart = UART(3, baudrate=115200)

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.set_windowing((0, 100, 320, 20))
sensor.skip_frames(time=2000)

sensor.set_auto_gain(False)
sensor.set_gainceiling(8)
sensor.set_auto_exposure(False, exposure_us=5000)

led = pyb.LED(3)
led.off()
clock = time.clock()

SEARCH_ROI = (40, 0, 240, 20)

while True:
    clock.tick()
    img = sensor.snapshot()
    gray = img.to_grayscale()
    circles = gray.find_circles(roi=SEARCH_ROI,
                                threshold=700,
                                x_margin=15,
                                y_margin=15,
                                r_margin=10,
                                r_min=3,
                                r_max=5)

    best_circle = None
    best_score = 0.0
    for circle in circles:
        if circle[3] > best_score:
            best_score = circle[3]
            best_circle = circle

    if best_circle:
        x = best_circle[0]
        y = best_circle[1] + 100
        radius = best_circle[2]
        img.draw_circle((x, best_circle[1], radius), color=255, thickness=2)
        img.draw_cross((x, best_circle[1]), color=255)
        led.on()

        # Keep the MCU protocol ASCII-only and line-based.
        uart.write(("X%d\r\n" % x).encode("ascii"))
        print("ball:(%d, %d) r=%d FPS:%.1f" %
              (x, y, radius, clock.fps()))
    else:
        led.off()
        uart.write(b"N\r\n")
        print("no ball FPS:%.1f" % clock.fps())
