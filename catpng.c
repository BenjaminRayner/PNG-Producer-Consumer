#include <stdio.h>
#include <string.h>

#include "crc.h"
#include "zutil.h"
#include "cURL.c"

#define STRIPCOUNT 50
#define HEIGHT 6
#define WIDTH 400
#define INFLATEDSIZE HEIGHT*(WIDTH*4+1)

int catpng(unsigned char* inflatedData) {

    unsigned long len_def = 0;
    unsigned char gp_buf_def[1000000];

    /*Compress into gp_buf_def*/
    mem_def(gp_buf_def, &len_def, inflatedData, INFLATEDSIZE*STRIPCOUNT, Z_DEFAULT_COMPRESSION);
    unsigned char png[44+len_def+13];
    memcpy(&png[41], gp_buf_def, len_def);

    /*Signature*/
    png[0] = 0x89;
    png[1] = 0x50;
    png[2] = 0x4e;
    png[3] = 0x47;
    png[4] = 0x0d;
    png[5] = 0x0a;
    png[6] = 0x1a;
    png[7] = 0x0a;

    /*HDR Length*/
    png[8] = 0x00;
    png[9] = 0x00;
    png[10] = 0x00;
    png[11] = 0x0d;

    /*IHDR Type Code*/
    png[12] = 0x49;
    png[13] = 0x48;
    png[14] = 0x44;
    png[15] = 0x52;

    /*Width of image*/
    png[16] = 0x00;
    png[17] = 0x00;
    png[18] = 0x01;
    png[19] = 0x90;

    /*Height of image*/
    png[20] = 0x00;
    png[21] = 0x00;
    png[22] = 0x01;
    png[23] = 0x2c;

    /*IHDR Params*/
    png[24] = 0x08;
    png[25] = 0x06;
    png[26] = 0x00;
    png[27] = 0x00;
    png[28] = 0x00;

    /*IHDR CRC*/
    png[29] = 0xed;
    png[30] = 0xb7;
    png[31] = 0xe5;
    png[32] = 0xc2;

    /*Inserting IDAT length*/
    png[33] = len_def >> 24;
    png[34] = len_def >> 16;
    png[35] = len_def >> 8;
    png[36] = len_def;

    /*IDAT Type Code*/
    png[37] = 0x49;
    png[38] = 0x44;
    png[39] = 0x41;
    png[40] = 0x54;

    /*IDAT crc*/
    unsigned int IDAT_crc = crc(&png[37], len_def+4);
    png[len_def+41] = IDAT_crc >> 24;
    png[len_def+42] = IDAT_crc >> 16;
    png[len_def+43] = IDAT_crc >> 8;
    png[len_def+44] = IDAT_crc;

    /*Inserting IEND*/
    png[44+len_def+5] = 0x49;
    png[44+len_def+6] = 0x45;
    png[44+len_def+7] = 0x4e;
    png[44+len_def+8] = 0x44;
    png[44+len_def+9] = 0xae;
    png[44+len_def+10] = 0x42;
    png[44+len_def+11] = 0x60;
    png[44+len_def+12] = 0x82;
    png[44+len_def+1] = 0;
    png[44+len_def+2] = 0;
    png[44+len_def+3] = 0;
    png[44+len_def+4] = 0;

    FILE* catpng;
    catpng = fopen("all.png", "w");
    fwrite(&png, 1, sizeof(png), catpng);
    fclose(catpng);

    return 0;
}
