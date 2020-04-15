Final Project - Mandelbrot Viewer

By station 73 

Diego Defaz(1004905132) and Yogesh Iyer(1005603438)

Instructions:

Go to the project folder using cd project

To compile we use the following code : gcc -Wall -o mandelbrot mandelbrot.c -lm -pthread 

Insert the video driver using: insmod video.ko

Make sure to connect the USB mouse to the device.  

We have two codes to show: 
1) mandelbrot_old.c ( has zoom functionality upto factor of 2)
2) mandelbrot.c ( shows the mouse coordinates in complex space)

After that we run the code using: ./mandelbrot_old /dev/input/mouse0

Note: When pressing the mouse button keep it pressed for few seconds then only you'd be able to see the change in the VGA screen for zoom in and out.

On Terminal window you can observe the mouse left, middle and right data being changed.

Also for the coordinates part please run the following: ./mandelbrot /dev/input/mouse0

On Terminal window you can observe the mouse coordinates in complex space.

The program Mandelbrot.c prints the complex coordinates where the mouse is in although it doesn’t show the zoom function at all in the VGA monitor. 
The program Mandelbrot_old.c has the zoom with a factor of 2 but it doesn’t show the mouse coordinate in the complex space.

Extras: 
So we tried a lot stuff for the zoom part and we couldn't get the desired output that we had aimed for. 



