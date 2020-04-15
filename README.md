# Final Project - Mandelbrot Viewer using DE1SoC Board

The Programming has been done using C/C++ and the Linux interface of the board has been used.

**Instructions**

To compile we use the following code : gcc -Wall -o mandelbrot mandelbrot.c -lm -pthread 

Insert the video driver using: insmod video.ko

Make sure to connect the USB mouse to the device.  

We have two codes to show: 
1) mandelbrot_old.c ( has zoom functionality upto factor of 2)
2) mandelbrot.c ( shows the mouse coordinates in complex space)

After that we run the code using: ./mandelbrot_old /dev/input/mouse0

**Note:**
- When pressing the mouse button keep it pressed for few seconds then only you'd be able to see the change in the VGA screen for zoom in and out.

On Terminal window you can observe the mouse left, middle and right data being changed.

Also for the coordinates part please run the following: ./mandelbrot /dev/input/mouse0

On Terminal window you can observe the mouse coordinates in complex space.

**Extras:**
- [Final Project Report](https://drive.google.com/file/d/1dkwd3aEwDjmG0-xR9eDcRPx4XfrU25Oe/view?usp=sharing) has also been attached to get to know more about the project.

Mandelbrot set looks like this:

![Mandelbrot set](Mandel.jpg)



