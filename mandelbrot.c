#define _GNU_SOURCE 						// required for set_processor_affinity code
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include "address_map_arm.h"
//linux threads
#include <linux/input.h>
#include <pthread.h>
#include <sched.h>

//gcc -Wall -o mandelbrot mandelbrot.c -lm -pthread

//video defines
#define X_RES               320
#define Y_RES               240 
#define COMMAND 	          64
#define video_BYTES         8  

//mouse define
#define mouse_BYTES         3
#define EVENT_LBUTTON       1
#define EVENT_RBUTTON       2
#define EVENT_MBUTTON       4
#define EVENT_MOUSEMOVE     0 

//define color numbers
#define PURPLE                0x90FF
#define DARK_PURPLE           0X6170
#define LIGHT_PURPLE          0x39FF
#define GREY                  0x8410
#define BROWN                 0xB301
#define RED                   0xF000
#define GREEN                 0x0F00
#define GREEN2			          0x7777
#define BLUE                  0x0099
#define NAVY                  0x0009
#define PINK                  0xF00F
#define YELLOW                0xFF00
#define LIGHT_BLUE            0x0FFF
#define ORANGE                0xF501
#define WHITE                 0xFFFF
#define BLACK                 0x0001

// Global variables
//video 
int screen_x, screen_y;
int video_FD = -1; // video file descriptor
char command[COMMAND];// buffer for commands written to /dev/video
char video_buffer[video_BYTES]; // buffer for video char data
char *argument;

// for mouse movements and coordinates
int mouse_FD = -1; // mouse file descriptor
char mouse_buffer[mouse_BYTES]; // buffer for video char data
int mouse_bytes, left, middle, right, event; 
signed char x_mouse, y_mouse;

struct mouse
{
  int event;
  int x_c;
  int y_c;
};

struct mouse mandel_mouse;

// mandelbrot iteration max
#define max_count 500

// mandelbrot arrays
float Zre, Zim, Cre, Cim;
float Zre_sq, Zim_sq ;
//arrays for conversion from screen space to image space
float x[320], y[240];
int i, j, count, total_count;

//function prototypes
void clear_screen(void);
void friendly_cleanup(void);
int read_from_driver_FD(int, char[], int);
void mandelbrot(struct mouse mandel_mouse);
void CallBackFunc(int, int, int);

//Threads variables
pthread_mutex_t mice_mutex;
static volatile sig_atomic_t stop;
static pthread_t t_mouse, t_video;			// thread IDs

// Set the current thread's affinity to the core specified
int set_processor_affinity(unsigned int core) {
    cpu_set_t cpuset;
    pthread_t current_thread = pthread_self(); 
    
    if (core >= sysconf(_SC_NPROCESSORS_ONLN)){
        printf("CPU Core %d does not exist!\n", core);
        return -1;
    }
    // Zero out the cpuset mask
    CPU_ZERO(&cpuset);
    // Set the mask bit for specified core
    CPU_SET(core, &cpuset);
    
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset); 
}

void *mouse_thread( ) 
{
	set_processor_affinity(1);			// assign this thread to CPU 1
	while (1)
	{	
		pthread_testcancel();			// exit if this thread has been cancelled
    
      // Read mouse from driver     
      mouse_bytes = read(mouse_FD, mouse_buffer, mouse_BYTES);

      if(mouse_bytes > 0)
      {
        left = mouse_buffer[0] & 0x1;
        right = mouse_buffer[0] & 0x2;
        middle = mouse_buffer[0] & 0x4;

        x_mouse = mouse_buffer[1];
        y_mouse = mouse_buffer[2];
        printf("x=%d, y=%d, left=%d, middle=%d, right=%d\n", x_mouse, y_mouse, left, middle, right);

        if (left == 1) 
        {
          event = 1;
        } 
        else if (right == 2) 
        {
          event = 2;
        } 
        else if (middle == 4) 
        {
          event = 4;
        } 
        else if ( x_mouse > 0 || y_mouse > 0)
        { 
          event = 0;
        }
        printf ("event = %d\n", event); 

        //pthread_mutex_lock (&mice_mutex);
        CallBackFunc (event, x_mouse, y_mouse);
        //pthread_mutex_unlock (&mice_mutex);
      }      
	}
}

void *video_thread( ) 
{
	set_processor_affinity(0);			// assign this thread to CPU 0
  // Open the video character device driver, using INTEL drivers for portability
  //if ((video_FD = open("/dev/IntelFPGAUP/video", O_RDWR)) == -1) 
  if ((video_FD = open("/dev/video", O_RDWR)) == -1) 
  {
    printf ("Error opening /dev/video: %s\n", strerror(errno));
    friendly_cleanup();
    return (-1);
  }

  	// Read VGA screen size from the video driver
	if(read_from_driver_FD(video_FD, video_buffer, video_BYTES) != 0)
  {
		printf("Error: Cannot read from the driver (/dev/video)\n");
		return (-1);
	}

  //Print coordinates received from driver
	argument = strchr(video_buffer,' ');
	*argument = '\0';
	screen_y = atoi(argument + 1);
	screen_x = atoi(video_buffer); 
    printf("SCREEN_X %d\n",screen_x);
    printf("SCREEN_Y %d\n",screen_y);

  clear_screen();  

	while (1)
	{
		pthread_testcancel();			// exit if this thread has been canceled
    mandelbrot(mandel_mouse);
	}
}

// catch ctrl-c
void catchSIGINT(int s)
{
    stop = 1;
}

int main(int argc, char *argv[])
{
	int err1, err2, loops = 0;

	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
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
	
	// Spawn threads
	err1 = pthread_create (&t_mouse, NULL, &mouse_thread, NULL);
	err2 = pthread_create (&t_video, NULL, &video_thread, NULL);
	if ((err1 || err2) != 0)
	{
		printf("pthread_create failed\n");
		exit (-1);
	}
	set_processor_affinity(0);			// assign the main thread to CPU 0
	while (!stop) 
  {
		//printf ("The program is running\n");
    //printf ("count %d", count);
		++loops;
	}
	printf ("\nThe program is stopping!\n");
	pthread_cancel(t_mouse);
	pthread_cancel(t_video);
	// Wait until threads terminates
	pthread_join(t_mouse, NULL);
	pthread_join(t_video, NULL);

  // exiting main
  clear_screen();
  friendly_cleanup();

	return 0;
}

///*****************Local Functions*****************/
void clear_screen(void)
{
  write (video_FD, "clear", COMMAND);
  write (video_FD, "sync", COMMAND);
  write (video_FD, "clear", COMMAND);
}

void friendly_cleanup(void)
{   
  //close video char dev
  if (video_FD != -1)
    close (video_FD);
  //close mouse char dev
  if (mouse_FD != -1)
    close (mouse_FD);
}

int read_from_driver_FD(int driver_FD, char buffer[], int buffer_len)
{
	int bytes_read = 0;
	int bytes = 0;

	while ((bytes = read (driver_FD, buffer, buffer_len)) > 0)
		bytes_read += bytes;	// read the foo device until EOF	
		buffer[bytes_read] = '\0';	

	if (bytes_read != buffer_len) 
	{
		fprintf (stderr, "Error: %d bytes expected from driver "
			     "FD, but %d bytes read\n", buffer_len, bytes_read);
		return 1;
	}

	bytes_read = 0;

	return 0;
}

void mandelbrot (struct mouse mandel_mouse)
{   
  if (mandel_mouse.event != 0)
  {  
   switch (mandel_mouse.event)
   {
   case 1:
      // x array from screen space to imagespace
      for (i=0; i<320; i++) 
      {
        // range from -2 to 1
        x[i] = (-2.0f + 3.0f * (float)i/320.0f) * 0.5f;
      } 
      // y arrays from screen space to imagespace
      for (j=0; j<240; j++) 
      {
        // range from -1 to 1
        y[j] = (-0.5f + 1.5f * (float)j/240.0f) * 0.5f;
      }
     break;

   case 4:
      // x array from screen space to imagespace
      for (i=0; i<320; i++) 
      {
        // range from -2 to 1
        x[i] = (-2.0f + 3.0f * (float)i/320.0f)*2;
      } 
      // y arrays from screen space to imagespace
      for (j=0; j<240; j++) 
      {
        // range from -1 to 1
        y[j] = (-1.0f + 2.0f * (float)j/240.0f)*2;
      }

     break;
   
   default:

     break;
   }

  }
  else 
  {
    // x array from screen space to imagespace
    for (i=0; i<320; i++) 
    {
      // range from -2 to 1
      x[i] = -2.0f + 3.0f * (float)i/320.0f;
    } 
    // y arrays from screen space to imagespace
    for (j=0; j<240; j++) 
    {
    // range from -1 to 1
    y[j] = -1.0f + 2.0f * (float)j/240.0f ;
    }
  }
    
    // zero global counter
    total_count = 0 ;

    //sweep the coordinates of VGA screen  
    for (i=0; i<320; i++) 
    {
      for (j=0; j<240; j+=1) 
      {
        Zre = 0;
        Zre_sq = 0;
        Zim = 0;
        Zim_sq = 0;
        Cre = x[i];
        Cim = y[j];
        count = 0 ;

        // iterate to find convergence, or not
        while (count++ < max_count)
        {
          //complex iterator, implement f(z) = z^2+c
          Zre_sq = (Zre* Zre) - (Zim*Zim);
          //using shift to keep 28 bit logic
          Zim_sq = Zre*Zim*2.0f;
          Zre = Zre_sq + Cre ;
          Zim = Zim_sq + Cim ;
          // check non-convergence
          if ((Zre + Zim) >= 4.0)
          { 
          break;
          }
        }
        total_count += count ;
        
        //Plotting pixels depending   
        if (count>=max_count) 
        {
          int length = sprintf(command, "pixel %d,%d %X", i, j, BLUE);
          write(video_FD, command, length);
        }
        else if (count>=(max_count>>1))
        {
          int length = sprintf(command, "pixel %d,%d %X", i, j, PURPLE);
          write(video_FD, command, length);
        } 
        else if (count>=(max_count>>2))
        {
          int length = sprintf(command, "pixel %d,%d %X", i, j, PINK);
          write(video_FD, command, length);
        } 
        else if (count>=(max_count>>3))
        {
          int length = sprintf(command, "pixel %d,%d %X", i, j, RED);
          write(video_FD, command, length);
        } 
        else if (count>=(max_count>>4))
        {
          int length = sprintf(command, "pixel %d,%d %X", i, j, DARK_PURPLE);
          write(video_FD, command, length);         
        } 
        else if (count>=(max_count>>5))
        {
          int length = sprintf(command, "pixel %d,%d %X", i, j, LIGHT_BLUE);
          write(video_FD, command, length);
        } 
        else if (count>=(max_count>>6))
        {
          int length = sprintf(command, "pixel %d,%d %X", i, j, LIGHT_PURPLE);
          write(video_FD, command, length);
        } 
        else if (count>=(max_count>>7))
        {
          int length = sprintf(command, "pixel %d,%d %X", i, j, NAVY);
          write(video_FD, command, length);
        } 
        else 
        {
          int length = sprintf(command, "pixel %d,%d %X", i, j, BLACK);
          write(video_FD, command, length);
        }
        
      }
    }
    write (video_FD, "sync", COMMAND);
}
//Function to detect events and return coordinates from mouse
void CallBackFunc(int event, int x, int y)
{
     if  ( event == EVENT_LBUTTON )
     {
        mandel_mouse.x_c = x;
        mandel_mouse.y_c = y;
        mandel_mouse.event = EVENT_LBUTTON;
     }
     else if  ( event == EVENT_RBUTTON )
     {
        mandel_mouse.x_c = x;
        mandel_mouse.y_c = y;
        mandel_mouse.event = EVENT_LBUTTON;          
     }
     else if  ( event == EVENT_MBUTTON )
     {
        mandel_mouse.x_c = x;
        mandel_mouse.y_c = y;
        mandel_mouse.event = EVENT_MBUTTON; 
     }   
     else if ( event == EVENT_MOUSEMOVE )
     {
        mandel_mouse.x_c = x;
        mandel_mouse.y_c = y;
        mandel_mouse.event = EVENT_MOUSEMOVE;          
     }
}
