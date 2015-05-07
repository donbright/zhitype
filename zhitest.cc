/*

ZhiType
Copyright (c) 2015, don bright, http://patreon.com/hugbright

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of zhitype nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/*
A unicode TrueType(TM) font renderer for microcontrollers and e-paper

Goals

1. very very simple code, skip unnecessary steps
2. keep code size small
3. don't try to work with every TTF file, just the basic ones

Notes

0. tradeoff portability, speed, error-checking+security, for small memory usage
1. goal 2 results in lots of ifdefs
2. use C++ but use it as if it were C. no classes, STL, etc.
3. no objects, no class hierarchy, no nothin'.
4. treat font file as a streaming object we can pull off an SD card
5. don't care about pixel tweaking at low resolutions. because
5.a. e-paper is high res so you wont have the same problems as old monitors
5.b. e-paper is generally only two colors (black/white) so it wont help
5.c. the code is so complicated it wont be doable on microcontrollers?
6. pretend we are reading from an SD card, which on arduino means one
 byte at a time.
8. DEBUG can be turned on for testing inside of a computer, like
 on linux. DEBUG can be turned off to chop down code size and for porting
 to Arduino libraries.
9. avoid use of pointers, use C++ pass by reference, to cut down on errors
 and possible crashes.
10. cannot store full data of points in memory, there isnt enough memory.
 40 points could be 400 bytes, our SRAM is 2k on an arduino.
 process points as a stream. using fseek(). slow but it might work.
11. a lot of error checking has to be skipped (checksums for example)
12. try not to use heap memory at all
13. trade time for memory. this is why fseek() is everywhere.


Things Not Supported:

Which 'CMAPs' are supported?
( https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6cmap.html )

A1. Unicode Only
A2. Formats 4 6 and 12 only
A3. There will be no error reporting due to code size requirements being
 tiny.

Simple glyphs only, no Compound Glyphs are supported.


See also
Basic overview and format details:
https://developer.apple.com/fonts/TrueType-Reference-Manual/RM01/Chap1.html
http://www.freetype.org/freetype2/docs/reference/ft2-truetype_tables.html
https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6.html
https://www.microsoft.com/typography/tt/ttf_spec/ttch02.doc
http://cplusplus.com/
Alternatives:
http://en.wikipedia.org/wiki/SFNT
https://code.google.com/p/sfntly/
https://github.com/ccxvii/snippets/blob/master/ttfdump.c
http://manpages.ubuntu.com/manpages/maverick/man1/ttfdump.1.html
FreeType's glyph loader is in a file named 'ttgload.c', which you can google.
(it's in dozens of projects). See http://freetype.org



How does TrueType work?

There are two main tables we care about. CMAP and GLYF.
CMAP is a map from, say, unicode, to a position in the file with the glyph.
The GLYF table contains all the points to draw the characters.

But... GLYF is kind of special. It is designed to squeeze the most
information in the least amount of space. How?

1. It is essentially Run Length Encoded kinda like the old PCX picture format.
The coordinates can be 'repeated' and the flag (which indicates info about a)
 point) can also be 'repeated'. If you had 4 x-coordinates in a row and
they were each 16 bit numbers, this could save like 3 bytes. If you have
dozens of fonts with hundreds of characters each, this space savings adds up,
in theory.

2. Coordinates are relative to the previous coordinate, not to an
origin. If you ponder it for a while, you will perhaps notice that this
tends to cut down on the size of the numbers ( for example if i have 3
points, 1000,200 1024,201 1048,210 i can save several ASCII digits by
saying  1000,200 +24,1    +24,9. Same is true with binary digits.

3. Coordinates can be either 16 bit signed, or 8 bit unsigned. But there
is a trick. There is a 'sign' bit in the 'flag' for 8 bit numbers....
.. so they are actually 9-bit signed integers! Clever.

All of this space-packing makes te unpacking code a bit complicated.


*/

#include <stdint.h>
#include <stdio.h>

#define DEBUG 1

// Number types, byte order, endian-ness:
// https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6.html#Types
// https://www.microsoft.com/typography/tt/ttf_spec/ttch02.doc
// http://en.wikipedia.org/wiki/Endianness
// https://github.com/ccxvii/snippets/blob/master/ttfdump.c
union uint32 {
	uint32_t uint32;
	uint8_t uint8[4];
};

union uint16 {
	uint16_t uint16;
	uint8_t uint8[2];
};

union int16 {
	int16_t int16;
	uint8_t uint8[2];
};

void read_uint32(union uint32 &data, FILE *file) {
	fread(&data.uint8[3],1,1,file);
	fread(&data.uint8[2],1,1,file);
	fread(&data.uint8[1],1,1,file);
	fread(&data.uint8[0],1,1,file);
}

void read_uint16(union uint16 &data, FILE *file) {
	fread(&data.uint8[1],1,1,file);
	fread(&data.uint8[0],1,1,file);
}

void read_uint8(uint8_t &data, FILE *file) {
	fread(&data,1,1,file);
}

void read_int16(union int16 &data, FILE *file) {
	fread(&data.uint8[1],1,1,file);
	fread(&data.uint8[0],1,1,file);
}


bool equal(union uint32 &data, const char (&s)[5]) {
	for (int i=0;i<4;i++) if (data.uint8[3-i]!=s[i]) return false;
	return true;
}

// Main font info storage: combined data from several tables.
typedef struct fontinfo_t {
	union uint32 ofascaler;
	union uint16 numtables;
	union uint32 cmap_table_offset;
	union uint32 cmap_table_length;
	union uint32 glyf_table_offset;
	union uint32 glyf_table_length;
} fontinfo;

// Part of main Font Directory, at beginning of file
// https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6.html#Overview
typedef struct table_directory_t {
	union uint32 tag;
	union uint32 checkSum;
	union uint32 offset;
	union uint32 length;
} table_directory;

void read_table_directory( table_directory &td, FILE * f ) {
	read_uint32(td.tag,f);
	read_uint32(td.checkSum,f);
	read_uint32(td.offset,f);
	read_uint32(td.length,f);
}

void read_table_directories( fontinfo &fi, FILE * file ) {
	table_directory td;
	for (int i=0;i<fi.numtables.uint16;i++) {
		read_table_directory(td,file);
		if (equal(td.tag,"cmap")) {
			fi.cmap_table_offset = td.offset;
			fi.cmap_table_length = td.length;
		} else if (equal(td.tag,"glyf")) {
			fi.glyf_table_offset = td.offset;
			fi.glyf_table_length = td.length;
		}
	}
}

// each Glyph (pictorial representation of character) should have a definition
// https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6glyf.html
typedef struct glyf_description_t {
	union int16 numberOfContours;
	union int16 xMin;
	union int16 yMin;
	union int16 xMax;
	union int16 yMax;
} glyf_description;

typedef struct glyf_point_t {
	union uint16 endPtIndex;
	uint8_t flag;
	union int16 xCoord;
	union int16 yCoord;
} glyf_point;

// GF stands for 'Glyf Flag' - where the space-packing magic happens!
#define GF_ON_CURVE   0b00000001;
#define GF_XSHORT_VEC 0b00000010;
#define GF_YSHORT_VEC 0b00000100;
#define GF_REPEAT     0b00001000;
#define GF_X_IS_SAME  0b00010000;
#define GF_Y_IS_SAME  0b00100000;
#define GF_POS_XSHORT 0b00010000;
#define GF_POS_YSHORT 0b00100000;

void read_glyf_description( glyf_description &gd, FILE * f ) {
	read_int16(gd.numberOfContours,f);
	read_int16(gd.xMin,f);
	read_int16(gd.yMin,f);
	read_int16(gd.xMax,f);
	read_int16(gd.yMax,f);
}

void read_coord( uint32_t fpos, int16_t &delta, FILE *f, bool isshort, bool issame ) {
	fseek(f,fpos,SEEK_SET);
	if (isshort) {
		read_uint8( tmp, f );
		if (ispos) xdelta=tmp;
		else xdelta=-tmp;
	} else if (issame) {
		xdelta = 0;
	} else {
		read_int16( xdelta, f );
	}
}

void do_glyf_data( glyf_description &gd, FILE *f, uint32_t dataoffset ){
	fseek(f,dataoffset,SEEK_SET);
	uint16_t num_points;
	union uint16 endpt_index;
	for (int i=0;i<gd.num_contours;i++) read_uint16( endpt_index, f );
	num_points = endpt_index+1;

	union uint16 instructionLength;
	read_uint16( instructionLength, f );
	uint32_t flags_i = ftell(f) + instructionLength * 1;
	uint32_t endpts_i = endpts_offset;
	glyf_point gp;
	uint32_t x_i = flags_i;
	uint32_t y_i = 0;

	// step one. figure out the offsets where x coords begin and
	// where y coords begin. how? read through all the flags.
	// where the flags end, x coordinates begin. what about y?
	// we can calculate how many bytes the x-coordinate list will use
	// as we process the flags. so find that and add it to find y start.
	uint8_t flag = 0;
	uint8_t repeat_counter = 0;
	bool done = false
	for (int i = 0; i < num_points; i++) {
		if (repeat_counter>0)
			flag = flag;
			repeat_counter--;
		} else {
			read_uint8( flag, f );
			if (flag & GF_REPEAT) read_uint8(repeat_counter,f);
		}
		uint8_t xcoord_numbytes = 2;
		if (flag & GF_XSHORT_VEC) xcoord_numbytes = 1;
		else if (flag & GF_X_IS_SAME) xcoord_numbytes = 0;
		y_i += xcoord_numbytes;
	}
	x_i = ftell(f);
	y_i += x_i;
	int16_t xcursor,ycursor;
	int16_t xdelta,ydelta;
	uint8_t tmp;
	// step two. we have flag_i, x_i, y_i all set up. process the points.
	for (int i=0;i<num_contours;i++){
		fseek(f,flags_i,SEEK_SET);
		read_uint8( flag, f );
		read_coord( x_i, flag, xdelta, f, GF_XSHORT_VEC, GF_POS_XSHORT );
		read_coord( y_i, flag, ydelta, f, GF_YSHORT_VEC, GF_POS_YSHORT );
	}
}

#ifdef DEBUG
void printinfo( fontinfo &f ){
	printf("ofascaler %u\n",f.ofascaler.uint32);
	printf("numtables %u\n",f.numtables.uint16);
	printf("cmap table offset hex %08x\n",f.cmap_table_offset.uint32);
	printf("cmap table length %u\n",f.cmap_table_length.uint32);
	printf("glyf table offset hex %08x\n",f.glyf_table_offset.uint32);
	printf("glyf table length %u\n",f.glyf_table_length.uint32);
}

void printglyf( glyf_description &gd ){
	printf("numberOfContours %i\n",gd.numberOfContours.int16);
	printf("xMin %i\n",gd.xMin.int16);
	printf("xMax %i\n",gd.xMax.int16);
	printf("yMin %i\n",gd.yMin.int16);
	printf("yMax %i\n",gd.yMax.int16);
}
#endif // DEBUG

FILE *openfile( const char *filename ) {
	FILE * f = fopen(filename,"rb");
#ifdef DEBUG
	if (!f) {
		printf("can't open file %s\n",filename);
	}
#endif // DEBUG
	return f;
}

int main(int argc, char * argv[]) {
	const char * filename = "FreeSerif.ttf";
	FILE *file = openfile( filename );
	if (!file) return 1;

	fontinfo fi;
	read_uint32(fi.ofascaler,file);
	read_uint16(fi.numtables,file);
	fseek(file,12,SEEK_SET); // skip Offset Subtable

	read_table_directories( fi, file );

	fseek(file,fi.glyf_table_offset.uint32,SEEK_SET);
	glyf_description gd;
	read_glyf_description( gd, file );
	do_glyf_data( gd, file, ftell(file) );

#ifdef DEBUG
	printglyf( gd );
	printinfo( fi );
#endif // DEBUG
	return 0;
}
