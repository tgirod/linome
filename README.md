This program aims to reproduce the behaviour of monome's serialosc, but with a
launchpad instead.

My goal is to expose a very monome like OSC API, and extend it with launchpad
specific elements : red/green leds with variable intensity, scene and control
buttons ...

The launchpad's micro-contoller uses a degraded mode for USB communication,
with very poor bandwidth. Moreover, the protocol used by the launchpad in order
to communicate with the computer is poorly designed. The result is a crappy
refresh rate.

In order to mitigate the problem, linome exploits two functionnalities of the
launchpad: **fast led update** and **double buffering**. The first allows us to
update the 80 RG leds of the launchpad in one batch, doing it with 81 bytes
instead of 240. The second allows us to send our 81 bytes of LED data, then
update the 80 LEDs all at once.


