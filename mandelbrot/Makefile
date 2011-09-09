CC = mpicc
LDFLAGS = -lX11 -lm

mandelbrot: mandelbrot.c
	$(CC) $(LDFLAGS) mandelbrot.c -o mandelbrot

mandel_bitmap: mandel_bitmap.c
	$(CC) $(LDFLAGS) mandel_bitmap.c -o mandel_bitmap

image_manip_serial: image_manip_serial.c
	$(CC) $(LDFLAGS) image_manip_serial.c -o image_manip_serial

clean:
	rm *.o mandelbrot image_manip_serial mandel_bitmap
