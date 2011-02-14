#include <stdio.h>

#include "png.h"

/* Write a png file */
void write_png(const char *file_name, unsigned char *data, int width, int height)
{
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep row_pointers[height];
    int i;

    /* Open the file */
    fp = fopen(file_name, "wb");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed to open '%s' for writing!\n", file_name);
        return;
    }

    /* Create and initialize the png_struct with the desired error handler
     * functions.  If you want to use the default stderr and longjump method,
     * you can supply NULL for the last three parameters.  We also check that
     * the library version is compatible with the one used at compile time,
     * in case we are using dynamically linked libraries.  REQUIRED.
     */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
        NULL, NULL, NULL);

    if (png_ptr == NULL)
    {
        fclose(fp);
        fprintf(stderr, "Failed to create write struct!\n");
        return;
    }

    /* Allocate/initialize the image information data.  REQUIRED */
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL)
    {
        fclose(fp);
        png_destroy_write_struct(&png_ptr,  png_infopp_NULL);
        fprintf(stderr, "Failed to create info struct!\n");
        return;
    }

    /* Set error handling.  REQUIRED if you aren't supplying your own
     * error handling functions in the png_create_write_struct() call.
     */
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        /* If we get here, we had a problem writing the file */
        fclose(fp);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fprintf(stderr, "Failed somewhere writing PNG file!\n");
        return;
    }

    /* Set up the output control if you are using standard C streams */
    png_init_io(png_ptr, fp);

    for (i = 0; i < height; i++)
    {
        row_pointers[i] = data + (3 * width * i);
    }
    
    png_set_rows(png_ptr, info_ptr, row_pointers);    
    
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    /* This is the easy way.  Use it if you already have all the
     * image info living in the structure.  You could "|" many
     * PNG_TRANSFORM flags into the png_transforms integer here.
     */
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, png_voidp_NULL);

    /* Clean up after the write, and free any memory allocated */
    png_destroy_write_struct(&png_ptr, &info_ptr);

    /* Close the file */
    fclose(fp);

    /* That's it */
}
