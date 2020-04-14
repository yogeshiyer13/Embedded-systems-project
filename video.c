#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "address_map_arm.h"

#define SUCCESS 0
#define DEVICE_NAME "video"
#define MAX_SIZE 256
#define PIXEL_CMD_PRE_SIZE 6
#define LINE_CMD_PRE_SIZE 5
#define BUFFER_REGISTER         0
#define BACK_BUFFER_REGISTER    1
#define RESOLUTION_REGISTER     2
#define STATUS_REGISTER         3
//Define contants for box
#define BOX_CMD_PRE_SIZE 4
//constant fro char display
#define TEXT_CMD_PRE_SIZE 5

// Declare global variables needed to use the pixel buffer
void *LW_virtual; // used to access FPGA light-weight bridge
volatile int * pixel_ctrl_ptr; // virtual address of pixel buffer controller
int pixel_buffer; // used for virtual address of pixel buffer
//double buffer pointer
void *SDRAM_virtual; // used to access the SDRAM to store buffer 
int pixel_back_buffer; // used for virtual address of pixel back buffer
int wrt_buffer; // used for virtual address of write buffer front or back
//part6
volatile int * char_ctrl_ptr; // virtual address of the character buffer controller
int char_buffer; // used for virtual address of character buffer
int resolution_x, resolution_y; // VGA screen size
//char driver display text
int c_resolution_x, c_resolution_y;

// Declare variables and prototypes needed for a character device driver
static int video_open (struct inode *, struct file *);
static int video_release (struct inode *, struct file *);
static ssize_t video_read (struct file *, char *, size_t, loff_t *);
static ssize_t video_write (struct file *, const char *, size_t, loff_t *);

//structures for pixel and line data
typedef struct pixel_data
{
	int x, y, color;
} pixel_data;

typedef struct box_line_data
{
	int x0, y0, x1, y1, color;
} box_line_data;

//strucute for text data
typedef struct text_data
{
	int x, y;
	char *string;
} text_data;

//declare local function prototypes
void get_screen_specs(volatile int *);
void clear_screen(void);
void plot_pixel(int, int, short);
void draw_line(int, int, int, int, short int);
void draw_box(int, int, int, int, short int ); 
void sync_loop(void);
//pat5 display text functions
void put_char(int, int, char);
void erase_characters(void);
void write_text(int, int, char*);
//struct return functions
pixel_data parse_pixel_command(char*);
box_line_data parse_box_line_command(char*, char);
text_data parse_text_command(char*);
void swap_int(int*, int*);


//video buffer
static char video_m1[MAX_SIZE];
//static char video_m2[MAX_SIZE];
//static int back_buffer;

//Pointers for character device driver
static dev_t video_no = 0;
static struct cdev *video_cdev = NULL;
static struct class *video_class = NULL;

//File Operations structure to open, release, read and write device-driver
static struct file_operations video_fops = {
	.owner = THIS_MODULE,
	.read = video_read,
	.write = video_write,
	.open = video_open,
	.release = video_release
};

/* Code to initialize the video driver */
static int __init start_video(void)
{
	int err = 0;
    // initialize the dev_t, cdev, and class data structures
    if ((err = alloc_chrdev_region (&video_no, 0, 1, DEVICE_NAME)) < 0) {
		printk (KERN_ERR "video: alloc_chrdev_region() failed with return value %d\n", err);
		return err;
	}
    // Allocate and initialize the character device
	video_cdev = cdev_alloc ();
	video_cdev->ops = &video_fops;
	video_cdev->owner = THIS_MODULE;

    // Add the character device to the kernel
	if ((err = cdev_add (video_cdev, video_no, 1)) < 0) {
		printk (KERN_ERR "video: cdev_add() failed with return value %d\n", err);
		return err;
	}

    video_class = class_create (THIS_MODULE, DEVICE_NAME);
	device_create (video_class, NULL, video_no, NULL, DEVICE_NAME);

    // generate a virtual address for the FPGA lightweight bridge
		LW_virtual = ioremap_nocache (0xFF200000, 0x00005000);
		if (LW_virtual == 0)
			printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
    
	// Create virtual memory access to the pixel buffer controller
		pixel_ctrl_ptr = (unsigned int *) (LW_virtual + 0x00003020);
		get_screen_specs (pixel_ctrl_ptr); // determine X, Y screen size

	// Create virtual memory access to the character buffer controller
		char_ctrl_ptr = (unsigned int*) (LW_virtual + CHAR_BUF_CTRL_BASE);

    // Create virtual memory access to the pixel buffer
		pixel_buffer = (int) ioremap_nocache (0xC8000000, 0x0003FFFF);
		if (pixel_buffer == 0)
			printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");

	// Create virtual memory access to the character buffer
        char_buffer = (int) ioremap_nocache (0xC9000000, 0x00002FFF);
        if (char_buffer == 0)
            printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
	// Create virtual memory access to the pixel back buffer
		pixel_back_buffer = (int) ioremap_nocache( SDRAM_BASE, FPGA_ONCHIP_SPAN );
		if (pixel_back_buffer == 0)
            printk (KERN_ERR "SDRAM Error: ioremap_nocache returned NULL\n");
		
	
		*(pixel_ctrl_ptr + BACK_BUFFER_REGISTER) = SDRAM_BASE;

		//front/back swap
		wrt_buffer = pixel_buffer;

		//2 resolutions, calculate here fix get_screen_specs
		resolution_x = (*(pixel_ctrl_ptr + RESOLUTION_REGISTER) & 0xFFFF);
    	resolution_y = ((*(pixel_ctrl_ptr + RESOLUTION_REGISTER) >> 16 ) & 0xFFFF);
		c_resolution_x = (*(pixel_ctrl_ptr + RESOLUTION_REGISTER) & 0xFFFF);
    	c_resolution_y = ((*(pixel_ctrl_ptr + RESOLUTION_REGISTER) >> 16 ) & 0xFFFF);

	/* Erase the pixel buffer */
		clear_screen();
		return 0;
	}

void get_screen_specs(volatile int * pixel_ctrl_ptr)
{
	sprintf(video_m1, "%i %i\n", resolution_x, resolution_y);
}
void clear_screen(void)
{
    int x,y;

    for(x = 0; x < resolution_x; x++)
    {
        for(y = 0; y < resolution_y; y++)
        {
            plot_pixel(x, y, 0);
        }
    }
}
void plot_pixel(int x, int y, short int color)
{
	//changed to write buffer to do the swap
	short int* pixel = (short int*) (wrt_buffer + ((x & 0x1FF) << 1) + ((y & 0xFF) << 10)); 
	*pixel = color;
}

void draw_line(int x0, int y0, int x1, int y1, short int color)
{
	int deltax = 0;
	int deltay = 0;
	int error = 0;
	int x = 0;
	int y = 0;
	int y_step = 0;
	int is_steep = (abs(y1-y0) > abs(x1-x0));
	
	if (is_steep)
	{
		swap_int(&x0, &y0);
		swap_int(&x1, &y1);
	}
	if (x0 > x1)
	{
		swap_int(&x0, &x1);
		swap_int(&y0, &y1);
	}
	
	deltax = x1 - x0;
	deltay = abs(y1 - y0);
	error = -(deltax / 2);
	y = y0;
	
	if (y0 < y1)
		y_step = 1;
	else
		y_step = -1;
	
	for (x = x0; x <= x1; x++)
	{
		if (is_steep)
			plot_pixel(y, x, color);
		else
			plot_pixel(x, y, color);
		
		error = error + deltay;
		
		if (error > 0)
		{
			y = y + y_step;
			error = error - deltax;
		}
	}
		
}

void sync_loop(void)
{
	*(pixel_ctrl_ptr + BUFFER_REGISTER) = 0x1;
	
	while(*(pixel_ctrl_ptr + STATUS_REGISTER) & 1)
	{
		// Block
	}

	if(*(pixel_ctrl_ptr + BUFFER_REGISTER) == SDRAM_BASE)
	{
		wrt_buffer = pixel_buffer;
	}	
	else
	{
		wrt_buffer = pixel_back_buffer;
	}
	return;
}

void draw_box(int x0, int y0, int x1, int y1, short int color)
{
	int i = 0;

	if (x0 > resolution_x)
		x0 = resolution_x;
	if (x1 > resolution_x)
		x1 = resolution_x;
	if (y0 > resolution_y)
		y0 = resolution_y;
	if (y1 > resolution_y)
		y1 = resolution_y;

	if (y0 > y1)
		swap_int(&y1, &y0);

	for(i=y0; i<=y1; i++)
	{
		draw_line(x0, i, x1, i, color);
	}
}


static void __exit stop_video(void)
{
/* unmap the physical-to-virtual mappings */
	iounmap (LW_virtual);
	iounmap ((void *) pixel_buffer);
	iounmap ((void *) pixel_back_buffer);
	iounmap (SDRAM_virtual);

/* Remove the device from the kernel */
    device_destroy (video_class, video_no);
	cdev_del (video_cdev);
	class_destroy (video_class);
	unregister_chrdev_region (video_no, 1);
}
static int video_open(struct inode *inode, struct file *file)
{
return SUCCESS;
}
static int video_release(struct inode *inode, struct file *file)
{
return 0;
}
static ssize_t video_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	size_t bytes;

	get_screen_specs(pixel_ctrl_ptr);
	sprintf(video_m1, "%d %d\n", resolution_x, resolution_y);

	bytes = strlen (video_m1) - (*offset);	// how many bytes not yet sent?
	bytes = bytes > length ? length : bytes;	// too much to send all at once?

	if (bytes)
		if (copy_to_user (buffer, &video_m1[*offset], bytes) != 0)
			printk (KERN_ERR "Error: copy_to_user unsuccessful");
	*offset = bytes;	// keep track of number of bytes sent to the user
	return bytes;
}
static ssize_t video_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
 	size_t bytes;
	bytes = length;
	unsigned long ret = 0;


	if (bytes > MAX_SIZE - 1)	// can copy all at once, or not?
		bytes = MAX_SIZE - 1;

	//if (copy_from_user (video_m1, buffer, bytes) != 0)
	//	printk (KERN_ERR "Error: copy_from_user unsuccessful");

	ret = copy_from_user (video_m1, buffer, bytes);
    video_m1[bytes] = '\0';
	if (video_m1[bytes-1] == '\n')
		video_m1[bytes-1] = '\0';

    //Check user input and perform actions
    if (!strcmp(video_m1, "clear"))
    { 
        clear_screen(); 
    }

    else if (!strncmp(video_m1, "pixel ", PIXEL_CMD_PRE_SIZE))
	{
		pixel_data pixel = parse_pixel_command(video_m1);
		plot_pixel(pixel.x, pixel.y, pixel.color);
	}
	else if (!strncmp(video_m1, "line ", LINE_CMD_PRE_SIZE))
	{
		box_line_data line = parse_box_line_command(video_m1,0);
		draw_line(line.x0, line.y0, line.x1, line.y1, line.color);
	}

	else if (!strncmp(video_m1, "box ", BOX_CMD_PRE_SIZE))
	{
		box_line_data box = parse_box_line_command(video_m1, 1);
		draw_box(box.x0, box.y0, box.x1, box.y1, box.color);
	}

	else if (!strcmp(video_m1, "sync"))
	{
		sync_loop();
	}
	else if (!strcmp(video_m1, "erase"))
	{
		erase_characters();
	}
	else if (!strncmp(video_m1, "text ", TEXT_CMD_PRE_SIZE))
	{
		text_data text = parse_text_command(video_m1);
		write_text(text.x, text.y, text.string);
	}


	return bytes;
}

pixel_data parse_pixel_command(char* command)
{
	// When this function is called, we have to start with a pixel
	pixel_data pixel = {0, 0, 0};
	int err = 0;
	char* arguments = command + PIXEL_CMD_PRE_SIZE;
	char* comma_pos = strchr(arguments, ',');
	char* space_pos = strchr(arguments, ' ');

	// Input is not correctly formatted
	if (comma_pos == NULL || space_pos == NULL)
		return pixel;

	// By replacing the \0 to the position of the space and the comma
	// in the argument string we create substrings that are parsed
	// by the kstrtoint function. If the function fails, it means
	// there is a syntatic error in the command, and then a default
	// pixel is returned
	err = kstrtoint(space_pos+1, 16, &pixel.color);
	*space_pos = '\0';
	err |= kstrtoint(comma_pos+1, 10, &pixel.y);
	*comma_pos = '\0';
	err |= kstrtoint(arguments, 10, &pixel.x);

	// If the color input is not a valid hex number, the  pixel 0,0
	// is set to black. 
	if (err)
	{
		pixel.x = 0;
		pixel.y = 0;
		pixel.color = 0;
	}

	return pixel;
}

box_line_data parse_box_line_command(char* command, char box)
{
	//similar logic adding "\0" as above, new structure to draw the box
	box_line_data line = {0, 0, 0, 0, 0};
	int err = 0;
	//A box or a line
	char* arguments = command + (box ? BOX_CMD_PRE_SIZE : LINE_CMD_PRE_SIZE);
	char* comma = NULL;

	char* point_space = strchr(arguments, ' ');
	*point_space = '\0';
	comma = strchr(arguments, ',');
	*comma = '\0';
	err = kstrtoint(arguments, 10, &line.x0);
	err |= kstrtoint(comma+1, 10, &line.y0);

	arguments = point_space+1;

	point_space = strchr(arguments, ' ');
	*point_space = '\0';
	comma = strchr(arguments, ',');
	*comma = '\0';
	err |= kstrtoint(arguments, 10, &line.x1);
	err |= kstrtoint(comma+1, 10, &line.y1);

	arguments = point_space+1;
	
	err |= kstrtoint(arguments, 16, &line.color);
	
	return line;
}

//similar parsing for text

text_data parse_text_command(char* command)
{
	text_data text = {0, 0, " "};
	int err = 0;
	char* arguments = command + TEXT_CMD_PRE_SIZE;
	char* comma_pos = strchr(arguments, ',');
	char* space_pos = strchr(arguments, ' ');

	// Input is not correctly formatted
	if (comma_pos == NULL || space_pos == NULL)
		return text;

	text.string = space_pos + 1;
	*space_pos = '\0';
	err |= kstrtoint(comma_pos+1, 10, &text.y);
	*comma_pos = '\0';
	err |= kstrtoint(arguments, 10, &text.x);

	// Forcing string limitation based on the resolution.
	if (text.x >= c_resolution_x)
		text.x = c_resolution_x - 1;
	if (text.y >= c_resolution_y)
		text.y = c_resolution_y - 1;

	if (err)
	{
		text.x = 0;
		text.y = 0;
		text.string = " ";
	}

	return text;
}

//character display functions
void put_char(int x, int y, char character_in)
{
	//shape of the register
	short int* character = (short int*) (char_buffer + (x & 0x7F) + ((y & 0x3F) << 7));

	*character = character_in;
}

void erase_characters(void)
{
	int x = 0;
	int y = 0;

	for (x = 0; x < c_resolution_x; x++)
		for (y = 0; y < c_resolution_y; y++)
			put_char(x, y, ' ');	
}

void write_text(int x, int y, char* string)
{
	int written_chars = 0;

	while(written_chars != strlen(string))
	{
		put_char(x,y,string[written_chars]);
		x++;

		if (x > c_resolution_x - 1)
		{
			x=0;
			y++;
		}

		if(y > c_resolution_y - 1)
			y=0;

		written_chars++;
	}
}


void swap_int(int* a, int* b)
{
	*a = *a + *b;
	*b = *a - *b;
	*a = *a - *b;
}
MODULE_LICENSE("GPL");
module_init (start_video);
module_exit (stop_video);