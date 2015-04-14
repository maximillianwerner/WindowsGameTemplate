# WindowsGameTemplate
This template is both a preliminary exercise for myself and also an informative breakdown of setting up the Windows aspect of a game from scratch. For anybody new to Windows programming, the comments provide a brief description of some of the new mechanics at work.
The base of this template has the same core as any Windows program. Steps are taken for initializing the window, the window class, all of its attributes. beyond the base, there are three main components to this template: Rendering, DirectSound, and Controller Support.

1. The rendering is done by using a Device Independent Bitmap and is redrawn every time the window is resized. Furthermore, the buffer will copy to the window with every "PAINT" message sent to the window. For demonstrational purposes, an RGB gradient is being rendered to the screen.

2. The DirectSound library is being used for sound output. I am NOT using DirectSound8 and am instead simnply using the previous version. As a test, I am running a sine wave at 262Hz.

3. The XInput library is implemented with this program. Plug in a wired Xbox 360 controller to test the vibrate functionality. For additional fun, pressing the A button will shift the gradient horizontally.

