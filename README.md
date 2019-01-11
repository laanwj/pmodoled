Drive Digilent pmodOLED module from a HiFive board

 - SPI interface attached via GPIO pins. Uses SiFive SPI device, or alternatively
   simple GPIO bitbanging.
 - SSD1306 controller.
 - 128x32 display.

![SiFive with connected display](img/example.jpg)

(note: this is an older version of the software when the wiring was different).

Wiring up
-----------

See [display.c](display.c) under "Wiring" how to wire connect the PMOD connector
to the HiFive GPIO pins.

Usage
----------

- Build the [freedom-e-sdk](https://github.com/sifive/freedom-e-sdk)
- Check out this repository into the `software` subdirectory.
```bash
cd software
git clone https://github.com/laanwj/pmodoled.git
cd ..
```
- Compile
```
make software PROGRAM=pmodoled
```
Alternatively you can run `make -C software/pmodoled` yourself. This is useful
during incremental development as no automatic `make clean` will be run.

- Upload program to device
```
make upload PROGRAM=pmodoled
```

Demo
------

The program will automatically launch after it is uploaded.

Initially it will display a zooming mandelbrot set on the display, and log a
bit of debug information to the UART.

At the moment there are two modes:

- Mandelbrot mode: Show a zooming mandelbrot set. To switch mode, type any
  character on the serial console.

- Terminal mode: the device will act as a simple terminal: everything you enter
  on the serial console will be printed to the display. Newline and backspace
  should work as expected. Escape exits to the next mode.
