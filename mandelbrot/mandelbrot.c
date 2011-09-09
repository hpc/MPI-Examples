/* File: mandelbrot.c
   Modified by: Jharrod LaFon
   Date: Spring 2011
   Purpose:  Compute and display the Mandelbrot set
   */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <mpi.h>
#define		X_RESN	1024       /* x resolution */
#define		Y_RESN	1024      /* y resolution */
#define     M_MAX   (1 << 24)       /* Max iterations for Mandelbrot */
// BITMAP header
#pragma pack(push, 1)
typedef struct
{
    uint16_t type; 
    uint32_t size;
    uint16_t reserved1, reserved2;
    uint32_t offset;
} HEADER;
#pragma pack(pop)

// BITMAP information header
#pragma pack(push, 1)
typedef struct
{
    uint32_t size;
    int32_t  width, height;
    uint16_t planes;
    uint16_t bits;
    uint32_t compression;
    uint32_t isize;
    int32_t  xres,yres;
    uint32_t colors;
    uint32_t impcolors;
} IHEADER;
#pragma pack(pop)

// Pixel 
#pragma pack(push, 1)
typedef struct Pixel
{
    uint8_t b,g,r;
} Pixel;
#pragma pack(pop)

// Struct to hold info on a bitmap
#pragma pack(push, 1)
typedef struct
{
    HEADER header;
    IHEADER iheader;
    Pixel ** array;
} BMP;
#pragma pack(pop)


typedef struct complextype
	{
        float real, imag;
	} Compl;
typedef struct Xstuff
{
    Window * win;
    Display * display;
    GC * gc;
} Xstuff;

typedef enum { DATA_TAG, TERM_TAG, RESULT_TAG} Tags;

int cal_pixel(Compl c)
{
    int count;
    Compl z;
    float temp, lengthsq;
    z.real = 0; z.imag = 0;
    count = 0;
    do
    {
        temp = z.real * z.real - z.imag * z.imag + c.real;
        z.imag = 2 * z.real * z.imag + c.imag;
        z.real = temp;
        lengthsq = z.real * z.real + z.imag * z.imag;
        count++;
    } while((lengthsq < 4.0) && (count < M_MAX));
    return count;
}

void draw_row(Xstuff * session, int row,int * colors)
{
    int j = 0;
    for(j=0; j < Y_RESN; j++) 
      if (colors[j] == 100) 
          XDrawPoint (session->display, *session->win, *session->gc, j, row);

}

void worker()
{
    int * colors = malloc(sizeof(int) * (X_RESN+1));
    MPI_Status status;
    int row,x,rank;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Recv(&row , 1 , MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    Compl c;
    while(status.MPI_TAG == DATA_TAG)
    {
        colors[0] = row;
        c.imag = ((float)row - 400.0)/200.0;
        for(x = 0; x < X_RESN; x++)
        {
            c.real = ((float) x - 400.0)/200.0;
            colors[x+1] = cal_pixel(c);
        }
        MPI_Send(&colors[0], X_RESN, MPI_INT, 0, RESULT_TAG,MPI_COMM_WORLD);
        MPI_Recv(&row , 1 , MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      
    }
}

int main (int argc, char **argv)
{
    int rank, size;
    if(MPI_Init(&argc,&argv) != MPI_SUCCESS)
    {
        perror("Unable to initialize MPI\n");
        exit(1);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if(rank != 0)
        worker();
    else
    {

    Window		win;                            /* initialization for a window */
	unsigned
	int             width, height,                  /* window size */
                        x, y,                           /* window position */
                        border_width,                   /*border width in pixels */
                        display_width, display_height,  /* size of screen */
                        screen;                         /* which screen */

	char            *window_name = "Mandelbrot Set", *display_name = NULL;
	GC              gc;
	unsigned
	long		valuemask = 0;
	XGCValues	values;
	Display		*display;
	XSizeHints	size_hints;
    Xstuff session;
//	Pixmap		bitmap;
//	XPoint		points[800];
//	FILE		*fp, *fopen ();
//	char		str[100];
	
	XSetWindowAttributes attr[1];

      
	/* connect to Xserver */

	if (  (display = XOpenDisplay (display_name)) == NULL ) {
	   fprintf (stderr, "drawon: cannot connect to X server %s\n",
				XDisplayName (display_name) );
	exit (-1);
	}
	
	/* get screen size */

	screen = DefaultScreen (display);
	display_width = DisplayWidth (display, screen);
	display_height = DisplayHeight (display, screen);

	/* set window size */

	width = X_RESN;
	height = Y_RESN;

	/* set window position */

	x = 0;
	y = 0;

        /* create opaque window */

	border_width = 4;
	win = XCreateSimpleWindow (display, RootWindow (display, screen),
				x, y, width, height, border_width, 
				BlackPixel (display, screen), WhitePixel (display, screen));

	size_hints.flags = USPosition|USSize;
	size_hints.x = x;
	size_hints.y = y;
	size_hints.width = width;
	size_hints.height = height;
	size_hints.min_width = 300;
	size_hints.min_height = 300;
	
	XSetNormalHints (display, win, &size_hints);
	XStoreName(display, win, window_name);

        /* create graphics context */

	gc = XCreateGC (display, win, valuemask, &values);

	XSetBackground (display, gc, WhitePixel (display, screen));
	XSetForeground (display, gc, BlackPixel (display, screen));
	XSetLineAttributes (display, gc, 1, LineSolid, CapRound, JoinRound);

	attr[0].backing_store = Always;
	attr[0].backing_planes = 1;
	attr[0].backing_pixel = BlackPixel(display, screen);

	XChangeWindowAttributes(display, win, CWBackingStore | CWBackingPlanes | CWBackingPixel, attr);

	XMapWindow (display, win);
	XSync(display, 0);
    session.gc = &gc;
    session.win = &win;
    session.display = display;
      	 
   /* Mandlebrot variables */
    int k, count, row;
    

    count = 0;
    row = 0;
    for(k = 1; k < size; k++)
    {
        MPI_Send(&row, 1, MPI_INT, k, DATA_TAG,MPI_COMM_WORLD);
        count++;
        row++;
    }
    int * colors = (int *) malloc(sizeof(int) * (X_RESN+1));
    MPI_Status status;
    do
    {
        MPI_Recv(&colors[0] , X_RESN , MPI_INT, MPI_ANY_SOURCE, RESULT_TAG, MPI_COMM_WORLD, &status);
        count--;
        if(row < X_RESN)
        {
            MPI_Send(&row, 1, MPI_INT, status.MPI_SOURCE, DATA_TAG, MPI_COMM_WORLD);
            row++;
            count++;
        }
        else
            MPI_Send(&row, 1, MPI_INT, status.MPI_SOURCE, TERM_TAG, MPI_COMM_WORLD);
        draw_row(&session,colors[0],&colors[1]);
    }
    while(count > 0);
	printf("\n");
	XFlush (display);
	sleep (10);
	/* Program Finished */

    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}

