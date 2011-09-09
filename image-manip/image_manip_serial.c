#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include<string.h>
#include<math.h>
#define BM 0x4D42           // Value of a bitmap type
#define MAX_FILENAME 256   
#define PI 3.14159265
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
    char * filename;
    Vector v;
    double theta;
    double scalefactor;
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
{
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

// Performs an image shift
void shift(BMP * img, Vector * v, char * file)
{
    // Number of bits in a line
    int linebits = (*img).iheader.width * (*img).iheader.bits;
    // 4 byte alignment
    int linebytes = ((linebits + 31)/32)*4;
    
    // Allocate memory for a new pixel array
    Pixel ** newimg = malloc(sizeof(Pixel*) * (*img).iheader.height);

    // Accounting
    int row = 0;
    int p = 0;
    int xp, yp;
    Pixel * current;

    // Open the new file
    FILE * nf;
    nf = fopen(file,"w");

    // Make sure we are at the beginning
    fseek(nf,0,SEEK_SET);

    // Write the headers
    fwrite(&(*img).header,1,sizeof((*img).header),nf);
    fwrite(&(*img).iheader,1,sizeof((*img).iheader),nf);

    // For every row
    for(row = 0; row < (*img).iheader.height; row++)
    {
        // Get memory for this row
        current = (Pixel *) malloc(sizeof(Pixel)*linebytes);

        // Set the memory to 0 by default (black), this also pads zeroes
        memset(current,0,sizeof(current));

        // For every pixel in the row (3 bytes)
        for(p = 0; p < linebytes/3; p++)
        {
          // Calculate the new x,y
          xp = p + (*v).x;
          yp = row + (*v).y;
          // Make sure they are on the canvas
          if(xp >= 0 && xp < linebytes/3 && yp < (*img).iheader.height && yp >= 0)
          {
              current[p] = (*img).array[yp][xp];
          }
        }
        // Store this row
        newimg[row] = current;
    }   

    // Write the new image
    for(row = (*img).iheader.height-1; row >= 0; row--)
        fwrite(&newimg[row][0],1,linebytes,nf);
    fclose(nf);
    free(newimg);
}

// Scales an image up or down
void scale(BMP * img, double sf, char * file)
{
     // Number of bits in a line
    int linebits = (*img).iheader.width * (*img).iheader.bits;
    // 4 byte alignment
    int linebytes = ((linebits + 31)/32)*4;
    
    // Allocate memory for a new pixel array
    Pixel ** newimg = malloc(sizeof(Pixel*) * (*img).iheader.height);

    // Accounting
    int row = 0;
    int p = 0;
    int xp, yp;
    Pixel * current;

    // Open the new file
    FILE * nf;
    nf = fopen(file,"w");

    // Make sure we are at the beginning
    fseek(nf,0,SEEK_SET);

    // Write the headers
    fwrite(&(*img).header,1,sizeof((*img).header),nf);
    fwrite(&(*img).iheader,1,sizeof((*img).iheader),nf);

    // For every row
    for(row = 0; row < (*img).iheader.height; row++)
    {
        // Get memory for this row
        current = (Pixel *) malloc(sizeof(Pixel)*linebytes);

        // Set the memory to 0 by default (black), this also pads zeroes
        memset(current,0,sizeof(current));

        // For every pixel in the row (3 bytes)
        for(p = 0; p < linebytes/3; p++)
        {
          // Calculate the new x,y
          xp = (int)p*sf;
          yp = (int)row*sf;
          // Make sure they are on the canvas
          if(xp >= 0 && xp < linebytes/3 && yp < (*img).iheader.height && yp >= 0)
          {
              current[p] = (*img).array[yp][xp];
          }
        }
        // Store this row
        newimg[row] = current;
    }   

    // Write the new image
    for(row = (*img).iheader.height-1; row >= 0; row--)
        fwrite(&newimg[row][0],1,linebytes,nf);
    fclose(nf);
    free(newimg);

}

void rotate(BMP * img, double theta, char * file)
{
     // Number of bits in a line
    int linebits = (*img).iheader.width * (*img).iheader.bits;
    // 4 byte alignment
    int linebytes = ((linebits + 31)/32)*4;
    
    // Allocate memory for a new pixel array
    Pixel ** newimg = malloc(sizeof(Pixel*) * (*img).iheader.height);

    // Accounting
    int row = 0;
    int p = 0;
    int xp, yp;
    Pixel * current;

    // Open the new file
    FILE * nf;
    nf = fopen(file,"w");

    // Make sure we are at the beginning
    fseek(nf,0,SEEK_SET);

    // Write the headers
    fwrite(&(*img).header,1,sizeof((*img).header),nf);
    fwrite(&(*img).iheader,1,sizeof((*img).iheader),nf);

    // For every row
    for(row = 0; row < (*img).iheader.height; row++)
    {
        // Get memory for this row
        current = (Pixel *) malloc(sizeof(Pixel)*linebytes);

        // Set the memory to 0 by default (black), this also pads zeroes
        memset(current,0,sizeof(current));

        // For every pixel in the row (3 bytes)
        for(p = 0; p < linebytes/3; p++)
        {
          // Calculate the new x,y
          xp = (int)p*cos(theta*PI/180)+row*sin(theta*PI/180);
          yp = (int)-1*p*sin(theta*PI/180)+row*cos(theta*PI/180);
          // Make sure they are on the canvas
          if(xp >= 0 && xp < linebytes/3 && yp < (*img).iheader.height && yp >= 0)
          {
              // new image array[row][p] = old image array [yp][xp]
              current[p] = (*img).array[yp][xp];
          }
        }
        // Store this row
        newimg[row] = current;
    }   

    // Write the new image
    for(row = (*img).iheader.height-1; row >= 0; row--)
        fwrite(&newimg[row][0],1,linebytes,nf);
    fclose(nf);
    free(newimg);


}

// Program menu
int do_menu(BMP * img)
{
    fflush(stdout);
    printf("\
            Options\n\
            =======\n\
            1. Shift\n\
            2. Scale\n\
            3. Rotate\n\
            4. Exit\n\
            Enter a selection: ");
    int choice;
    double sf;
    double theta;
    char * name = (char *) malloc(sizeof(char) * MAX_FILENAME);
    struct Vector v;
    scanf("%d",&choice);
    if(choice == 1 || choice == 2 || choice == 3)
    {
        printf("Enter a filename for the new image: ");
        scanf("%s",name);
    }
    switch(choice)
    {
        case 1:
            printf("Enter integer delta x: ");
            scanf("%d",&v.x);
            printf("Enter integer delta y: ");
            scanf("%d",&v.y);
            shift(img,&v,name);
            break;
        case 2:
            printf("Enter a decimal scale factor: ");
            scanf("%lf",&sf);
            scale(img,sf,name);
            break;
        case 3:
            printf("Enter decimal degrees to rotate: ");
            scanf("%lf",&theta);
            rotate(img,theta,name);
            break;
        case 4:
            return 0;
            break;
        default: break;
    }
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
    int linebits = (*bmp).iheader.width * (*bmp).iheader.bits;
    int linebytes = ((linebits + 31)/32)*4;
    printf("File: \n\t\
        Linebytes: %d\n\t\
        Type: %X\n\t\
        Size: %d\n\t\
        Offset: %d\n\t\
        Size: %d\n\t\
        Res1: %d\n\t\
        Res2: %d\n\t\
        Width: %d\n\t\
        Height: %d\n\t\
        Image Size: %u\n\t\
        Xres: %u\n\t\
        Yres: %u\n\t\
        Planes: %u\n\t\
        Bits/Pixel: %u\n\t\
        Compression: %u\n\t\
        Number of colors: %u\n\t\
        Important colors: %u\n", linebytes,(*bmp).header.type, (*bmp).header.size, (*bmp).header.offset,(*bmp).iheader.size,(*bmp).header.reserved1,(*bmp).header.reserved2,(*bmp).iheader.width,(*bmp).iheader.height,(*bmp).iheader.isize, 
        (*bmp).iheader.xres, (*bmp).iheader.yres,(*bmp).iheader.planes,(*bmp).iheader.bits, (*bmp).iheader.compression,(*bmp).iheader.colors,(*bmp).iheader.impcolors);
    return 0;
}


// Main
int main(int argc, char **argv)
{
int c = 0;
char *file = 0;
FILE * fp = 0;
int flag = 0;
BMP bmp;
Options opts;
opts.op = NS;
opts.filename = 0;
// Parse options
while((c = getopt(argc, argv, "f:o:x:y:r:s:")) != -1)
    {
        switch(c)
        {
        case 'f':
            file = optarg;
            break;
        case 'o':
            opts.filename = optarg;
            break;
        case 'x':
            flag++;
            opts.op = SHIFT;
            opts.v.x = atoi(optarg);
            break;
        case 'y':
            flag++;
            opts.op = SHIFT;
            opts.v.y = atoi(optarg);
            break;
        case 'r':
            opts.op = ROTATE;
            opts.theta = atof(optarg);
            break;
        case 's':
            opts.op = SCALE;
            opts.scalefactor = atof(optarg);
        default:
            break;
        }
    }
if((fp = fopen(file,"rw")) == NULL)
{
    perror("Unable to open file");
    exit(1);
}

get_bmp_header(fp, &bmp);
print_bmp_attr(&bmp);
int linebits = bmp.iheader.width * bmp.iheader.bits;
int linebytes = ((linebits + 31)/32)*4;
Pixel ** img = malloc(sizeof(Pixel*) * bmp.iheader.height);
fseek(fp, bmp.header.offset, SEEK_SET);
int row = 0;
int byte = 0;
int column = 0;
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
do_menu(&bmp);
else if(opts.filename == 0)
    usage();
else
{
    switch(opts.op)
    {
        case SCALE:
          scale(&bmp,opts.scalefactor,opts.filename);
          break;
        case SHIFT:
          if(flag == 2)
              shift(&bmp,&opts.v,opts.filename);
          else
              usage();
          break;
        case ROTATE:
          rotate(&bmp,opts.theta,opts.filename);
          break;
        case NS:
        default:
          usage();
          break;

    }

}
free(img);
return 0;
}
