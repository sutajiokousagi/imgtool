/**
 * $Id$
 * imgtool.cpp
 * All-purpose image render and capture tool
 * Copyright (C) 2007-2009 Chumby Industries. All rights reserved.
**/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/mman.h>

// libpng
#ifndef NO_PNG
#include <png.h>
#endif


// jpeg
#include <jpeglib.h>

#define VER_DATA	1, 22
#define VER_FMT		"%d.%02d"


// Global flags
#define RESIZE_ANY	0xfff	 // Mask to check for any resize bits
#define	RESIZE_STRETCH_X	0x01	// Stretch width to fit
#define RESIZE_STRETCH_Y	0x02	// Stretch height to fit
#define RESIZE_STRETCH_MAX	0x04	// Select larger of X and Y and stretch proportionally
#define	RESIZE_STRETCH_MIN	0x08	// Select smaller of X and Y and stretch proportionally - clipping will occur
#define RESIZE_SHRINK_X		0x10	// Reduce X if larger than screen
#define	RESIZE_SHRINK_Y		0x20	// Reduce Y if larger
#define	RESIZE_SHRINK_MAX	0x40	// Reduce proportionally larger of X and Y
#define	RESIZE_SHRINK_MIN	0x80	// Reduce proportionally smaller of X and Y - clipping will occur
#define	RESIZE_FIT_X		0x100	// Reduce or stretch X
#define	RESIZE_FIT_Y		0x200	// Reduce or stretch Y
#define	RESIZE_FIT_MAX		0x400	// Reduce or stretch larger
#define	RESIZE_FIT_MIN		0x800	// Reduce or stretch smaller - clipping will occur
#define X_STRETCH		0x01	// We are stretching X
#define Y_STRETCH		0x02
#define	X_SHRINK		0x04	// Shrinking X
#define Y_SHRINK		0x08

enum bit_format {
	BF_RGB565,
	BF_RGB888,
	BF_BGR565,
	BF_ARGB8888,
};
static const char *bit_format_names[] = {
	"rgb565",
	"rgb888",
	"bgr565",
	"argb8888",
	NULL
};

enum operation {
	OP_DRAW,
	OP_CAPTURE,
};

struct imgtool_conf {
	char filename[2048];
	char output[2048];
	double gamma;
	enum bit_format fmt;
	unsigned int width, height;
	int fb_num;
	enum operation op;

	int debug_level;

	/* Filename extension of output type */
	char output_format[16];

	/* Resizing parameters */
	int x_pct;
	int y_pct;
	unsigned int resize_options;
	int resize;

	/* I have no idea what these are for */
	char disp_x[100];
	char disp_y[100];

	/* JPEG settings */
	int jpeg_quality;

	/* BMP settings */
	int bmp_mode;
	int mirror_h;

	/* Fill settings */
	unsigned int fill_color;
};

static inline unsigned int BytesPerFBPixel(enum bit_format fmt)
{
	switch (fmt)
	{
		case BF_RGB565:
		case BF_BGR565:
		default:
			return 2;
		case BF_RGB888:
			return 3;
		case BF_ARGB8888:
			return 4;
	}
}

static enum bit_format BitFormatToEnum( const char *name )
{
	int n;
	for (n = 0; bit_format_names[n]; n++)
		if (!strcasecmp( name, bit_format_names[n] ))
			return (enum bit_format)n;
	printf( "Error: unrecognized name %s\n", name );
	printf( "Valid names are:\n" );
	for (n = 0; bit_format_names[n]; n++)
		printf( "  %s\n", bit_format_names[n] );
	return (enum bit_format)-1;
}


// Frame buffer is <x_size> x <y_size> 16bpp r5 g6 b5
// or <x_size> x <y_size> 24bpp r8 g8 b8
// or <x_size> x <y_size> 32bpp a8 r8 g8 b8

// Header on disk for BMP files
#if 0
#pragma pack(2)
typedef struct _BitmapInfoHeader {
// Offsets are from start of file
// All values are little-endian with low order byte coming first. Use GetWord or GetDword
	static unsigned short GetWord( const unsigned char *b ) { return b[0] | (b[1]<<8); }
	static void SetWord( unsigned char *pb, unsigned short w ) { pb[0] = w&0xff; pb[1] = (w&0xff00)>>8; }
	static unsigned long GetDword( const unsigned char *b ) { return b[0] | (b[1]<<8) | (b[2]<<16) | (b[3]<<24); }
	static void SetDword( unsigned char *pb, unsigned long dw ) { pb[0] = dw&0xff; pb[1] = (dw&0xff00)>>8; pb[2] = (dw&0xff0000)>>16; pb[3] = (dw&0xff000000)>>24; }
//14 0x0e 4 size of BITMAPINFOHEADER structure which includes the header size, normally 40
	unsigned char dwHeaderSize[4];
	unsigned long GetHeaderSize() const { return GetDword(dwHeaderSize); }
	void SetHeaderSize( unsigned long dw ) { SetDword(dwHeaderSize,dw); }
//18 0x12 4 image width in pixels
	unsigned char dwImageWidth[4];
	unsigned long GetImageWidth() const { return GetDword(dwImageWidth); }
	void SetImageWidth( unsigned long dw ) { SetDword(dwImageWidth,dw); }
//22 0x16 4 image height in pixels
	unsigned char dwImageHeight[4];
	unsigned long GetImageHeight() const { return GetDword(dwImageHeight); }
	void SetImageHeight( unsigned long dw ) { SetDword(dwImageHeight,dw); }
//26 0x1a 2 number of planes in the image, must be 1
	unsigned char wPlanes[2];
	unsigned short GetPlanes() const { return GetWord(wPlanes); }
	void SetPlanes( unsigned short w ) { SetWord(wPlanes,w); }
//28 0x1c 2 number of bits per pixel (1, 4, 8, or 24)
	unsigned char wBitsPerPixel[2];
	unsigned short GetBitsPerPixel() const { return GetWord(wBitsPerPixel); }
	void SetBitsPerPixel( unsigned short w ) { SetWord(wBitsPerPixel,w); }
//30 0x1e 4 compression type (0=none, 1=RLE-8, 2=RLE-4)
	unsigned char dwCompressionType[4];
	unsigned long GetCompressionType() const { return GetDword(dwCompressionType); }
	void SetCompressionType( unsigned long dw ) { SetDword(dwCompressionType,dw); }
//34 0x22 4 size of image data in bytes (including padding)
	unsigned char dwImageDataLength[4];
	unsigned long GetImageDataLength() const { return GetDword(dwImageDataLength); }
	void SetImageDataLength( unsigned long dw ) { SetDword(dwImageDataLength,dw); }
//38 0x26 4 horizontal resolution in pixels per meter (unreliable)
	unsigned char dwHorizontalPixPerMeter[4];
//42 0x2a 4 vertical resolution in pixels per meter (unreliable)
	unsigned char dwVerticalPixPerMeter[4];
//46 0x2e 4 number of colors in image, or zero
	unsigned char dwColors[4];
//50 0x32 4 number of important colors, or zero
	unsigned char dwImportantColors[4];
//54 0x36 0-16 undefined
//70 0x46 usual start of data
} BitmapInfoHeader_t;

// What to read from start of file to get the image data offset. This is always followed
// immediately by the bitmap info header (Windows) or bitmap core header (OS/2)
typedef struct _BMPFileHeader {
// All values are little-endian with low order byte coming first. Use GetWord or GetDword
//0  0x00 2  signature, must be 'BM'
	unsigned char wSig[2];
	bool IsBMP() const { return wSig[0]=='B' && wSig[1]=='M'; }
//2  0x02 4 size of BMP file in bytes (unreliable)
	unsigned char dwFilesize[4];
	void SetFilesize(unsigned long dw) { BitmapInfoHeader_t::SetDword(dwFilesize,dw); }
//6  0x06 2 reserved, must be zero
	unsigned char _rsvd1[2];
//8  0x08 2 reserved, must be zero
	unsigned char _rsvd2[2];
//10 0x0a 4 offset to start of image data in bytes
	unsigned char dwImageDataOffset[4];
	unsigned long GetImageDataOffset() const { return BitmapInfoHeader_t::GetDword(dwImageDataOffset); }
	void SetImageDataOffset(unsigned long dw) { BitmapInfoHeader_t::SetDword(dwImageDataOffset,dw); }
} BMPFileHeader_t;

// Structure containing the minimum we should find at start of file
typedef struct _BMPHeader {
	BMPFileHeader_t fileHeader;
	BitmapInfoHeader_t infoHeader;
} BMPHeader_t;
#pragma pack()
#endif

// Open output file in specified format
static int OpenOutput(int width, int height, char *dest, uint8_t isOutput)
{
	int openFlags = isOutput ? O_RDWR | O_CREAT | O_TRUNC : O_RDONLY;
	fprintf( stderr, "Opening %s for %s\n", isOutput ? "output" : "input", dest);
	return open( dest, openFlags, 0644 );
}


// Generic write to frame buffer.
// Returns bytes written
static int WriteFB( int fb_handle, void *data, int length )
{
	unsigned char *_data = (unsigned char *)data;
	return write( fb_handle, _data, length );
}

// Seed pixel display vector based on percentage
static void SetDisplayVector( int pct, char *v )
{
	memset( v, 0, 100 );
	// Eliminate 0 and 100% cases
	if (pct == 0) return;
	if (pct >= 100)
	{
		memset( v, 1, 100 );
		return;
	}
	// Try to distribute as evenly as possible
	int skip = 100 / pct;
	int n;
	int numSet = 0;
	for (n = 0; n < 100; n += skip)
	{
		v[n] = 1;
		numSet++;
		// Adjust skip factor
		if (numSet < pct)
		{
			skip = (100-n-1) / (pct-numSet);
		}
		if (skip <= 0)
		{
			skip = 1;
		}
	}
	// If any left over, stick them in anywhere
	char *firstUnset = (char*)memchr( v, 0, 100 );
	while (pct - numSet > 0 && firstUnset)
	{
		*firstUnset = 1;
		numSet++;
		firstUnset = (char*)memrchr( v, 0, 100 );
	}
}

static void DumpVector( const char *label, char *v )
{
	char mask[128];
	memset( mask, '.', 100 );
	int n;
	for (n = 0; n < 100; n++)
	{
		if (v[n]) mask[n] = 'X';
	}
	mask[100] = '\0';
	fprintf( stderr, "%s: %s\n", label, mask );
}

// Adjust output size and width based on resize flags. true if resizing
static int AdjustOutputSize( unsigned int *width, unsigned int *height, struct imgtool_conf *conf )
{
	conf->x_pct = 100;
	conf->y_pct = 100;
	conf->resize = 0;
	if (!(conf->resize_options & RESIZE_ANY))
	{
		return 0;
	}
	// Reject bogus proportions
	if (*width <= 0 || *height <= 0)
	{
		return 0;
	}
	if ((RESIZE_SHRINK_X & conf->resize_options) && *width > conf->width)
	{
		conf->x_pct = (conf->width*100L / ((unsigned long)*width));
		*width = conf->width;
	}
	if ((RESIZE_SHRINK_Y & conf->resize_options) && *height > conf->height)
	{
		conf->y_pct = (conf->height*100L / ((unsigned long)*height));
		*height = conf->height;
	}
	if ((RESIZE_SHRINK_MAX & conf->resize_options) && (*width > conf->width || *height > conf->height))
	{
		conf->x_pct = (conf->width*100L / ((unsigned long)*width));
		conf->y_pct = (conf->height*100L / ((unsigned long)*height));
		if (conf->y_pct < conf->x_pct)
		{
			*height = conf->height;
			*width = (conf->width*100L * *width) / 100L;
		}
		else
		{
			if (*width > conf->width)
			{
				*width = conf->width;
			}
			*height = (conf->height*100L * *height) / 100L;
		}

	}
	if (conf->x_pct < 100) conf->resize |= X_SHRINK;
	if (conf->x_pct > 100) conf->resize |= X_STRETCH;
	if (conf->y_pct < 100) conf->resize |= Y_SHRINK;
	if (conf->y_pct > 100) conf->resize |= Y_STRETCH;
	// Seed display vectors. If we wanted non-proportionate scaling we'd use separate scale factors
	SetDisplayVector( conf->x_pct, conf->disp_x );
	SetDisplayVector( conf->y_pct, conf->disp_y );
	if (conf->debug_level)
	{
		if (conf->x_pct!=100) DumpVector( "X vector", conf->disp_x );
		if (conf->y_pct!=100) DumpVector( "Y vector", conf->disp_y );
	}
	return (conf->resize != 0);
}


#ifndef NO_PNG



///////////////////////// png ////////////////////////

void user_error_fn(png_structp png_ptr,
	png_const_charp error_msg)
{
	fprintf( stderr, "libpng fatal error: %s\n", error_msg );
	exit(1);
}

#define NON_PROGRESSIVE

#ifndef NON_PROGRESSIVE

info_callback(png_structp png_ptr, png_infop info)
{
/* do any setup here, including setting any of the transformations
 * mentioned in the Reading PNG files section.  For now, you _must_
 * call either png_start_read_image() or png_read_update_info()
 * after all the transformations are set (even if you don't set
 * any).  You may start getting rows before png_process_data()
 * returns, so this is your last chance to prepare for that.
 */
}

row_callback(png_structp png_ptr, png_bytep new_row,
   png_uint_32 row_num, int pass)
{
/*
 * This function is called for every row in the image.  If the
 * image is interlaced, and you turned on the interlace handler,
 * this function will be called for every row in every pass.
 *
 * In this function you will receive a pointer to new row data from
 * libpng called new_row that is to replace a corresponding row (of
 * the same data format) in a buffer allocated by your application.
 *
 * The new row data pointer new_row may be NULL, indicating there is
 * no new data to be replaced (in cases of interlace loading).
 *
 * If new_row is not NULL then you need to call
 * png_progressive_combine_row() to replace the corresponding row as
 * shown below:
 */
   /* Check if row_num is in bounds. */
   if((row_num >= 0) && (row_num < height) && row_num < conf->height)
   {
     /* Get pointer to corresponding row in our
      * PNG read buffer.
      */
     png_bytep old_row = ((png_bytep *)our_data)[row_num];

     /* If both rows are allocated then copy the new row
      * data to the corresponding row data.
      */
     if((old_row != NULL) && (new_row != NULL))
     png_progressive_combine_row(png_ptr, old_row, new_row);
   }
/*
 * The rows and passes are called in order, so you don't really
 * need the row_num and pass, but I'm supplying them because it
 * may make your life easier.
 *
 * For the non-NULL rows of interlaced images, you must call
 * png_progressive_combine_row() passing in the new row and the
 * old row, as demonstrated above.  You can call this function for
 * NULL rows (it will just return) and for non-interlaced images
 * (it just does the png_memcpy for you) if it will make the code
 * easier.  Thus, you can just do this for all cases:
 */

   png_progressive_combine_row(png_ptr, old_row, new_row);

/* where old_row is what was displayed for previous rows.  Note
 * that the first pass (pass == 0 really) will completely cover
 * the old row, so the rows do not have to be initialized.  After
 * the first pass (and only for interlaced images), you will have
 * to pass the current row as new_row, and the function will combine
 * the old row and the new row.
 */
}

void end_callback(png_structp png_ptr, png_infop info)
{
/* this function is called when the whole image has been read,
 * including any chunks after the image (up to and including
 * the IEND).  You will usually have the same info chunk as you
 * had in the header, although some data may have been added
 * to the comments and time fields.
 *
 * Most people won't do much here, perhaps setting a flag that
 * marks the image as finished.
 */
}

#endif

static void user_warning_fn(png_structp png_ptr,
        png_const_charp warning_msg)
{
	fprintf( stderr, "libpng warning: %s\n", warning_msg );
}

#endif

static void ARGB8888toRGB565( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, unsigned int nColumns )
{
	// Source is in R8G8B8 (alpha should be stripped)
	// Destination is in R5G6B5 or B5G6R5
	int bytes_per_pixel = 4;
	// Fill to end of row
	memset( dest, 0, BytesPerFBPixel(conf->fmt)*conf->width );
	int nStart = 0;
	// Check for clipping required
	if (nColumns > conf->width && conf->resize == 0)
	{
		nColumns = conf->width;
	}
	unsigned int col, dcol;
	int ri = 2, gi = 1, bi = 0;
	if (conf->fmt == BF_BGR565)
	{
		ri = 0;
		//gi = 1;
		bi = 2;
	}
	src += (bytes_per_pixel * nStart);
	for (col = nStart, dcol = 0; dcol < conf->width && col < nColumns; col++)
	{
		// Check horizontal display vector if resizing
		if ((conf->resize & X_SHRINK) == 0 || conf->disp_x[col%100])
		{
			// src: rrrrrrrr gggggggg bbbbbbbb aaaaaaaa
			// dst: rrrrrggg gggbbbbb
			unsigned char r, g, b;
			r = src[ri] >> 3;
			g = src[gi] >> 2;
			b = src[bi] >> 3;

			int pixIndex = conf->mirror_h ? 2 * (conf->width - dcol) : 2 * dcol;
			// hg - not sure why these appear to be in little-endian order?
			dest[pixIndex + 1] = (r << 3) | (g >> 3);
			dest[pixIndex + 0] = (g << 5) | b;
			dcol++;
		}

		src += bytes_per_pixel;
	}
}

// line copy for rgb888
static void ARGB8888toRGB888( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, unsigned int nColumns )
{
	// Source is in R8G8B8 (alpha should be stripped)
	// Destination is in R8G8B8
	int bytes_per_pixel = 4;
	// Fill to end of row
	memset( dest, 0, 3*conf->width );
	int nStart = 0;
	// Check for clipping required
	if (nColumns > conf->width && conf->resize == 0)
	{
		nColumns = conf->width;
	}
	unsigned int col, dcol;
	src += (bytes_per_pixel * nStart);
	for (col = nStart, dcol = 0; dcol < conf->width && col < nColumns; col++)
	{
		// Check horizontal display vector if resizing
		if ((conf->resize & X_SHRINK) == 0 || conf->disp_x[col%100])
		{
			// src: rrrrrrrr gggggggg bbbbbbbb aaaaaaaa
			// dst: rrrrrrrr gggggggg bbbbbbbb
			unsigned char r, g, b;
			r = src[2];
			g = src[1];
			b = src[0];
			int pixIndex = conf->mirror_h ? 3 * (conf->width - dcol) : 3 * dcol;
			dest[pixIndex + 2] = b;
			dest[pixIndex + 1] = g;
			dest[pixIndex + 0] = r;
			dcol++;
		}

		src += bytes_per_pixel;
	}
}

// line copy for argb888
static void ARGB8888toARGB8888( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, unsigned int nColumns )
{
	// Source is in A8R8G8B8
	// Destination is in A8R8G8B8
	int bytes_per_pixel = 4;
	// Fill to end of row
	memset( dest, 0, 4*conf->width );
	int nStart = 0;
	// Check for clipping required
	if (nColumns > conf->width && conf->resize == 0)
	{
		nColumns = conf->width;
	}
	unsigned int col, dcol;
	src += (bytes_per_pixel * nStart);
	for (col = nStart, dcol = 0; dcol < conf->width && col < nColumns; col++)
	{
		// Check horizontal display vector if resizing
		if ((conf->resize & X_SHRINK) == 0 || conf->disp_x[col%100])
		{
			// src: rrrrrrrr gggggggg bbbbbbbb aaaaaaaa
			// dst: rrrrrrrr gggggggg bbbbbbbb aaaaaaaa
			*((unsigned int*)dest) = *((unsigned int*)src) | 0xff000000L;
			dest += 4;
			dcol++;
		}

		src += bytes_per_pixel;
	}
}

// bitmap-format agnostic form of line copy
static void ARGB8888toFB( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, int nColumns )
{
	switch (conf->fmt)
	{
		case BF_RGB565:
		case BF_BGR565:
			ARGB8888toRGB565( conf, dest, src, nColumns );
			break;
		case BF_RGB888:
			ARGB8888toRGB888( conf, dest, src, nColumns );
			break;
		case BF_ARGB8888:
			ARGB8888toARGB8888( conf, dest, src, nColumns );
			break;
		default:
			printf( "%s() in %s:%d - unsupported bit format %d\n", __FUNCTION__, __FILE__, __LINE__, conf->fmt );
			exit( -1 );
	}
}

#ifndef NO_PNG
static void RGB8toR5G6B5( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, unsigned int nColumns, int nPalette, png_colorp pal )
{
	// Source is in R8G8B8 (alpha should be stripped)
	// Destination is in R5G6B5
	int bytes_per_pixel = nPalette > 0 ? 1 : 3;
	int nStart = 0;

	unsigned int col, dcol;
	unsigned char r, g, b;
	unsigned char *high = &r;
	unsigned char *mid = &g;
	unsigned char *low = &b;
	int dest_hi = 1;
	int dest_lo = 0;
	if (conf->fmt == BF_BGR565) {
		high = &b;
		low = &r;
		dest_hi = 0;
		dest_lo = 1;
	}

	src += (bytes_per_pixel * nStart);
	// Fill to end of row
	memset(dest, 0, BytesPerFBPixel(conf->fmt)*conf->width);

	// Check for clipping required
	if (nColumns > conf->width && conf->resize == 0)
		nColumns = conf->width;

	for (col = nStart, dcol = 0; dcol < conf->width && col < nColumns; col++) {

		// Check horizontal display vector if resizing
		if ((conf->resize & X_SHRINK) == 0 || conf->disp_x[col%100]) {
			int pixIndex = conf->mirror_h ? 2 * (conf->width - dcol) : dcol * 2;

			// src: rrrrrrrr gggggggg bbbbbbbb aaaaaaaa
			// dst: rrrrrggg gggbbbbb
			if (nPalette) {
				int pi = *src % nPalette;
				r = pal[pi].red >> 3;
				g = pal[pi].green >> 2;
				b = pal[pi].blue >> 3;
			}

			else {
				r = src[0] >> 3;
				g = src[1] >> 2;
				b = src[2] >> 3;
			}

			// hg - not sure why these appear to be in little-endian order?
			dest[pixIndex + dest_hi] = (*high << 3) | (*mid >> 3);
			dest[pixIndex + dest_lo] = (*mid << 5) | *low;
			dcol++;
		}

		src += bytes_per_pixel;
	}
}

static void RGB8toR8G8B8( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, unsigned int nColumns, int nPalette, png_colorp pal )
{
	// Source is in R8G8B8 (alpha should be stripped) or indexed
	// Destination is in R8G8B8
	int bytes_per_pixel = nPalette > 0 ? 1 : 3;
	// Fill to end of row
	memset( dest, 0, BytesPerFBPixel(conf->fmt)*conf->width );
	int nStart = 0;
	// Check for clipping required
	if (nColumns > conf->width && conf->resize == 0)
	{
		nColumns = conf->width;
	}
	unsigned int col, dcol;
	src += (bytes_per_pixel * nStart);
	for (col = nStart, dcol = 0; dcol < conf->width && col < nColumns; col++)
	{
		// Check horizontal display vector if resizing
		if ((conf->resize & X_SHRINK) == 0 || conf->disp_x[col%100])
		{
			// src: rrrrrrrr gggggggg bbbbbbbb aaaaaaaa
			// dst: rrrrrrrr gggggggg bbbbbbbb
			unsigned char r, g, b;
			if (nPalette)
			{
				int pi = *src % nPalette;
				r = pal[pi].red;
				g = pal[pi].green;
				b = pal[pi].blue;
			}
			else
			{
				r = src[0];
				g = src[1];
				b = src[2];
			}
			int pixIndex = conf->mirror_h ? 3 * (conf->width - dcol) : dcol * 3;
			dest[pixIndex + 2] = r;
			dest[pixIndex + 1] = g;
			dest[pixIndex + 0] = b;
			dcol++;
		}

		src += bytes_per_pixel;
	}
}

static void RGB8toARGB8888( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, int unsigned nColumns, int nPalette, png_colorp pal )
{
	// Source is in R8G8B8 (alpha should be stripped) or indexed
	// Destination is in R8G8B8
	int bytes_per_pixel = nPalette > 0 ? 1 : 3;
	// Fill to end of row
	memset( dest, 0, BytesPerFBPixel(conf->fmt)*conf->width );
	int nStart = 0;
	// Check for clipping required
	if (nColumns > conf->width && conf->resize == 0)
	{
		nColumns = conf->width;
	}
	unsigned int col, dcol;
	src += (bytes_per_pixel * nStart);
	for (col = nStart, dcol = 0; dcol < conf->width && col < nColumns; col++)
	{
		// Check horizontal display vector if resizing
		if ((conf->resize & X_SHRINK) == 0 || conf->disp_x[col%100])
		{
			// src: rrrrrrrr gggggggg bbbbbbbb aaaaaaaa
			// dst: rrrrrrrr gggggggg bbbbbbbb
			unsigned char r, g, b, a;
			if (nPalette)
			{
				int pi = *src % nPalette;
				r = pal[pi].red;
				g = pal[pi].green;
				b = pal[pi].blue;
				a = 255;
			}
			else
			{
				r = src[0];
				g = src[1];
				b = src[2];
				a = src[3];
			}
			int pixIndex = conf->mirror_h ? 4 * (conf->width - dcol) : dcol * 4;
			dest[pixIndex + 3] = a;
			dest[pixIndex + 2] = r;
			dest[pixIndex + 1] = g;
			dest[pixIndex + 0] = b;
			dcol++;
		}

		src += bytes_per_pixel;
	}
}

// Bit format-agnostic form
static void RGB8toFBPng( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, int nColumns, int nPalette, png_colorp pal )
{
	switch (conf->fmt)
	{
		case BF_RGB565:
		case BF_BGR565:
			RGB8toR5G6B5( conf, dest, src, nColumns, nPalette, pal );
			break;
		case BF_RGB888:
			RGB8toR8G8B8( conf, dest, src, nColumns, nPalette, pal );
			break;
		case BF_ARGB8888:
			RGB8toARGB8888( conf, dest, src, nColumns, nPalette, pal );
			break;
		default:
			printf( "%s() in %s:%d - unsupported bit format %d\n", __FUNCTION__, __FILE__, __LINE__, conf->fmt );
			exit( -1 );
	}
}
#endif

// Get a single pixel row capture from the frame buffer and convert to RGB888
static void RGB565toRGB888( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, unsigned int nColumns )
{
	// Source is in R5G6B5 (alpha should be stripped)
	// Destination is in R8G8B8
	int bytes_per_pixel = 3;
	// Fill to end of row
	memset( dest, 0, bytes_per_pixel*conf->width );
	int nStart = 0;
	// Check for clipping required
	if (nColumns > conf->width)
	{
		nColumns = conf->width;
	}
	unsigned int col, dcol;
	src += (bytes_per_pixel * nStart);
	for (col = nStart, dcol = 0; dcol < conf->width && col < nColumns; col++)
	{
		// src: rrrrrggg gggbbbbb
		// dst: rrrrrrrr gggggggg bbbbbbbb
		unsigned char r, g, b;
		r = src[1] >> 3;
		g = (src[1] << 3) | (src[0] >> 5);
		b = src[0] & 0x1f;

		// hg - not sure why these appear to be in little-endian order?
		// Scale values from  5/6 bits to 8 bits
		dest[0] = r << 3; //(r << 3) | (g >> 3);
		dest[1] = g << 2;
		dest[2] = b << 3; //(g << 5) | b;
		dest += bytes_per_pixel;
		dcol++;

		src += 2;
	}
}

// Get a single pixel row capture from the frame buffer and convert to RGB888
static void RGB888toRGB888( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, unsigned int nColumns )
{
	// Source is in R8G8B8 (alpha should be stripped)
	// Destination is in R8G8B8
	int bytes_per_pixel = 3;
	// Fill to end of row
	memset( dest, 0, bytes_per_pixel*conf->width );
	int nStart = 0;
	// Check for clipping required
	if (nColumns > conf->width)
	{
		nColumns = conf->width;
	}
	unsigned int col, dcol;
	src += (bytes_per_pixel * nStart);
	for (col = nStart, dcol = 0; dcol < conf->width && col < nColumns; col++)
	{
		// src: rrrrrrrr gggggggg bbbbbbbb
		// dst: rrrrrrrr gggggggg bbbbbbbb
		unsigned char r, g, b;
		r = src[2];
		g = src[1];
		b = src[0];

		// Scale values from  5/6 bits to 8 bits
		dest[0] = r;
		dest[1] = g;
		dest[2] = b;
		dest += bytes_per_pixel;
		dcol++;

		src += 3;
	}
}

// Bitformat-agnostic method to get a single pixel row as RGB888
static void FBtoRGB888( struct imgtool_conf *conf, unsigned char *dest, unsigned const char *src, int nColumns )
{
	switch (conf->fmt)
	{
		case BF_RGB565:
		case BF_BGR565:
			RGB565toRGB888( conf, dest, src, nColumns );
			break;
		case BF_RGB888:
			RGB888toRGB888( conf, dest, src, nColumns );
			break;
		case BF_ARGB8888:
			ARGB8888toRGB888( conf, dest, src, nColumns );
			break;
		default:
			printf( "%s() in %s:%d - unsupported bit format %d\n", __FUNCTION__, __FILE__, __LINE__, conf->fmt );
			exit( -1 );
	}
}

// Dump a display row in hex
static void HexDump( int rowNum, const char *rowId, unsigned char *rowBuff, int rowBytes )
{
	int n;
	// Space for 4 hex bytes per pixel, 2 characters per hex byte, plus leadin
	char outBuff[320*3*2+128];
	int endLine;
	if (rowBytes > 320*4)
	{
		rowBytes = 320*4;
	}
	endLine = sprintf( outBuff, "%s[%3d] ", rowId, rowNum );
	for (n = 0; n < rowBytes; n++)
	{
		endLine += sprintf( &outBuff[endLine], "%02x", rowBuff[n] );
	}
	puts( outBuff );
}

#ifndef NO_PNG

static int ShowPng(struct imgtool_conf *conf)
{
   png_structp png_ptr;
   png_infop info_ptr;
   unsigned int sig_read = 0;
   png_uint_32 width, height;
   int bit_depth, color_type, interlace_type;
   FILE *fp;

   if ((fp = fopen(conf->filename, "rb")) == NULL)
	{
		fprintf( stderr, "Error: unable to open %s\n", conf->filename );
		return -1;
	}

   /* Create and initialize the png_struct with the desired error handler
    * functions.  If you want to use the default stderr and longjump method,
    * you can supply NULL for the last three parameters.  We also supply the
    * the compiler header file version, so that we know if the application
    * was compiled with a compatible version of the library.  REQUIRED
    */
  void * user_error_ptr = 0;
   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
      user_error_ptr, user_error_fn, user_warning_fn);

   if (png_ptr == NULL)
   {
      fclose(fp);
      fprintf( stderr, "Failed to create read struct\n" );
      return -1;
   }

   /* Allocate/initialize the memory for image information.  REQUIRED. */
   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fclose(fp);
      png_destroy_read_struct(&png_ptr, png_infopp_NULL, png_infopp_NULL);
      fprintf( stderr, "Failed to create info struct\n" );
      return -1;
   }

   /* Set up the input control if you are using standard C streams */
   png_init_io(png_ptr, fp);

   /* If we have already read some of the signature */
   png_set_sig_bytes(png_ptr, sig_read);

   /* The call to png_read_info() gives us all of the information from the
    * PNG file before the first IDAT (image data chunk).  REQUIRED
    */
   png_read_info(png_ptr, info_ptr);

	//png_uint32 width, height;
	//int bit_depth, color_type, interlace_type;
   png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
       &interlace_type, int_p_NULL, int_p_NULL);

	fprintf( stderr, "Image %dx%d %dbpp color type=%d interlace=%d\n", (int)width, (int)height, (int)bit_depth, color_type, interlace_type );

   /* tell libpng to strip 16 bit/color files down to 8 bits/color */
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

   png_colorp palette = NULL;
   int num_palette = 0;
   if (color_type == PNG_COLOR_TYPE_PALETTE)
   {
		//png_set_palette_to_rgb(png_ptr);
		png_get_PLTE(png_ptr, info_ptr, &palette,
                            &num_palette);
   }

	// Also strip alpha
	if (color_type & PNG_COLOR_MASK_ALPHA)
		png_set_strip_alpha(png_ptr);

	// More stuff here suchas setting alpha and background


   /* Tell libpng to handle the gamma conversion for you.  The final call
    * is a good guess for PC generated images, but it should be configurable
    * by the user at run time by the user.  It is strongly suggested that
    * your application support gamma correction.
    */

   int intent;

   if (png_get_sRGB(png_ptr, info_ptr, &intent))
      png_set_gamma(png_ptr, conf->gamma, 0.45455);
   else
   {
      double image_gamma;
      if (png_get_gAMA(png_ptr, info_ptr, &image_gamma))
         png_set_gamma(png_ptr, conf->gamma, image_gamma);
      else
         png_set_gamma(png_ptr, conf->gamma, 0.45455);
   }



	// More bit diddling stuff that might be useful



#define NON_PROGRESSIVE
#define SUCK_IN_ONE_GO

#ifndef NON_PROGRESSIVE
   /* Turn on interlace handling.  REQUIRED if you are not using
    * png_read_image().  To see how to handle interlacing passes,
    * see the png_read_row() method below:
    */
	int
   number_passes = png_set_interlace_handling(png_ptr);
#endif

   /* Optional call to gamma correct and add the background to the palette
    * and update info structure.  REQUIRED if you are expecting libpng to
    * update the palette for you (ie you selected such a transform above).
    */
   png_read_update_info(png_ptr, info_ptr);

#ifdef NON_PROGRESSIVE
   /* Allocate the memory to hold the image using the fields of info_ptr. */

	fprintf( stderr, "non-progressive: allocating %d row buffers\n", (int)height );

   /* The easiest way to read the image: */
	png_uint_32 row;
	png_bytep *row_pointers = (png_bytep*)png_malloc(png_ptr, height*sizeof(png_bytep));
	int memory_failed = (row_pointers == NULL);
	if (memory_failed)
	{
		fprintf( stderr, "Error: failed to allocate %d row pointers\n", (int)height );
	}
	else
	{

		size_t row_bytes = png_get_rowbytes( png_ptr, info_ptr );
		fprintf( stderr, "allocating %d bytes per row\n", row_bytes );
		for (row = 0; row < height; row++)
		{
			if (!(row_pointers[row] = (png_byte*)png_malloc(png_ptr, row_bytes)))
			{
				fprintf( stderr, "Error: memory allocate failed on row %d\n", (int)row );
				memory_failed = 1;
				break;
			}
			//else fprintf( stderr, "row_pointers[%d] = %lx\n", row, (unsigned long)row_pointers[row] );
		}
	}

	if (memory_failed)
	{
		if (row_pointers != NULL)
		{
			png_free( png_ptr, row_pointers );
		}
		/* Free all of the memory associated with the png_ptr and info_ptr */
		// Presumably this takes care of the memory we allocated with png_malloc() ?
		// Apparently not!
		png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);

		fclose( fp );
		return -1;
	}

#ifdef SUCK_IN_ONE_GO
	fprintf( stderr, "Reading entire set of rows in one pass: base pointer = %lx\n", (unsigned long)row_pointers );
   png_read_image(png_ptr, row_pointers);

	// Determine resizing
	unsigned int scaledWidth, scaledHeight;
	scaledWidth = width;
	scaledHeight = height;
	if (AdjustOutputSize( &scaledWidth, &scaledHeight, conf))
	{
		fprintf( stderr, "Scaling from %dX%d to %dX%d (%d%%/%d%%)\n",
			(int)width, (int)height,
			scaledWidth, scaledHeight,
			conf->x_pct, conf->y_pct );
	}

   // Convert rows from R8G8B8 to frame buffer format
   int fb = OpenOutput(scaledWidth, scaledHeight, conf->output, 1);
   if (fb > 0)
   {
		unsigned char *fbRow = (unsigned char *)alloca( BytesPerFBPixel(conf->fmt)*conf->width );
		if (!fbRow)
		{
			fprintf( stderr, "malloc() failed, errno=%d (%s)\n", errno, strerror(errno) );
			exit( 1 );
		}
		png_uint_32 maxRow = height-1;
		png_uint_32 minRow = 0;
		png_uint_32 fillRows = 0;
		if (!conf->resize)
		{
			// Clip oversized rows
			if (maxRow < height-1)
			{
				minRow = height-conf->height;
			}
		}
		unsigned int dispRow = minRow;
		// Fill empty space
		if (maxRow - minRow < conf->height-1)
		{
			fillRows = conf->height - 1 - maxRow - minRow;
		}
		fprintf( stderr, "Displaying rows from %d to %d inclusive\n", (int)minRow, (int)maxRow );
		for (row = minRow; row<=maxRow && dispRow < conf->height; row++)
		{

			// If resizing, determine whether we skip this one
			if (conf->resize & Y_SHRINK)
			{
				// Determine rows to skip
				if (conf->disp_y[row%100] == 0)
				{
					// Skip this one
					continue;
				}
			}

			RGB8toFBPng( conf, fbRow, row_pointers[row], width, num_palette, palette );
			WriteFB( fb, fbRow, BytesPerFBPixel(conf->fmt) * conf->width );
			dispRow++;
			if (conf->debug_level && dispRow <= 10)
			{
				HexDump( row, "r8g8b8", row_pointers[row], width*3 );
				HexDump( row, "r5g6b5", fbRow, width*2 );
			}
		}
		memset( fbRow, 0, BytesPerFBPixel(conf->fmt) * conf->width );
		if (dispRow < conf->height-1)
		{
			fillRows = conf->height - dispRow;
		}
		for (row = 0; row < fillRows; row++)
		{
			WriteFB( fb, fbRow, BytesPerFBPixel(conf->fmt) * conf->width );
		}
		fprintf( stderr, "Closing frame buffer\n" );
		close(fb);
   }
#else

   /* The other way to read images - deal with interlacing: */
	int pass, y;
	int number_of_rows = 12;
   for (pass = 0; pass < number_passes; pass++)
   {
      for (y = 0; y < height; y += number_of_rows)
      {
#ifdef sparkle /* Read the image using the "sparkle" effect. */
         png_read_rows(png_ptr, &row_pointers[y], png_bytepp_NULL,
            number_of_rows);
#else //no_sparkle /* Read the image using the "rectangle" effect */
         png_read_rows(png_ptr, png_bytepp_NULL, &row_pointers[y],
            number_of_rows);
#endif //no_sparkle /* use only one of these two methods */
      }

      /* if you want to display the image after every pass, do
         so here */
	  // Rows are in RGB format. Convert to frame buffer format
   }
#endif // Suck in one go

   /* read rest of file, and get additional chunks in info_ptr - REQUIRED */
   png_read_end(png_ptr, info_ptr);

#else
	// Progressive reader

           /* This one's new.  You can provide functions
              to be called when the header info is valid,
              when each row is completed, and when the image
              is finished.  If you aren’t using all functions,
              you can specify NULL parameters.  Even when all
              three functions are NULL, you need to call
              png_set_progressive_read_fn().  You can use
              any struct as the user_ptr (cast to a void pointer
              for the function call), and retrieve the pointer
              from inside the callbacks using the function

                 png_get_progressive_ptr(png_ptr);

              which will return a void pointer, which you have
              to cast appropriately.
            */
		void *user_ptr = NULL;
           png_set_progressive_read_fn(png_ptr, (void *)user_ptr,
               info_callback, row_callback, end_callback);


#endif

	// Free read pointers
	for (row = 0; row < height; row++)
	{
		if (row_pointers[row] != NULL)
		{
			png_free( png_ptr, row_pointers[row] );
			row_pointers[row] = NULL; ///< This is just for debugging
		}
	}

	// Free read pointer collection
	png_free( png_ptr, row_pointers );

      /* Free all of the memory associated with the png_ptr and info_ptr
		(other than what we explicitly allocated with png_malloc) */
      png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);

	fclose( fp );

	return 0;
}

#endif

#ifndef NO_PNG
/*******************************************************
 Jpeg support functions
********************************************************/

LOCAL(unsigned int)
jpeg_getc (j_decompress_ptr cinfo)
/* Read next byte */
{
  struct jpeg_source_mgr * datasrc = cinfo->src;

  if (datasrc->bytes_in_buffer == 0) {
    if (! (*datasrc->fill_input_buffer) (cinfo))
      return 0; //ERREXIT(cinfo, JERR_CANT_SUSPEND);
  }
  datasrc->bytes_in_buffer--;
  return GETJOCTET(*datasrc->next_input_byte++);
}

METHODDEF(boolean)
print_text_marker (j_decompress_ptr cinfo)
{
  boolean traceit = (cinfo->err->trace_level >= 1);
  INT32 length;
  unsigned int ch;
  unsigned int lastch = 0;

  length = jpeg_getc(cinfo) << 8;
  length += jpeg_getc(cinfo);
  length -= 2;			/* discount the length word itself */

  if (traceit) {
    if (cinfo->unread_marker == JPEG_COM)
      fprintf(stderr, "Comment, length %ld:\n", (long) length);
    else			/* assume it is an APPn otherwise */
      fprintf(stderr, "APP%d, length %ld:\n",
	      cinfo->unread_marker - JPEG_APP0, (long) length);
  }

  while (--length >= 0) {
    ch = jpeg_getc(cinfo);
    if (traceit) {
      /* Emit the character in a readable form.
       * Nonprintables are converted to \nnn form,
       * while \ is converted to \\.
       * Newlines in CR, CR/LF, or LF form will be printed as one newline.
       */
      if (ch == '\r') {
	fprintf(stderr, "\n");
      } else if (ch == '\n') {
	if (lastch != '\r')
	  fprintf(stderr, "\n");
      } else if (ch == '\\') {
	fprintf(stderr, "\\\\");
      } else if (isprint(ch)) {
	putc(ch, stderr);
      } else {
	fprintf(stderr, "\\%03o", ch);
      }
      lastch = ch;
    }
  }

  if (traceit)
    fprintf(stderr, "\n");

  return TRUE;
}

static int
ShowJpeg(struct imgtool_conf *conf)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JDIMENSION num_scanlines;
	/* Output pixel-row buffer.  Created by module init or start_output.
	* Width is cinfo->output_width * cinfo->output_components;
	* height is buffer_height.
	*/
	JSAMPARRAY buffer;
	JDIMENSION buffer_height;
	FILE * input_file;

	input_file = fopen( conf->filename, "rb" );
	if (!input_file)
	{
		fprintf( stderr, "Cannot open %s r/o\n", conf->filename );
		return -1;
	}

	/* Initialize the JPEG decompression object with default error handling. */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	/* Insert custom marker processor for COM and APP12.
	* APP12 is used by some digital camera makers for textual info,
	* so we provide the ability to display it as text.
	* If you like, additional APPn marker types can be selected for display,
	* but don't try to override APP0 or APP14 this way (see libjpeg.doc).
	*/
	jpeg_set_marker_processor(&cinfo, JPEG_COM, print_text_marker);
	jpeg_set_marker_processor(&cinfo, JPEG_APP0+12, print_text_marker);

	/* Specify data source for decompression */
	jpeg_stdio_src(&cinfo, input_file);

	/* Read file header, set default decompression parameters */
	(void) jpeg_read_header(&cinfo, TRUE);

	/* Calculate output image dimensions so we can allocate space */
	jpeg_calc_output_dimensions(&cinfo);

	/* Create decompressor output buffer. */
	JDIMENSION row_width;
	row_width = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)
	((j_common_ptr) &cinfo, JPOOL_IMAGE, row_width, (JDIMENSION) 1);
	buffer_height = 1;

	// Determine resizing
	unsigned int scaledWidth, scaledHeight;
	scaledWidth = cinfo.output_width;
	scaledHeight = cinfo.output_height;
	if (AdjustOutputSize(&scaledWidth, &scaledHeight, conf))
	{
		fprintf( stderr, "Scaling from %dX%d to %dX%d (%d%%/%d%%)\n",
			(int)cinfo.output_width, (int)cinfo.output_height,
			scaledWidth, scaledHeight,
			conf->x_pct, conf->y_pct );
	}

	/* Start decompressor */
	(void) jpeg_start_decompress(&cinfo);

	/* Write output file header */
	//(*dest_mgr->start_output) (&cinfo, dest_mgr);

	// Open frame buffer
   int fb = OpenOutput(scaledWidth, scaledHeight, conf->output, 1);
   if (fb > 0)
   {
		unsigned char *fbRow = (unsigned char *)malloc(BytesPerFBPixel(conf->fmt)*conf->width);
		if (!fbRow)
		{
			fprintf( stderr, "malloc() failed errno=%d (%s)\n", errno, strerror(errno) );
			exit( 1 );
		}
		unsigned int maxRow = cinfo.output_height-1;
		unsigned int minRow = 0;
		int fillRows = 0;
		if (!conf->resize)
		{
			// Clip oversized rows
			if (maxRow < cinfo.output_height-1)
			{
				minRow = cinfo.output_height-conf->height;
			}
		}
		unsigned int dispRow = minRow;
		// Fill empty space
		if (maxRow - minRow < conf->height-1)
		{
			fillRows = conf->height - 1 - maxRow - minRow;
		}
		fprintf( stderr, "Displaying rows from %d to %d inclusive\n", minRow, maxRow );
		unsigned int row = minRow;

		/* Process data */
		while (cinfo.output_scanline < cinfo.output_height)
		{
			num_scanlines = jpeg_read_scanlines(&cinfo, buffer,
						buffer_height);
			row = cinfo.output_scanline;
			if (row >= minRow && row<=maxRow && dispRow < conf->height)
			{
				//(*dest_mgr->put_pixel_rows) (&cinfo, dest_mgr, num_scanlines);
				// If resizing, determine whether we skip this one
				if (conf->resize & Y_SHRINK)
				{
					// Determine rows to skip
					if (conf->disp_y[row%100] == 0)
					{
						// Skip this one
						continue;
					}
				}

				RGB8toFBPng( conf, fbRow, buffer[0], cinfo.output_width, 0, NULL );
				WriteFB( fb, fbRow, BytesPerFBPixel(conf->fmt) * conf->width );
				dispRow++;
				if (conf->debug_level && dispRow <= 10)
				{
					HexDump( row, "r8g8b8", buffer[0], cinfo.output_width*3 );
					HexDump( row, "r5g6b5", fbRow, cinfo.output_width*2 );
				}
			}
		}

		fprintf( stderr, "Closing frame buffer\n" );
		close(fb);
		free( fbRow );
   }
   else
   {
   	fprintf( stderr, "Error: could not open frame buffer (errno=%d)\n", errno );
   }

	/* Finish decompression and release memory.
	* I must do it in this order because output module has allocated memory
	* of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.
	*/
	//(*dest_mgr->finish_output) (&cinfo, dest_mgr);
	(void) jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	fclose( input_file );

	return 0;
}

#endif

// Display bmp to frame buffer. No resizing - must be the right size
#if 0
static int ShowBmp(struct imgtool_conf *conf)
{
	// Open input file
	int hInput = open( conf->filename, O_RDONLY );
	if (hInput == -1)
	{
		fprintf( stderr, "Error: cannot open %s for input; errno=%d (%s)\n", conf->filename, errno, strerror(errno) );
		return -1;
	}
	int ret = -1;
	int hOutput = -1;
	int bpp;
	int bmp_width;
	int bmp_height;
	int bmp_compression;
	int bytes_per_pixel;
	int row;
	unsigned char *input_buff;
	unsigned char *output_buff;
	// Read bmp header
	BMPHeader_t bh;
	if (read( hInput, &bh, sizeof(bh) ) != sizeof(bh))
	{
		fprintf( stderr, "Error: cannot read %d bytes from %s for bmp header\n", sizeof(bh), conf->filename );
		goto exit_close_input;
	}
	if (!bh.fileHeader.IsBMP())
	{
		fprintf( stderr, "Error: %s lacks required BM signature - not a bmp file\n", conf->filename );
		goto exit_close_input;
	}
	if (bh.infoHeader.GetPlanes() != 1)
	{
		fprintf( stderr, "Error: %s has %d planes, only 1 plane supported\n", conf->filename, bh.infoHeader.GetPlanes() );
		goto exit_close_input;
	}
	bpp = bh.infoHeader.GetBitsPerPixel();
	bmp_width = bh.infoHeader.GetImageWidth();
	bmp_height = bh.infoHeader.GetImageHeight();
	bmp_compression = bh.infoHeader.GetCompressionType();
	fprintf( stderr, "Opened %s for input, %dX%d %dbpp\n", conf->filename, bmp_width, bmp_height, bpp );
	if (bmp_compression != 0)
	{
		if (bmp_compression == 3)
		{
			fprintf( stderr, "Warning: file marked as using compression type %d, attempting render anyway\n", bmp_compression );
		}
		else
		{
			fprintf( stderr, "Compression type %d not supported\n", bmp_compression );
			goto exit_close_input;
		}
	}
	if (bpp != 16 && bpp != 24 && bpp != 32)
	{
		fprintf( stderr, "Only 16bpp, 24bpp or 32bpp supported\n" );
		goto exit_close_input;
	}
	/*****
	 * Allow display of  smaller bmp left justified
	if (bmp_width != conf->width || bmp_height != conf->height)
	{
		fprintf( stderr, "bmp size is %dX%d - must match specified frame buffer size %dX%d\n",
			bmp_width, bmp_height, conf->width, conf->height );
		goto exit_close_input;
	}
	 *
	******/
	if (lseek( hInput, bh.fileHeader.GetImageDataOffset(), SEEK_SET ) == -1L)
	{
		fprintf( stderr, "Error: seek to image data offset %ld on input file %s failed, errno=%d (%s)\n",
			bh.fileHeader.GetImageDataOffset(), conf->filename, errno, strerror(errno) );
		goto exit_close_input;
	}

	// Now open output
	hOutput = OpenOutput( conf->width, conf->height, conf->output, true );
	if (hOutput < 0)
	{
		fprintf( stderr, "Error: failed to open %s (errno=%d)\n", conf->output, errno );
		goto exit_close_input;
	}

	// Allocate input and output buffers
	bytes_per_pixel = bpp / 8;
	input_buff = (unsigned char*)malloc( bytes_per_pixel * bmp_width * bmp_height );
	if (input_buff == NULL)
	{
		fprintf( stderr, "Malloc failed for %d bytes\n", bytes_per_pixel * bmp_width * bmp_height );
		goto exit_close_input;
	}
	output_buff = (unsigned char*)malloc( BytesPerFBPixel(conf->fmt) * conf->width );
	if (output_buff == NULL)
	{
		fprintf( stderr, "malloc failed for %d bytes\n", BytesPerFBPixel(conf->fmt) * conf->width );
		goto exit_free_input_buff;
	}

	if (read( hInput, input_buff, bytes_per_pixel * bmp_width * bmp_height ) != bytes_per_pixel * bmp_width * bmp_height)
	{
		fprintf( stderr, "read failed for %d bytes\n", bytes_per_pixel * bmp_width * bmp_height );
		goto exit_free_output_buff;
	}

	// Write bmp header if requested
	if (conf->bmp_mode)
	{
		// Fix up bits per pixel, size of image, and file size
		bh.infoHeader.SetBitsPerPixel( 8 * BytesPerFBPixel(conf->fmt) );
		bh.infoHeader.SetImageDataLength( conf->width * conf->height * BytesPerFBPixel(conf->fmt) );
		bh.fileHeader.SetFilesize( bh.fileHeader.GetImageDataOffset() + conf->width * conf->height * BytesPerFBPixel(conf->fmt) );
		// Write same size header
		if (WriteFB( hOutput, &bh, bh.fileHeader.GetImageDataOffset() ) != (int)bh.fileHeader.GetImageDataOffset())
		{
			fprintf( stderr, "bmp header write failed, errno=%d (%s)\n", errno, strerror(errno) );
			goto exit_free_output_buff;
		}
	}

	// FIXME use g_fill
	memset( output_buff, 0, bytes_per_pixel * conf->width );
	for (row = 0; row < bmp_height; row++)
	{
		// If bmp mode, we're not flipping
		int yRow = row;
		int bmpyRow = row;
		if (!conf->bmp_mode)
		{
			yRow = conf->height - row - 1;
			bmpyRow = bmp_height - row - 1;
		}
		// Kluge for rgb565 bmp files
		if (bpp == 16)
		{
			memcpy( output_buff, &input_buff[bmpyRow * bmp_width * bytes_per_pixel], bmp_width * bytes_per_pixel );
		}
		else
		{
			ARGB8888toFB( conf, output_buff, &input_buff[bmpyRow*bmp_width*bytes_per_pixel], bmp_width );
		}
		if (WriteFB( hOutput, output_buff, BytesPerFBPixel(conf->fmt) * conf->width ) != (int)(BytesPerFBPixel(conf->fmt) * conf->width))
		{
			fprintf( stderr, "write failed for %d bytes at row %d\n", BytesPerFBPixel(conf->fmt) * conf->width, row );
			goto exit_free_output_buff;
		}
	}

	ret = 0;

exit_free_output_buff:
	free( output_buff );
exit_free_input_buff:
	free( input_buff );
	close(hOutput);
exit_close_input:
	close( hInput );
	return ret;
}
#endif

// Capture frame buffer to jpeg
static void CaptureJpeg(struct imgtool_conf *conf)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE * output_file;
	JDIMENSION num_scanlines;
	int usingStdout = (strcmp( conf->filename, "-" ) == 0);
	JSAMPARRAY buffer;
	JDIMENSION buffer_height;

	/* Initialize the JPEG compression object with default error handling. */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	/* Initialize JPEG parameters.
	* Much of this may be overridden later.
	* In particular, we don't yet know the input file's color space,
	* but we need to provide some value for jpeg_set_defaults() to work.
	*/

	cinfo.in_color_space = JCS_RGB; /* arbitrary guess */
	jpeg_set_defaults(&cinfo);

	// Create output handle
	if (usingStdout)
	{
		output_file = stdout;
	}
	else
	{
		output_file = fopen( conf->filename, "wb" );
	}
	if (!output_file)
	{
		fprintf( stderr, "Error: cannot open %s for output\n", conf->filename );
		return;
	}

	/* Allocate one-row buffer for returned data */
	buffer = (cinfo.mem->alloc_sarray)
	((j_common_ptr) &cinfo, JPOOL_IMAGE,
	 (JDIMENSION) (conf->width * 3), (JDIMENSION) 1);
	buffer_height = 1;

	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;
	cinfo.data_precision = 8; // bits per RGB component
	cinfo.image_width = (JDIMENSION) conf->width;
	cinfo.image_height = (JDIMENSION) conf->height;

	/* Input colorspace has been set (always RGB) - fix colorspace-dependent defaults */
	jpeg_default_colorspace(&cinfo);

    /* Set quantization tables for selected quality. */
    /* Some or all may be overridden if -qtables is present. */
    jpeg_set_quality(&cinfo, conf->jpeg_quality, FALSE	/* by default, allow 16-bit quantizers */);

	/* Specify data destination for compression */
	jpeg_stdio_dest(&cinfo, output_file);

	/* Start compressor */
	jpeg_start_compress(&cinfo, TRUE);

	// Open frame buffer for input
	int fb = OpenOutput(cinfo.image_width, cinfo.image_height, conf->output, 0);
	if (fb == -1) {
		fprintf( stderr, "Error: could not open frame buffer for input!\n" );
		goto out;
	}

	unsigned char *fbRow = (unsigned char *)malloc(BytesPerFBPixel(conf->fmt)*conf->width);
	if (!fbRow) {
		fprintf( stderr, "malloc failed error %d (%s)\n", errno, strerror(errno) );
		exit( 1 );
	}
	int errCount = 0;
	int rowCount = 0;

	/* Process data */
	while (cinfo.next_scanline < cinfo.image_height && errCount == 0) {
		num_scanlines = buffer_height;
		// Get a row from frame buffer in native format
		if (read( fb, fbRow, BytesPerFBPixel(conf->fmt) * conf->width ) < (int)(BytesPerFBPixel(conf->fmt) * conf->width))
		{
			fprintf( stderr, "Error: failed reading row %d from frame buffer\n", cinfo.next_scanline );
			errCount++;
			continue;
		}
		// Convert to RGB888
		FBtoRGB888( conf, buffer[0], fbRow, cinfo.image_width );
		if (conf->debug_level && rowCount < 10)
		{
			HexDump( rowCount, "r8g8b8", buffer[0], cinfo.image_width*3 );
			HexDump( rowCount, "fb", fbRow, cinfo.image_width*BytesPerFBPixel(conf->fmt) );
		}
		rowCount++;
		// Compress
		(void) jpeg_write_scanlines(&cinfo, buffer, num_scanlines);
	}

	fprintf( stderr, "Closing frame buffer\n" );
	close(fb);
	free( fbRow );

	// FIXME add JFIF comments on source, date/time, hostname, guid, etc

out:

	/* Finish compression and release memory */
	//(*src_mgr->finish_input) (&cinfo, src_mgr);
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	if (!usingStdout)
	{
		fclose( output_file );
	}
}

#ifndef NO_PNG


// Capture frame buffer to png (lossless RGB)
static int CapturePng(struct imgtool_conf *conf)
{
	png_structp png_ptr;
	png_infop info_ptr;
	FILE *fp;
	unsigned char *screen;
	int x, y;
	int fd;
	/* static */
	png_byte **row_pointers;
	png_uint_32 bytes_per_row;

	fd = open("/dev/fb0", O_RDWR);
	screen = (unsigned char *) mmap(0, conf->width * conf->height * BytesPerFBPixel(conf->fmt),
				PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	fp = fopen(conf->filename, "wb");

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		munmap(screen, conf->width * conf->height * BytesPerFBPixel(conf->fmt));
		close(fd);
		return -1;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, NULL);
		munmap(screen, conf->width * conf->height * BytesPerFBPixel(conf->fmt));
		close(fd);
		return -1;
	}


	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		munmap(screen, conf->width * conf->height * BytesPerFBPixel(conf->fmt));
		close(fd);
		return -1;
	}

	png_set_IHDR(png_ptr,
			info_ptr,
			conf->width,
			conf->height,
			8,
			PNG_COLOR_TYPE_RGB,
			PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);


	/* Initialize rows of PNG. */
	bytes_per_row = conf->width * BytesPerFBPixel(conf->fmt);
	row_pointers = png_malloc(png_ptr, conf->height * sizeof(png_byte *));
	for (y = 0; y < conf->height; ++y) {
		uint8_t *row = png_malloc(png_ptr, sizeof(uint8_t) * 3 * conf->width);
		row_pointers[y] = (png_byte *)row;
		//memcpy(row, screen, bytes_per_row);
		FBtoRGB888(conf, row, screen+(y*bytes_per_row), conf->width);
	}

	/* Actually write the image data. */
	png_init_io(png_ptr, fp);
	png_set_rows(png_ptr, info_ptr, row_pointers);
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	/* Cleanup. */
	for (y = 0; y < conf->height; y++)
		png_free(png_ptr, row_pointers[y]);
	png_free(png_ptr, row_pointers);

	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
	munmap(screen, conf->width * conf->height * BytesPerFBPixel(conf->fmt));
	close(fd);
	return 0;
}


#define	SUPPORTED_EXTENSIONS ".jpg or .bmp"

#else
#define SUPPORTED_EXTENSIONS ".jpg, .bmp or .png"
#endif

// Fill frame buffer with rgb value
static int FillRGB(struct imgtool_conf *conf)
{
	int ret = -1;
	int hOutput = -1;
	int bpp;
	int bytes_per_pixel;
	unsigned int row, col;
	unsigned char *input_buff;
	unsigned char *output_buff;
	char *scr;

	// Now open output
	hOutput = OpenOutput(conf->width, conf->height, conf->output, 1);
	if (hOutput < 0)
	{
		fprintf( stderr, "Error: failed to open %s (errno=%d)\n", conf->output, errno );
		goto exit_close_input;
	}

	// Allocate input and output buffers
	bpp = 32;
	bytes_per_pixel = bpp / 8;
	input_buff = (unsigned char*)malloc( bytes_per_pixel * conf->width );
	if (input_buff == NULL)
	{
		fprintf( stderr, "Malloc failed for %d bytes\n", bytes_per_pixel * conf->width );
		goto exit_close_input;
	}
	output_buff = (unsigned char*)malloc( BytesPerFBPixel(conf->fmt) * conf->width );
	if (output_buff == NULL)
	{
		fprintf( stderr, "malloc failed for %d bytes\n", 2 * conf->width );
		goto exit_free_input_buff;
	}

	// Set up input buffer
	for (col = 0; col < conf->width; col++)
	{
		((unsigned int*)input_buff)[col] = (conf->fill_color<<0);
	}
	// Convert to frame buffer
	ARGB8888toFB( conf, output_buff, input_buff, conf->width );
	// Dump in hex for 8 columns
	//HexDump( 0, "Fill pattern", output_buff, 4 * 8 );

	scr = (char *) mmap(0, conf->width * conf->height * BytesPerFBPixel(conf->fmt),
				PROT_READ | PROT_WRITE, MAP_SHARED, hOutput, 0);
	if(scr != (char *)-1) {
		char *current_row = scr;
		for (row = 0; row < conf->height; row++)
		{
			memcpy(current_row, output_buff, BytesPerFBPixel(conf->fmt)*conf->width);
			current_row += BytesPerFBPixel(conf->fmt)*conf->width;
		}
		munmap(scr, conf->width * conf->height * BytesPerFBPixel(conf->fmt));
	}
	else {
		perror("Unable to mmap framebuffer");

		for (row = 0; row < conf->height; row++)
		{
			if (WriteFB( hOutput, output_buff, BytesPerFBPixel(conf->fmt) * conf->width ) != ((int)(BytesPerFBPixel(conf->fmt) * conf->width)))
			{
				fprintf( stderr, "write failed for %d bytes at row %d\n", BytesPerFBPixel(conf->fmt) * conf->width, row );
				goto exit_free_output_buff;
			}
		}
	}

	ret = 0;

exit_free_output_buff:
	free( output_buff );
exit_free_input_buff:
	free( input_buff );
	close(hOutput);
exit_close_input:
	return ret;
}


static const char *imgHelpText = "[options] file\n"
"	where file is output (mode=cap) or - to write to stdout, or\n"
"	if mode==draw, a " SUPPORTED_EXTENSIONS " image file to write to frame buffer\n"
"	and options are any of the following:\n"
"\n"
"	* General options:\n"
"	--debug			  Increase verbosity\n"
"	--fb=n (0)		  Write to / read from frame buffer (0 or 1)\n"
"	--mode={cap,draw} (draw)  Capture frame buffer to file (cap)\n"
"				  or draw image file to frame buffer\n"
"	--width=n (%3d)		  Width in pixels\n"
"	--height=n (%3d)	  Height in pixels\n"
"	--output=path		  Write to path instead of /dev/fb0\n"
"	--bmpmode=n (0)		  Prepend output with bmp header\n"
"	--fill=r,g,b		  Fill frame buffer with rgb value\n"
"	--bitfmt={rgb565,rgb888,argb8888} 	Specify bit format\n"
"	--help			  Display this message\n"
"\n"
"	* Render options:\n"
"	--gamma=f (2.2)	  	  Screen gamma (for png decode)\n"
"	--resize=n (64)	  	  Resize options (draw mode only)\n"
"	--mirrorh		  Mirror horizontally\n"
"\n"
"	* Capture options:\n"
"	--quality=pct (75)	  JPEG capture quality (0-100)\n"
"	--fmt={jpg,png} (jpg)	  Format to write (if mode is cap)\n"
"";


static const char *
parse_args(struct imgtool_conf *conf, int argc, char **argv)
{
	int n;

	// Get default width and height from environment
	for (n = 1; n < argc; n++) {
		if (argv[n][0] != '-' || !strcmp(argv[n],"-")) {
			strncpy( conf->filename, argv[n], sizeof(conf->filename) );
			continue;
		}
		char *option = argv[n] + 2; // Skip --
		char *optarg = strchr( option, '=' );
		if (optarg) optarg++; // skip =
		int optionLength = strcspn( option, "=" );
		if (optionLength == 0)
			return "Invalid option specified";

		if (!strncmp( option, "debug", optionLength )) {
			conf->debug_level++;
		}

		else if (!strncmp( option, "resize", optionLength )) {
			if (!optarg)
				return "Numeric option required for --resize= option";
			conf->resize_options = atoi( optarg );
		}

		else if (!strncmp( option, "gamma", optionLength )) {
			if (!optarg)
				return "Float option required for --gamma= option";
			conf->gamma = atof( optarg );
		}

		else if (!strncmp( option, "quality", optionLength )) {
			if (!optarg)
				return "Numeric percentage required for --quality= option";
			conf->jpeg_quality = atoi( optarg );
			if (conf->jpeg_quality < 1 || conf->jpeg_quality > 100)
				return "Specified JPEG compression quality is outside acceptable range 1-100";
		}

		else if (!strncmp( option, "fb", optionLength )) {
			if (!optarg)
				return "Numeric arg required for --fb= option";
			conf->fb_num = atoi( optarg );
			snprintf( conf->output, sizeof(conf->output), "/dev/fb%d", conf->fb_num );
		}

		else if (!strncmp( option, "fmt", optionLength )) {
			if (!optarg)
				return "Either jpg or png required for --fmt= option";
			strncpy( conf->output_format, optarg, sizeof(conf->output_format) );
		}

		else if (!strncmp( option, "output", optionLength )) {
			if (!optarg)
				return "Output filename required for --output= option";
			strncpy(conf->output, optarg, sizeof(conf->output) );
		}

		else if (!strncmp( option, "mode", optionLength )) {
			if (optarg && !strcmp(optarg, "draw"))
				conf->op = OP_DRAW;
			else if (optarg && !strcmp(optarg, "cap"))
				conf->op = OP_CAPTURE;
			else
				return "Unrecognized mode";
		}

		else if (!strncmp( option, "width", optionLength ) && optarg)
			conf->width = atoi( optarg );

		else if (!strncmp( option, "height", optionLength ) && optarg)
			conf->height = atoi( optarg );

		else if (!strncmp( option, "bmpmode", optionLength ) && optarg)
			conf->bmp_mode = atoi( optarg );

		else if (!strncmp( option, "fill", optionLength ) && optarg) {
			int r, g, b, a;
			int num_elements = sscanf( optarg, "%d,%d,%d,%d", &r, &g, &b, &a );
			if (num_elements >= 3)
			{
				conf->fill_color = (((unsigned int)r) << 16) | (((unsigned int)g) << 8) | (unsigned int)b;
				if (num_elements > 3)
				{
					conf->fill_color |= (((unsigned int)a) << 24);
					fprintf( stderr, "Filling with r,g,b,a %d,%d,%d,%d (0x%08x)\n", r, g, b, a, conf->fill_color );
				}
				else
				{
					fprintf( stderr, "Filling with r,g,b %d,%d,%d (0x%06x00)\n", r, g, b, conf->fill_color );
				}
			}
			else
			{
				fprintf( stderr, "Unable to scan 3+ rgb[a] values from %s\n", optarg );
			}
		}

		else if (!strncmp( option, "bitfmt", optionLength ) && optarg) {
			conf->fmt = BitFormatToEnum( optarg );
			if (conf->fmt < 0)
				exit(1);
		}

		else if (!strncmp( option, "mirrorh", optionLength ))
			conf->mirror_h = 1;

		else if (!strncmp( option, "help", optionLength ))
			return "";

		else
			return "Unrecognized option specified";
	}

	return NULL;
}


static int
fill_fb_defaults (struct imgtool_conf *conf)
{
	int ret, fd;
	struct fb_var_screeninfo var;

	fd = open("/dev/fb0", O_RDONLY);
	if (fd == -1)
		return -1;

	ret = ioctl(fd, FBIOGET_VSCREENINFO, &var);
	if (ret == -1)
		goto out;

	conf->width = var.xres;
	conf->height = var.yres;
	if (var.bits_per_pixel == 16) {
		if (var.red.offset)
			conf->fmt = BF_RGB565;
		else
			conf->fmt = BF_BGR565;
	}
	else if (var.bits_per_pixel == 24)
		conf->fmt = BF_RGB888;
	else
		conf->fmt = BF_ARGB8888;
	ret = 0;

out:
	close(fd);
	return ret;
}


int
main( int argc, char *argv[] )
{
	const char *error_message = NULL;
	struct imgtool_conf conf;

	bzero(&conf, sizeof(conf));
	conf.gamma = 2.2;
	conf.fill_color = 0xffffffff;
	conf.x_pct = 100;
	conf.y_pct = 100;
	conf.jpeg_quality = 75;
	strncpy(conf.output_format, "jpg", sizeof(conf.output_format));
	snprintf(conf.output, sizeof(conf.output), "/dev/fb%d", conf.fb_num);

	fill_fb_defaults(&conf);


	fprintf( stderr, "%s " VER_FMT " (built for " CNPLATFORM ")\n", argv[0], VER_DATA );

	error_message = parse_args(&conf, argc, argv);


	// Are we filling?
	if (conf.fill_color != 0xffffffff) {
		FillRGB( &conf );
		if (!*conf.filename) {
			fprintf( stderr, "Filled with 0x%x, no image to load, exiting\n", conf.fill_color );
			return 0;
		}
	}

	// Did we get anything to process?
	if (!conf.filename[0] && error_message == NULL) {
		fprintf(stderr, "No file specified for %s", conf.op == OP_DRAW ? "output" : "input" );
		error_message = "";
	}

	// Any errors?
	if (error_message) {
		fprintf(stderr, "%s\nSyntax: %s ", error_message, argv[0] );
		fprintf(stderr, imgHelpText, conf.width, conf.height );
		return -1;
	}


	// Handle mode
	if (conf.op == OP_CAPTURE) {
		fprintf( stderr, "Capturing to %s from fb%d format %s\n",
			!strcmp(conf.filename, "-")?"<stdout>":conf.filename, conf.fb_num, conf.output_format);

		if (!strcmp( conf.output_format, "jpg" ))
			CaptureJpeg(&conf);

		else if (!strcmp( conf.output_format, "png" )) {
#ifdef NO_PNG
			fprintf( stderr, "--fmt=png not supported (NO_PNG)\n" );
			return -1;
#else
			CapturePng(&conf);
#endif
		}
		else {
			fprintf( stderr, "Error: unsupported output format %s - use jpg or png\n", conf.output_format );
			return -1;
		}
	}

	else if (conf.op == OP_DRAW) {
		if (!strcmp( conf.filename, "-" )) {
			fprintf( stderr, "Unable to accept image file from stdin\n" );
			return -1;
		}

		fprintf( stderr, "Drawing image %s\n", !strcmp(conf.filename, "-")?"<stdout>":conf.filename );

		char *ext = strrchr( conf.filename, '.' );
		if (ext == NULL) {
			fprintf( stderr, "No extension found in %s\n", conf.filename );
			return -1;
		}
#ifdef NO_PNG
		if (!strcasecmp( ext, ".jpg" ) ||
			!strcasecmp( ext, ".png" ))
		{
			fprintf( stderr, "%s not supported (NO_PNG build also does not support jpeg decode)\n", ext );
			return -1;
		}
#else
		if (!strcasecmp( ext, ".jpg" ))
			return ShowJpeg(&conf);

		if (!strcasecmp( ext, ".png" ))
			return ShowPng(&conf);
#endif

#if 0
		if (!strcasecmp( ext, ".bmp" ))
			return ShowBmp(&conf);
#endif

		fprintf( stderr, "%s files not supported\n", ext );
		return -1;
	}

	else {
		fprintf( stderr, "Unhandled mode -- must be cap or draw\n");
		return -1;
	}

	return 0;
}
