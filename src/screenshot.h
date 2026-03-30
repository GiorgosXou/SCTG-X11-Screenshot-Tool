#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <stdlib.h>
#include <zlib.h>
#include <string.h>
#include <stdio.h>
#include <jpeglib.h>
#include "notify.h"


#define MAX_JPEG_COM 65533
#define INTO_CLIPBOARD NULL

#if !defined(QUALITY)
    #define QUALITY 85
#endif

static XImage *image;
static XShmSegmentInfo shminfo;



static void store(char* path, unsigned char *jpeg_buf, unsigned long jpeg_size){
    FILE *fp = popen("xclip -selection clipboard -t image/png", "w"); // png... well that's weird
    CHECK_NOTIFY(fp, "Failed to open xclip stream");
    fwrite(jpeg_buf, 1, jpeg_size, fp);
    pclose(fp);

    if (path != INTO_CLIPBOARD){
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        struct tm tm = *localtime(&ts.tv_sec);
        char* filepath = (char*)malloc(strlen(path) + 50);

        sprintf(filepath, "%s/sctg_%d-%02d-%02d %02d:%02d:%02d.%ld.jpg",
            path,
            tm.tm_year + 1900,
            tm.tm_mon  + 1   ,
            tm.tm_mday ,
            tm.tm_hour ,
            tm.tm_min  ,
            tm.tm_sec  ,
            ts.tv_nsec / 10000
        );
        fp = fopen(filepath, "wb");
        CHECK_NOTIFY(fp, "Failed to open file");
        fwrite(jpeg_buf, 1, jpeg_size, fp);
        fclose(fp);
        free(filepath);
    }
}


static int save(char* path, unsigned char* text, XImage *image, int x, int y, int width, int height) {
    unsigned char *jpeg_buf = NULL;
    unsigned long jpeg_size = 0;

    if (!width)  width  = image->width;
    if (!height) height = image->height;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // jpeg_stdio_dest(&cinfo, fp);
    jpeg_mem_dest(&cinfo, &jpeg_buf, &jpeg_size);

    cinfo.image_width = width;
    cinfo.image_height = height;

    // use 4-byte BGRX directly
    cinfo.input_components = 4;
    cinfo.in_color_space = JCS_EXT_BGRX;

    // Builds custom Huffman tables based on frequency of values ???
    cinfo.optimize_coding = TRUE;

    jpeg_set_defaults(&cinfo);
    jpeg_simple_progression(&cinfo);
    jpeg_set_quality(&cinfo, QUALITY , TRUE);

    // Example: Adding a comment
    jpeg_start_compress(&cinfo, TRUE);

    // Write the comment. JPEG_COM is the marker type.
    if (text){
        unsigned int text_size = strlen((char*)text);
        if (text_size > MAX_JPEG_COM)
            text_size = MAX_JPEG_COM;

        jpeg_write_marker(&cinfo, JPEG_COM, (JOCTET *)text, text_size);
    }

    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW rowptr = (JSAMPROW)(
            image->data + 
            (y + cinfo.next_scanline) * image->bytes_per_line +
            (x * 4)
        );
        jpeg_write_scanlines(&cinfo, &rowptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    store(path, jpeg_buf, jpeg_size);
    free(jpeg_buf);
    return 0;
}


static void capture_image_clean(Display* display){
    // Detach the shared memory from the X server side. 
    // Tell the X server: "I'm done using this shared memory segment."
    XShmDetach(display, &shminfo);
    // Destory image
    XDestroyImage(image);
    // Detach the shared memory from my process (client side).
    shmdt(shminfo.shmaddr);
    // Mark the shared memory segment for deletion.
    shmctl(shminfo.shmid, IPC_RMID, 0);
}


static int capture_image_from(Display*  display){
    int screen  = DefaultScreen(display);
    Window root = RootWindow   (display, screen);
    int width   = DisplayWidth (display, screen);
    int height  = DisplayHeight(display, screen);


    image = XShmCreateImage(
        display, 
        DefaultVisual(display, screen), 
        DefaultDepth(display, screen), 
        ZPixmap, 
        NULL, 
        &shminfo, 
        width, height
    );

    if (!image) {
        printf("XShmCreateImage failed\n");
        return 1;
    }

    shminfo.shmid = shmget(
        IPC_PRIVATE,
        image->bytes_per_line * image->height,
        IPC_CREAT | 0777
    );

    if (shminfo.shmid < 0) {
        perror("shmget");
        return 1;
    }

    shminfo.shmaddr = (char*)shmat(shminfo.shmid, 0, 0);
    image->data = shminfo.shmaddr;
    shminfo.readOnly = False;

    if (!XShmAttach(display, &shminfo)) {
        printf("XShmAttach failed\n");
        return 1;
    }

    XSync(display, False);

    if (!XShmGetImage(display, root, image, 0, 0, AllPlanes)) {
        printf("XShmGetImage failed\n");
        return 1;
    }

    return 0;
}




// #include <png.h> 
// static int save(XImage* image, int width, int height){
//     FILE *fp = fopen("screen.png", "wb");
//     if (!fp) {
//         perror("fopen");
//         return 1;
//     }
//
//     if (!width ){width  = image->width ;}
//     if (!height){height = image->height;}
//
//     png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
//     if (!png_ptr) {
//         fprintf(stderr, "png_create_write_struct failed\n");
//         return 1;
//     }
//
//     png_infop info_ptr = png_create_info_struct(png_ptr);
//     if (!info_ptr) {
//         fprintf(stderr, "png_create_info_struct failed\n");
//         png_destroy_write_struct(&png_ptr, NULL);
//         return 1;
//     }
//
//     if (setjmp(png_jmpbuf(png_ptr))) {
//         fprintf(stderr, "Error during PNG creation\n");
//         png_destroy_write_struct(&png_ptr, &info_ptr);
//         fclose(fp);
//         return 1;
//     }
//
//     png_init_io   (png_ptr, fp);
//     png_set_IHDR  (png_ptr,
//         info_ptr,
//         width,
//         height,
//         8, // bits per channel
//         PNG_COLOR_TYPE_RGB,
//         PNG_INTERLACE_NONE,
//         PNG_COMPRESSION_TYPE_BASE,
//         PNG_FILTER_TYPE_BASE
//     );
//
//     // Write Text
//     png_text text_ptr[1];
//     text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
//     text_ptr[0].key = "Comment";
//     text_ptr[0].text = "Captured from X11 screen";
//     png_set_text(png_ptr, info_ptr, text_ptr, 1);
//
//     // Write PNG-info
//     png_write_info(png_ptr, info_ptr);
//
//     // // Compress image
//     png_set_filter(png_ptr, 0, PNG_ALL_FILTERS); // try all filters
//     png_set_compression_level(png_ptr, 7); // 9 is max but recomended 3-6 based on https://refspecs.linuxbase.org/LSB_5.0.0/LSB-TrialUse/LSB-TrialUse/libpng15-png-set-compression-level.html
//     png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);
//
//     // set BGR (Blue, Green, Red) & alpha. (we "ignore" alpha because XImage has RGBX ?)
//     png_set_bgr(png_ptr);
//     png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
//
//     unsigned char *data = (unsigned char*)image->data;
//
//     png_bytep rows[height];
//     for (int y = 0; y < height; y++) {
//         rows[y] = data + y * image->bytes_per_line;
//     }
//     // printf("%d ; %d ; %d\n", image->bytes_per_line, width*4, width);
//
//     // Write rows into image
//     png_write_image(png_ptr, rows);
//
//     // free stuff
//     png_destroy_write_struct(&png_ptr, &info_ptr);
//     fclose(fp);
//     return 0;
// }
