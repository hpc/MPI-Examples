/*     File: image_manip.c
    Authors: Jharrod LaFon, Gholamali Rahnavard
       Date: Spring 2011
    Purpose: Low level parallel image manipulation.
   */
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include<string.h>
#include<math.h>
#include<mpi.h>
#define BM 0x4D42           // Value of a bitmap type
#define MAX_FILENAME 256   
#define PI 3.14159265
#define BLOCK_LOW(id,p,n) ((id)*(n)/(p))
#define BLOCK_HIGH(id,p,n) \
    (BLOCK_LOW((id)+1,p,n)-1)
#define BLOCK_SIZE(id,p,n) \
    (BLOCK_LOW((id)+1)-BLOCK_LOW(id))
#define BLOCK_OWNER(index,p,n) \
    (((p)*((index)+1)-1)/(n))

// Struct to hold a 2d vector
#pragma pack(push, 1)
typedef struct Vector
{
    int x,y;
} Vector;
#pragma pack(pop)
typedef enum  { NS, SHIFT, SCALE, ROTATE } operation;
typedef struct Options
{
    operation op;
    char * inputfile;
    char * filename;
    Vector v;
    double param;
    int flag;
    void (*func)(Vector *v, Vector *r, int p, int row, double sf);
} Options;

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

    HEADER header;
    IHEADER iheader;
    Pixel ** array;
} BMP;
#pragma pack(pop)

// Printed if a filename is not supplied
void usage()
{
//while((c = getopt(argc, argv, "f:o:x:y:r:s:")) != -1)
    printf("Usage: image_manip: -f <filename> [OPTS]\n\
            The program can be run without the following options for a menu.\n\
            Otherwise, an output file name is required.\n\
            Operations are mutually exclusive.\n\
            -o <filename>\tName of output file.\n\
            -x <int>\t\tAmount to shift in x direction. Requires -y\n\
            -y <int>\t\tAmount to shift in y direction. Requires -x\n\
            -r <double>\t\tAmount to rotate image.\n\
            -s <double>\t\tAmount to scale image.\n");
    exit(1);
}
void shift(Vector * v, Vector * r, int  p, int row, double sf)
{
    (*r).x = p + (*v).x;
    (*r).y = row + (*v).y;
}
void scale(Vector * v, Vector * r, int p, int row, double sf)
{
    (*r).x = (int)p*sf;
    (*r).y = (int)row*sf;
}
void rotate(Vector * v, Vector *r, int p, int row, double sf)
{
    (*r).x = (int)p*cos(sf*PI/180)+row*sin(sf*PI/180);
    (*r).y = (int)-1*p*sin(sf*PI/180)+row*cos(sf*PI/180);
}


// Performs an image shift
void do_op(BMP * img, Options * opt)
{
   int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    // Number of bits in a line
    int linebits = (*img).iheader.width * (*img).iheader.bits;
    // 4 byte alignment
    int linebytes = ((linebits + 31)/32)*4;
    Pixel * current;
    Vector newvec;
    // Accounting
    int row = 0;
    int p = 0;
    int count = sizeof(Pixel)*linebytes;
    MPI_Status status;
    // Master    
    if(rank == 0)
    {
        // Open the new file
        FILE * nf;
        nf = fopen((*opt).filename,"w");
        // Make sure we are at the beginning
        fseek(nf,0,SEEK_SET);

        // Write the headers
        fwrite(&(*img).header,1,sizeof((*img).header),nf);
        fwrite(&(*img).iheader,1,sizeof((*img).iheader),nf);

        // Allocate memory for a new pixel array
        Pixel ** newimg = malloc(sizeof(Pixel*) * (*img).iheader.height);

        // For every row
        for(row = 0; row < (*img).iheader.height; row++)
        {
            current = (Pixel *) malloc(sizeof(Pixel)*linebytes);
            memset(current,0,sizeof(Pixel)*linebytes);
            MPI_Recv(current, count,MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            newimg[status.MPI_TAG] = current; 
        }
        // Write the new image
        for(row = (*img).iheader.height-1; row >= 0; row--)
            fwrite(&newimg[row][0],1,linebytes,nf);
        fclose(nf);
        free(newimg);
        
    }
    else
    {
        current = (Pixel *) malloc(sizeof(Pixel)*linebytes);
        // For every row
        int r = BLOCK_LOW(rank-1,size-1,(*img).iheader.height);
        Pixel blank;
        blank.r = blank.g = blank.b = 0;
        
        for(row = r; row <= BLOCK_HIGH(rank-1,size-1,(*img).iheader.height); row++)
        {
//            fprintf(stderr,"Rank,row [%d][%d]\n",rank,row);
            // Set the memory to 0 by default (black), this also pads zeroes
            memset(current,0,sizeof(current));

            // For every pixel in the row (3 bytes)
            for(p = 0; p < linebytes/3; p++)
            {
              // Calc x,y
                (*opt).func(&(*opt).v, &newvec, p, row, (*opt).param);
              // Make sure they are on the canvas
              if(newvec.x >= 0 && newvec.x < linebytes/3 && newvec.y < (*img).iheader.height && newvec.y >= 0)
                  current[p] = (*img).array[newvec.y][newvec.x];
              else
                  current[p] = blank;
            }
            MPI_Send(current, count, MPI_BYTE, 0, row, MPI_COMM_WORLD);
        }   
    }
//    fprintf(stderr, "Rank %d: %d->%d\n",rank,BLOCK_LOW(rank-1,size-1,(*img).iheader.height),BLOCK_HIGH(rank-1,size-1,(*img).iheader.height));
free(current);
}



// Program menu
int do_menu(BMP * img, Options * opts)
{
    int rank,size;
    int choice;
    char * name = (char *) malloc(sizeof(char) * MAX_FILENAME);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    //fprintf(stderr,"[%s][%d] %d\n",__FILE__,__LINE__,rank);
    if(rank == 0)
    {
        printf("\
                Options\n\
                =======\n\
                1. Shift\n\
                2. Scale\n\
                3. Rotate\n\
                4. Exit\n\
                Enter a selection: ");
        fflush(stdout);
        scanf("%d",&choice);
        if(choice == 1 || choice == 2 || choice == 3)
        {
            printf("Enter a filename for the new image: ");
            fflush(stdout);
            scanf("%s",name);
            (*opts).filename = name;
        }
    }
    // \TODO: Merge this
    if(rank == 0)
    {
        switch(choice)
        {
            case 1:
                printf("Enter integer delta x: ");
            fflush(stdout);
                scanf("%d",&(*opts).v.x);
                printf("Enter integer delta y: ");
            fflush(stdout);
                scanf("%d",&(*opts).v.y);
                (*opts).func = shift;
                break;
            case 2:
                printf("Enter a decimal scale factor: ");
            fflush(stdout);
                scanf("%lf",&(*opts).param);
                (*opts).func = scale;
                break;
            case 3:
                printf("Enter decimal degrees to rotate: ");
            fflush(stdout);
                scanf("%lf",&(*opts).param);
                (*opts).func = rotate;
                break;
            case 4:
                return 0;
                break;
            default: break;
        }
    }
    MPI_Bcast(opts,sizeof(*opts),MPI_BYTE,0,MPI_COMM_WORLD);
    do_op(img, opts);
    
return 0;
}

// Read bitmap header
int get_bmp_header(FILE * fp, BMP * img)
{
    if(sizeof((*img).header) != fread(&(*img).header,1,sizeof((*img).header),fp))
    {
        perror("Unable to read file");
        exit(1);
    }
    if(sizeof((*img).iheader) != fread(&(*img).iheader,1,sizeof((*img).iheader),fp))
    {
        perror("Unable to read file");
        exit(1);
    }
    if((*img).header.type != BM)
    {
        printf("Error: Not a valid bitmap file.\n");
        exit(1);
    }
    return 0;
}

// Print attributes
int print_bmp_attr(BMP * bmp)
{
    printf("File: \n\t\
        Type: %X\n\t\
        Size: %d\n\t\
        Offset: %d\n\t\
        Width: %d\n\t\
        Height: %d\n\t\
        Image Size: %u\n\t\
        Bits/Pixel: %u\n\t\
        Compression: %u\n\t\
        Number of colors: %u\n\t\
        Important colors: %u\n", (*bmp).header.type, (*bmp).header.size, (*bmp).header.offset,(*bmp).iheader.width,(*bmp).iheader.height,(*bmp).iheader.isize, (*bmp).iheader.bits, (*bmp).iheader.compression,(*bmp).iheader.colors,(*bmp).iheader.impcolors);
    return 0;
}

void parse_args(int argc, char **argv, Options * opts)
{
// Parse options
int c = 0;
while((c = getopt(argc, argv, "f:o:x:y:r:s:")) != -1)
    {
        switch(c)
        {
        case 'f':
            (*opts).inputfile = optarg;
            break;
        case 'o':
            (*opts).filename = optarg;
            break;
        case 'x':
            (*opts).flag++;
            (*opts).op = SHIFT;
            (*opts).func = shift;
            (*opts).v.x = atoi(optarg);
            break;
        case 'y':
            (*opts).flag++;
            (*opts).v.y = atoi(optarg);
            break;
        case 'r':
            (*opts).op = ROTATE;
            (*opts).func = rotate;
            (*opts).param = atof(optarg);
            break;
        case 's':
            (*opts).op = SCALE;
            (*opts).func = scale;
            (*opts).param = atof(optarg);
        default:
            break;
        }
    }

}

// Main
int main(int argc, char **argv)
{
    if(MPI_Init(&argc,&argv) != MPI_SUCCESS)
    {
        perror("Unable to initialize MPI\n");
        exit(1);
    }

    FILE * fp = 0;
    BMP bmp;
    Options opts;
    opts.op = NS;
    opts.filename = 0;
    opts.flag = 0;
    parse_args(argc, argv, &opts);
    if((fp = fopen(opts.inputfile,"rw")) == NULL)
    {
        usage();
        perror("Unable to open file");
        exit(1);
    }
    get_bmp_header(fp, &bmp);
//    print_bmp_attr(&bmp);
    int linebits = bmp.iheader.width * bmp.iheader.bits;
    int linebytes = ((linebits + 31)/32)*4;
    Pixel ** img = malloc(sizeof(Pixel*) * bmp.iheader.height);
    fseek(fp, bmp.header.offset, SEEK_SET);
    int row = 0;
    int byte = 0;
    int column = 0;
    //int rank, size;
    Pixel * current;
    for(row = bmp.iheader.height-1; row >= 0; row--)
    {
        current = (Pixel *) malloc(linebytes);
        column = 0;
        for(byte = 0; byte < linebytes; byte+=3)
        {
            fread(&current[column++],1,sizeof(Pixel),fp);
        }
        img[row] = current;
        fseek(fp, bmp.header.offset + (bmp.iheader.height-1-row) * linebytes, SEEK_SET);
    }
    fclose(fp);
    bmp.array = img;
    if(opts.op == NS)
        do_menu(&bmp,&opts);
    else if(opts.filename == 0)
        usage();
    else
    {
        switch(opts.op)
        {
            case SCALE:
                opts.func = scale;
              break;
            case SHIFT:
              if(opts.flag == 2)
                opts.func = shift;
              else
                  usage();
              break;
            case ROTATE:
              opts.func = rotate;
              break;
            case NS:
            default:
              usage();
              break;

        }
    do_op(&bmp, &opts);
    
    }
    free(img);
    MPI_Finalize();
    return 0;
}
