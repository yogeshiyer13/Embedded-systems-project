#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/mman.h>
#include "address_map_arm.h"

//video defines
#define X_RES               320
#define Y_RES               240 
#define COMMAND 	          64
#define video_BYTES 8  

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
int screen_x, screen_y;
int video_FD = -1; // video file descriptor
//int KEY_FD = -1; // KEYS file descriptor

char command[COMMAND];// buffer for commands written to /dev/video

volatile int stop = 0;
void catchSIGINT(int s)
{
    stop = 1;
}

// mandelbrot iteration max
#define max_count 500

// mandelbrot arrays
float Zre, Zim, Cre, Cim, Zre_temp, Zim_temp;
float Zre_sq, Zim_sq ;
float x[320], y[240];
int i, j, count, total_count;

//function prototypes
void clear_screen(void);
void friendly_cleanup(void);
int read_from_driver_FD(int, char[], int);
void mandelbrot (void);
//void display_video(int samples[], int size);

int main(void)
{
  char video_buffer[video_BYTES]; // buffer for video char data
  char *argument;
  // catch SIGINT from ^C, instead of having it abruptly close this program
  signal(SIGINT, catchSIGINT);

  //if ((video_FD = open("/dev/IntelFPGAUP/video", O_RDWR)) == -1) 

  // Open the video character device driver, using INTEL drivers for portability
  if ((video_FD = open("/dev/video", O_RDWR)) == -1) 
  {
    printf ("Error opening /dev/video: %s\n", strerror(errno));
    friendly_cleanup();
    return -1;
  }

  	// Read VGA screen size from the video driver
	if(read_from_driver_FD(video_FD, video_buffer, video_BYTES) != 0)
    {
		printf("Error: Cannot read from the driver (/dev/video)\n");
		return 1;
	}

  //Print coordinates received from driver
	argument = strchr(video_buffer,' ');
	*argument = '\0';
	screen_y = atoi(argument + 1);
	screen_x = atoi(video_buffer);
    printf("SCREEN_X %d\n",screen_x);
    printf("SCREEN_Y %d\n",screen_y);

  //Clear VGA screen
  clear_screen();
 
	while(!stop)
	{
    mandelbrot();
  }	

  
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

void mandelbrot (void)
{
    // x array sweep
    for (i=0; i<320; i++) 
    {
      x[i] = -2.0f + 3.0f * (float)i/320.0f;
    }

    //printf("X %d\n", x[300]);
      
    // y arrays
    for (j=0; j<240; j++) 
    {
      y[j] = -1.0f + 2.0f * (float)j/240.0f ;
    }

    //printf("Y %d\n", y[300]);

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

        //printf("current count %d\n",count);
        
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
