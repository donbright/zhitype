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

(Note: TrueType(TM) is a trademark and usage here does not imply endorsement)

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
14. use 0b00000000 GCC binary literal extension

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
http://scripts.sil.org/cms/scripts/page.php?site_id=nrsi&id=iws-chapter08
http://cplusplus.com/
Alternatives:
https://github.com/nothings/stb/blob/master/stb_truetype.h
https://github.com/behdad/fonttools/
http://en.wikipedia.org/wiki/SFNT
https://code.google.com/p/sfntly/
https://github.com/ccxvii/snippets/blob/master/ttfdump.c
Font Forge's ./fontforge/parsettf.c:
http://manpages.ubuntu.com/manpages/maverick/man1/ttfdump.1.html
FreeType's glyph loader is in a file named 'ttgload.c', which you can google.
(it's in dozens of projects). See http://freetype.org
TTF Security issues: (which this code completely ignores for smallness)
https://github.com/khaledhosny/ots


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




/*

On the mathematics of the Bezier Curve

Or... how to draw Bezier Curves without using multiplication, division,
floating point numbers, or other abominations of modern savagery.
(So that we might not assault our poor microcontroller with operations
requiring dozens of clockcycles)

0. Read the wikipedia article about Beziers. Now tear it up and throw it away.

1. Next, almost everything you ever learned in school about geometry

2. OK, now forget that algebra exists, Cartesian coordiantes OK, algebra no.

3. The bezier curve is a conic section. It's a parabola.

4. Parabola is the same thing you get when you throw a ball in the air and
 it comes back to earth.

5. It is quite easy to draw a ball in the air without using
multiplication, division, or floating point. Consider a ball with an x
coordinate and y coordinate.

 A. set y accelleration = -9.8
 B. run in a loop:
   x position = x position + 1
   y position = y position + y velocity
   y velocity = y velocity + y accelleration
   draw a point at x,y

This will produce a perfectly nice parabola.

Note that by setting accelleration to a constant we are essentially
simulating gravity. All of the calculus and stuff (dy/dt) is basically
saying the same thing. But this is alot simpler.

Notice, there is no multiplication, division, equations, squares, sqrts,
transcendentals, irrationals, sin, cosine, etc etc etc. It's just addition
and subtraction.

And thus we have drawn a Bezier curve using only addition and subtraction.

And... every bezier curve is a parabola.


The question is... how to take this simple situation on plain x-y coordinates
and translate it to the Beziers as used in TrueType(TM) fonts? Those Beziers
are given as... three points. Two 'on points' and one 'control point'. How
does this work?


Step 1. Find the directrix and focus of the Parabola. Also the axis of
symmetry. These are ideas from long ago, kind of predating the current
rage for algebra uber alles.

For any parabola, and two points on it, you can imagine it traces
the trajectory of a ball thrown..... where the Force upon it, the gravity
if you will, is directly perpendicular in dirction to the Directrix..
in parallel with the axis of symmetry.

See? Now, of course, x cannot just be incremented by one anymore. . .
and Y cannot be incremented by a simple velocity/acceleration addition. Why?
Because sometimes the parabola will be rotated with respect to the x-axis
and y-axis plane. It will be facing maybe 30 degrees or whatever.

But what we can do.... is look at the force-accelleration and the X velocity
as vectors. Now, break them down into their x and y pieces to match our
main coordinate system. For example if the directrix is heading on a slope
from 0,0 to 3,4, then you can think of it as a vector with two parts.
(3,0) and (4,0). The Axis of Symmetry will then have slope as though
it were from (0,0) to (4,-3). Thats how perpendicularity works.

So ... in this case we would have not just y accelleration, but both
x and y accelleration. And their proportion would be 4 to -3.
A new loop:

 x acceleration = 4
 y acceleration = -3
 loop
   x position = x position + x velocity
   x velocity = x velocity + x accelleration
   y position = y position + y velocity
   y velocity = y velocity + y accelleration
   draw a point at x,y

This will draw a 'rotated' parabola.



But there is another question yet left unanswered. We are given
three points... two on the parabola and one a 'control'. How to
find our axis and accelleration given only these three points?

Ah. Imagine the control point, forms a wedge with the line segments going
to the other two points. Bisect this wedge. That is your axis of
symmetry!

But what is it's slope? It's rise and run?
Ah. It is a combination of the slopes of the two line segments!

xdiff1, ydiff1, xdiff2, ydiff2

Imagine a parabola with an axis of symmetry that is straight up and down.
Its x-diff will be 0. It's bisector will go straight down! The slope
will be infinity. x = 0. Basically (x1-x0) + (x1-x2). The sum of the
differences of the two vectors.




Another view.
The 'linear interpolation'. Divide both legs into equal parts. draw points
delineating the parts. Draw line between the first point on leg A and the
last point on leg B. Second point on leg A and the next-to-last point on
leg B. Keep going.

How can we do this on code? Draw bresenham lines .... simply don't draw
certain pieces. In the end, you get a complete curve made of the lines!

put the points close enough together and .... you get a very smooth curve.

See Also

Art of Assembly, by Michael Abrash
8088MPH demo, by Hornet & CRTC & DESiRE
Dr Norman Wilderger's youtube channel


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

typedef struct cmap_subtable_format4_t {
	union uint16 format; //	Format number is set to 4
	union uint16 length; //	Length of subtable in bytes
	union uint16 language; //	Language code (see above)
	union uint16 segCountX2; //	2 * segCount
	union uint16 searchRange; //	2 * (2**FLOOR(log2(segCount)))
	union uint16 entrySelector; //	log2(searchRange/2)
	union uint16 rangeShift; //	(2 * segCount) - searchRange
	uint32_t endCodeArray; // uint16[segCount] 	Ending character code for each segment, last = 0xFFFF.
	union uint16 reservedPad; // 	This value should be zero
	uint32_t startCodeArray; // uint16[segCount] 	Starting character code for each segment
	uint32_t idDeltaArray; // uint16[segCount] 	Delta for all character codes in segment
	uint32_t idRangeOffsetArray; // uint16[segCount] Offset in bytes to glyph indexArray, or 0
	uint32_t glyphIndexArray; // uint16[variable]
} cmap_subtable_format4;

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
	union uint32 glyf_table_offset;
	union uint32 loca_table_offset;
	union uint32 head_table_offset;
	union int16  head_table_indexToLocFormat;
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
		} else if (equal(td.tag,"glyf")) {
			fi.glyf_table_offset = td.offset;
		} else if (equal(td.tag,"loca")) {
			fi.loca_table_offset = td.offset;
		} else if (equal(td.tag,"head")) {
			fi.head_table_offset = td.offset;
		}
	}
}

void read_head_table( fontinfo &fi, FILE * f ) {
	fseek( f, fi.head_table_offset.uint32, SEEK_SET ) ;
	// Number Types: Fixed = 32 bit
	// FWord 16 bit
	// uFWord 16 bit
	// longDateTime 64 bit

	fseek( f, 4, SEEK_CUR ); //version
	fseek( f, 4, SEEK_CUR ); //fontRevision
	fseek( f, 4, SEEK_CUR ); //chuckSumAdjustment
	//union uint32 tmp;      //magic number (for debug)
	//read_uint32( tmp, f );
	//printf("magic %08x\n",tmp.uint32); // sould be 0x5F0F3CF5
	fseek( f, 4, SEEK_CUR ); //magic number
	fseek( f, 2, SEEK_CUR ); //flags
	fseek( f, 2, SEEK_CUR ); //unitsPerEm
	fseek( f, 8, SEEK_CUR ); //time created
	fseek( f, 8, SEEK_CUR ); //time modified
	fseek( f, 2, SEEK_CUR ); //xmin
	fseek( f, 2, SEEK_CUR ); //ymin
	fseek( f, 2, SEEK_CUR ); //xmax
	fseek( f, 2, SEEK_CUR ); //ymax
	fseek( f, 2, SEEK_CUR ); //mac style
	fseek( f, 2, SEEK_CUR ); //lowestRecPPEM
	fseek( f, 2, SEEK_CUR ); //font diection hint
	read_int16( fi.head_table_indexToLocFormat, f );
	fseek( f, 2, SEEK_CUR ); //glyphDataFormat
}

// given 16 bit unicode, look up glyg index in a CMAP Format 4 table
// 32 bit unicode is not compatible with format 4 tables.
void lookup_cmap_format4( FILE * f, union uint32 &glyf_index, uint16_t &unicode16 ) {
	printf(" searching fmt4 table for unicode %u x%04x\n",unicode16,unicode16 );
	cmap_subtable_format4 fm;
	read_uint16( fm.length, f );
	read_uint16( fm.language, f );
	read_uint16( fm.segCountX2, f );
	read_uint16( fm.searchRange, f );
	read_uint16( fm.entrySelector, f );
	read_uint16( fm.rangeShift, f );
	printf( "length %u\n", fm.length.uint16 );
	printf( "language %u\n", fm.language.uint16 );
	printf( "segCountX2 %u\n", fm.segCountX2.uint16 );
	printf( "searchRange %u\n", fm.searchRange.uint16 );
	printf( "entrySelector %u\n", fm.entrySelector.uint16 );
	printf( "rangeShift %u\n", fm.rangeShift.uint16 );
	fm.endCodeArray = ftell( f );
	fm.startCodeArray = fm.endCodeArray + fm.segCountX2.uint16 + 2;
	// skip uint16 reservedPad
	fm.idDeltaArray = fm.startCodeArray + fm.segCountX2.uint16;
	fm.idRangeOffsetArray = fm.idDeltaArray + fm.segCountX2.uint16;
	fm.glyphIndexArray = fm.idRangeOffsetArray + fm.segCountX2.uint16;
	uint32_t endcode_i = fm.endCodeArray;
	uint32_t startcode_i = fm.startCodeArray;
	uint32_t iddelta_i = fm.idDeltaArray;
	uint32_t idrangeoffset_i = fm.idRangeOffsetArray;
	uint16_t segCount = fm.segCountX2.uint16 / 2;
	union uint16 endcode, startcode, iddelta, idrangeoffset;
	bool done = false;
	int counter = 0;
	while (!done) {
		counter++;
		if (counter>segCount) break;
		fseek( f, endcode_i, SEEK_SET );
		read_uint16( endcode, f );
		printf("end code x%04x\n",endcode.uint16);
		if (endcode.uint16 >= unicode16) {
			printf(" >= unicode \n");
			fseek( f, startcode_i, SEEK_SET );
			read_uint16( startcode, f );
			printf("start code x%04x\n",startcode.uint16);
			if (startcode.uint16 <= unicode16) {
				printf(" <= unicode \n");
				fseek( f, iddelta_i, SEEK_SET );
				read_uint16( iddelta, f );
				fseek( f, idrangeoffset_i, SEEK_SET );
				read_uint16( idrangeoffset, f );
				printf("end %x strt %x delt %u %x rof %x\n", endcode.uint16, startcode.uint16, iddelta.uint16, iddelta.uint16, idrangeoffset.uint16 );
				if (idrangeoffset.uint16==0) {
					printf( "rof 0, add delta to unicode\n");
					glyf_index.uint32 = ( iddelta.uint16 + unicode16 ) % 0x10000;
				} else {
					printf( "rof !0\n");
					union uint16 tmp;
					tmp.uint16 = idrangeoffset.uint16 + 2*(unicode16-startcode.uint16);
					fseek(f, idrangeoffset_i + tmp.uint16, SEEK_SET);
					read_uint16( tmp, f );
					glyf_index.uint32 = tmp.uint16;
				}
			}
			break;
		} else {
			printf(" keep looking..\n");
		}
		endcode_i+=2;
		startcode_i+=2;
		iddelta_i+=2;
		idrangeoffset_i+=2;
	}
}

// given a Unicode look up the glyf index
void lookup_glyf_index( fontinfo &fi, uint32_t &unicode32, union uint32 &glyf_index, FILE * f ) {

	// init glyf_index using MISSING CHARACTER standard index of 0.
	// if we can't find anything, it will be 0 on return.

	glyf_index.uint32 = 0;
	fseek( f, fi.cmap_table_offset.uint32, SEEK_SET );
	fseek( f, 2, SEEK_CUR ); //skip version
	union uint16 numberSubtables;
	union uint32 offset;
	read_uint16( numberSubtables, f );
	uint32_t subtable_i = ftell(f);
	union uint16 platformID, platformSpecificID;
	union uint16 format;
	for (uint16_t i=0;i<numberSubtables.uint16;i++ ) {
		fseek( f, subtable_i, SEEK_SET );
		read_uint16( platformID, f );
		read_uint16( platformSpecificID, f );
		read_uint32( offset, f );
		subtable_i = ftell(f);
		printf(" platID %i, platSpecID %02i, offset x%08x ",platformID.uint16,platformSpecificID.uint16, offset.uint32);
		bool ok = false;
		if (platformID.uint16==0) ok = true;
		else if (platformID.uint16==3 && platformSpecificID.uint16==10) ok = true;
		else if (platformID.uint16==3 && platformSpecificID.uint16==1) ok = true;
		if (ok) {
			fseek( f, fi.cmap_table_offset.uint32, SEEK_SET );
			fseek( f, offset.uint32, SEEK_CUR );
			read_uint16( format, f );
			printf("format %u\n",format.uint16);
			if (format.uint16==4) {
				if (unicode32>0xFFFF) {
					printf("unicode too big for format 4 table\n");
				}
				uint16_t unicode16 = unicode32;
				lookup_cmap_format4( f, glyf_index, unicode16 );
			}
			break;
		} else {
			printf("< cant parse this kind of cmap\n");
		}
	}
}

// given an glyph index, look up the byte offset from the begin of glyf table.
// for example, the 53rd glyph (index 52), could have a byte offset
// from the beginning of the glyph-data-table of 0x0004304.
// note there is no error checking for out of bounds.
void lookup_glyf_offset( fontinfo &fi, union uint32 &glyf_index, union uint32 &glyf_offset, FILE *f ) {
	fseek(f, fi.loca_table_offset.uint32, SEEK_SET);
	if (fi.head_table_indexToLocFormat.int16==0) {
		// LOC table is "short format".
		// each element of LOC array is 2 bytes long
		// and represents the offset in 16-bit words
		fseek(f, glyf_index.uint32 * 2, SEEK_CUR );
		union uint16 tmp;
		read_uint16( tmp, f );
		glyf_offset.uint32 = tmp.uint16 * 2;
	} else {
		// LOC table is "long format"
		// each element of LOC array is 4 bytes long
		// and represents the offset in 8-bit bytes
		fseek(f, glyf_index.uint32 * 4, SEEK_CUR );
		read_uint32( glyf_offset, f );
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
#define GF_ON_CURVE   0b00000001
#define GF_XSHORT_VEC 0b00000010
#define GF_YSHORT_VEC 0b00000100
#define GF_REPEAT     0b00001000
#define GF_X_IS_SAME  0b00010000
#define GF_Y_IS_SAME  0b00100000
#define GF_POS_XSHORT 0b00010000
#define GF_POS_YSHORT 0b00100000

void read_glyf_description( glyf_description &gd, FILE * f ) {
	read_int16(gd.numberOfContours,f);
	read_int16(gd.xMin,f);
	read_int16(gd.yMin,f);
	read_int16(gd.xMax,f);
	read_int16(gd.yMax,f);
}

void read_coord( uint32_t &offset, union int16 &delta, bool isshort, bool ispositive, FILE *f ) {
	fseek(f,offset,SEEK_SET);
	bool issame = ispositive; // two meanings, one number (see ttf docs)
	if (isshort) {
		uint8_t tmp;
		read_uint8( tmp, f );
		if (ispositive) delta.int16 = tmp; // cast 8bit to 16bit
		else delta.int16 = -tmp;           // cast 8bit to 16bit
	} else {
		if (issame) delta.int16 = 0;
		else read_int16( delta, f );
	}
	offset = ftell(f);
}

void read_x_coord( union int16 &xdelta, uint8_t &flag, uint32_t &x_i, FILE *f ){
	read_coord( x_i, xdelta, flag & GF_XSHORT_VEC, flag & GF_POS_XSHORT, f);
}

void read_y_coord( union int16 &ydelta, uint8_t &flag, uint32_t &y_i, FILE *f ){
	read_coord( y_i, ydelta, flag & GF_YSHORT_VEC, flag & GF_POS_YSHORT, f);
}

void printglyfflag( uint8_t &f ); // forward declaration, here until refactor

// read a flag. sounds easy but is complicated.
// flags are sort of 'run length encoded' so you have to deal with repeats (runs)
void readflag( uint8_t &flag, uint32_t &flags_i, uint8_t &repeat_counter, FILE *f ) {
	if (repeat_counter>0) {
		flag = flag;
		repeat_counter--;
	} else {
		fseek(f,flags_i,SEEK_SET);
		read_uint8( flag, f );
		if (flag & GF_REPEAT) read_uint8(repeat_counter,f);
		flags_i = ftell(f);
	}
}

// refacccctor
void do_glyf_data( glyf_description &gd, FILE *f, uint32_t glyfdataoffset ){
	// flag - explanation of 'xshort' and 'x-is-same/xshort-positive' bits.
	// bit1 bit4 result (truth table)
	// 1    0    x is 8bit value, sign is negative (9bit signed int)
	// 1    1    x is 8bit value, sign is positive (9bit signed int)
	// 0    0    x is 16 bit signed value
	// 0    1    x is the same as the previous x coordinate

	if (gd.numberOfContours.int16<0) return; // sient fail on compound glyfs
	fseek(f,glyfdataoffset,SEEK_SET);
	uint16_t num_points;

	// step one. figure out the offsets where x coords begin and
	// where y coords begin. how? read through all the flags.
	// where the flags end, x coordinates begin. what about y?
	// we can calculate how many bytes the x-coordinate list will use
	// as we process the flags. where the x-coords end, y-coords begin
	union uint16 endpt_index;
	for (int i=0;i<gd.numberOfContours.int16;i++) {
		read_uint16( endpt_index, f );
		printf("contour %i endpt idx: %i\n", i, endpt_index.uint16);
	}
	num_points = endpt_index.uint16+1;
	printf("total number of points: %i\n", num_points);

	// skip "instructions" section
	union uint16 instructionLength;
	read_uint16( instructionLength, f );
	fseek(f,instructionLength.uint16,SEEK_CUR);

	// read every flag, calculate the last x-coordinate. (which
	// is the beginning of the y coordinates). save index values (_i).
	uint32_t endpts_i = glyfdataoffset;
	uint32_t flags_offset = ftell(f);
	uint32_t flags_i = ftell(f);
	uint32_t x_i = ftell(f);
	uint32_t y_i = ftell(f);
	uint8_t xcoord_numbytes = 0;
	uint8_t flag = 0;
	uint8_t repeat_counter = 0;
	uint16_t xcoordlist_numbytes = 0;
	for (int i = 0; i < num_points; i++) {
		readflag( flag, flags_i, repeat_counter, f );
		if (flag & GF_XSHORT_VEC) xcoord_numbytes = 1;
		else if (flag & GF_X_IS_SAME) xcoord_numbytes = 0; //xcoord_numbytes;
		else xcoord_numbytes = 2;
		xcoordlist_numbytes += xcoord_numbytes;
	}
	// now we are at end of flags.
	// mark that this is where x coords start.
	x_i = ftell(f);
	// we have calculated size of x coordinate list, and can calculate
	// beginning of y coordinate list.
	y_i = x_i + xcoordlist_numbytes;
	// also reset the flags index
	flags_i = flags_offset;

	// step two. we have flag_i, x_i, y_i all set up. process the points.
	int16_t xcursor,ycursor;
	union int16 xdelta,ydelta;
	glyf_point gp;
	printf("idx endp %i flg %i x %i y %i\n",endpts_i,flags_i,x_i,y_i);
	union uint16 prev_endpt_index;
	prev_endpt_index.uint16 = 0;
	for (int i=0;i<gd.numberOfContours.int16;i++){
		fseek( f, endpts_i, SEEK_SET );
		read_uint16( endpt_index, f );
		endpts_i = ftell(f);
		repeat_counter = 0;
		for (int j=prev_endpt_index.uint16;j<endpt_index.uint16+1;j++) {
			readflag( flag, flags_i, repeat_counter, f );
			read_x_coord( xdelta, flag, x_i, f );
			read_y_coord( ydelta, flag, y_i, f );
			printf("idxes: endp %i flg %i x %i y %i\n",endpts_i,flags_i,x_i,y_i);
			printf("-\\\n");
			printf("contour idx %i point idx %i\n",i,j);
			printglyfflag( flag );
			printf("xdelta %i \t ydelta %i \n", xdelta.int16, ydelta.int16 );
			printf("-/\n");
		}
		prev_endpt_index.uint16 = endpt_index.uint16+1;
	}
}

#ifdef DEBUG
void printglyfflag( uint8_t &f ){
	printf("GF_ON_CURVE   %i\n", f & GF_ON_CURVE );
	printf("GF_XSHORT_VEC %i\n", f & GF_XSHORT_VEC );
	printf("GF_YSHORT_VEC %i\n", f & GF_YSHORT_VEC );
	printf("GF_REPEAT     %i\n", f & GF_REPEAT );
	if (f&GF_XSHORT_VEC) {
		printf("GF_POS_XSHORT %i\n", f & GF_POS_XSHORT );
		printf("GF_X_IS_SAME  n/a for 8bit coord\n" );
	} else {
		printf("GF_POS_XSHORT n/a for 16 bit coord\n" );
		printf("GF_X_IS_SAME  %i\n", f & GF_X_IS_SAME );
	}
	if (f&GF_YSHORT_VEC) {
		printf("GF_POS_YSHORT %i\n", f & GF_POS_YSHORT );
		printf("GF_Y_IS_SAME  n/a for 8bit coord\n" );
	} else {
		printf("GF_POS_YSHORT n/a for 16 bit coord\n" );
		printf("GF_Y_IS_SAME  %i\n", f & GF_Y_IS_SAME );
	}
}

void printinfo( fontinfo &f ){
	printf("ofascaler %u\n",f.ofascaler.uint32);
	printf("numtables %u\n",f.numtables.uint16);
	printf("cmap table offset hex %08x\n",f.cmap_table_offset.uint32);
	printf("glyf table offset hex %08x\n",f.glyf_table_offset.uint32);
	printf("loca table offset hex %08x\n",f.loca_table_offset.uint32);
	printf("head table offset hex %08x\n",f.head_table_offset.uint32);
	printf("head table indexToLocFormat %i\n",f.head_table_indexToLocFormat.int16);
	if (f.head_table_indexToLocFormat.int16==0)
		printf("  locformat: short (offsets=number of 16-bit words)\n");
	else if (f.head_table_indexToLocFormat.int16==1)
		printf("  locformat: long  (offsets=number of 8-bit bytes)\n");
	else
		printf("  locformat: unknown, not 0 or 1 \n");
}

void printglyfdescr( glyf_description &gd ){
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

	read_head_table( fi, file );
	printinfo( fi );

	uint32_t unicode = 65;
	//uint32_t unicode = 0x20d5;
	union uint32 glyf_index;
	lookup_glyf_index( fi, unicode, glyf_index, file );

	union uint32 glyf_offset;
	lookup_glyf_offset( fi, glyf_index, glyf_offset, file );
	printf("index, %u x%x, offset, x%x\n", glyf_index.uint32, glyf_index.uint32, glyf_offset.uint32 );
	fseek( file, fi.glyf_table_offset.uint32, SEEK_SET);
	printf("gt offqset, %x\n", fi.glyf_table_offset.uint32 );
	fseek( file, glyf_offset.uint32, SEEK_CUR);
	glyf_description gd;
	read_glyf_description( gd, file );
	printglyfdescr( gd );
	do_glyf_data( gd, file, ftell(file) );

	return 0;
}
