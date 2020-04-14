#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/mman.h>
#include "address_map_arm.h"
#include <fcntl.h>


//mouse define
#define mouse_BYTES         3


// for mouse movements and coordinates
int mouse_FD = -1; // mouse file descriptor
char mouse_buffer[mouse_BYTES]; // buffer for video char data
int mouse_bytes, left, middle, right; 
signed char x_mouse, y_mouse;

// catch SIGINT
volatile int stop = 0;
void catchSIGINT(int s)
{
    stop = 1;
}

//function prototypes
void friendly_cleanup(void);
int read_from_driver_FD(int, char[], int);


int main(int argc, char *argv[])
{
  // catch SIGINT from ^C, instead of having it abruptly close this program
  signal(SIGINT, catchSIGINT);

    // Open the mouse device
	if (argc != 2)
	{
		printf("Specify the path to the mouse device ex. /dev/input/mice\n");
		friendly_cleanup();
	  return -1;
	}

	// Error handling for mouse
	if ((mouse_FD = open (argv[1], O_RDONLY | O_NONBLOCK)) == -1)
	{
		printf ("Could not open %s\n", argv[1]);
		friendly_cleanup();
	  return -1;
	}
 
	while(!stop)
	{
    // Read Mouse     
        mouse_bytes = read(mouse_FD, mouse_buffer, sizeof(mouse_buffer));

   /* // Read mouse from driver
	  if(read_from_driver_FD(mouse_FD, mouse_buffer, mouse_BYTES) != 0)
    {
		printf("Error: Cannot read from the driver (/dev/input/mouse)\n");
		return 1;
	  }   */
    
      if(mouse_bytes > 0)
      {
        left = mouse_buffer[0] & 0x1;
        right = mouse_buffer[0] & 0x2;
        middle = mouse_buffer[0] & 0x4;

        x_mouse = mouse_buffer[1];
        y_mouse = mouse_buffer[2];
        printf("x=%d, y=%d, left=%d, middle=%d, right=%d\n", x_mouse, y_mouse, left, middle, right);
      
      }
    }  	
    
  // exiting main
 // clear_screen();
  friendly_cleanup();
	return 0;
}

///*****************Local Functions*****************/


void friendly_cleanup(void)
{   
  //close video char dev
  if (mouse_FD != -1)
    close (mouse_FD);
}


