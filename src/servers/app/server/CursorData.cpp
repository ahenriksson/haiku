//------------------------------------------------------------------------------
//	Copyright (c) 2001-2002, OpenBeOS
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		CursorData.cpp
//	Author:			DarkWyrm <bpmagic@columbus.rr.com>
//	Description:	File containing default cursor data
//  
//------------------------------------------------------------------------------
#include <SupportDefs.h>

/*
The first four bytes of cursor data give information about the cursor: 

Byte 1: Size in pixels-per-side. Cursors are always square; currently, only 16-by-16 
cursors are allowed. 
Byte 2: Color depth in bits-per-pixel. Currently, only one-bit monochrome is allowed. 
Bytes 3&4: Hot spot coordinates. Given in "cursor coordinates" where (0,0) is the upper 
left corner of the cursor grid, byte 3 is the hot spot's y coordinate, and byte 4 is its x 
coordinate. The hot spot is a single pixel that's "activated" when the user clicks the mouse. 
To push a button, for example, the hot spot must be within the button's bounds. 

Next comes an array of pixel color data. Pixels are specified from left to right in rows starting at 
the top of the image and working downward. The size of an array element is the depth of the 
image. In one-bit depth... 

the 16x16 array fits in 32 bytes. 
1 is black and 0 is white. 

Then comes the pixel transparency bitmask, given left-to-right and top-to-bottom. 1 is opaque, 0 is 
transparent. Transparency only applies to white pixels—black pixels are always opaque. 
*/

// Impressive ASCII art, eh?

int8 default_cursor_data[] = {
16,1,0,0,
255,224,	// ***********-----
128,16,		// *----------*----
128,16,		// *----------*----
128,96,		// *--------**-----
128,16,		// *----------*----
128,8,		// *-----------*---
128,8,		// *-----------*---
128,16,		// *----------*----
128,32,		// *---------*-----
144,64,		// *--*-----*------
144,128,	// *--*----*-------
105,0,		// -**-*--*--------
6,0,		// -----**---------

0,0,		// ----------------
0,0,		// ----------------
0,0,		// ----------------

// default_cursor mask - black pixels are always opaque
255,224,
255,240,
255,240,
255,224,
255,240,
255,248,
255,248,
255,240,
255,224,
255,192,
255,128,
111,0,
6,0,

0,0,
0,0,
0,0
};

int8 default_text_data[] = {
	0x10, 0x01, 0x07, 0x07, 0x00, 0x00, 0x06, 0xC0, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x06, 0xC0, 0x00, 0x00, 0x06, 0xC0, 0x0F, 0xE0, 0x07, 0xC0, 0x03, 0x80,
	0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x07, 0xC0, 0x0F, 0xE0,
	0x06, 0xC0
};

int8 default_move_data[] = {
	0x10, 0x01, 0x07, 0x07, 0x00, 0x00, 0x01, 0x00, 0x03, 0x80, 0x07, 0xC0, 0x00, 0x00, 0x10, 0x10, 0x30, 0x18, 0x71, 0x1C, 0x30, 0x18,
	0x10, 0x10, 0x00, 0x00, 0x07, 0xC0, 0x03, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x80, 0x07, 0xC0, 0x0F, 0xE0,
	0x1F, 0xF0, 0x38, 0x38, 0x7B, 0xBC, 0xFB, 0xBE, 0x7B, 0xBC, 0x38, 0x38, 0x1F, 0xF0, 0x0F, 0xE0, 0x07, 0xC0, 0x03, 0x80, 0x01, 0x00,
	0x00, 0x00
};

int8 default_drag_data[] = {
	0x10, 0x01, 0x00, 0x00, 0xFF, 0x00, 0x81, 0x00, 0x82, 0x00, 0x82, 0x00, 0x81, 0xFF, 0x80, 0x81, 0xB0, 0x41, 0xC8, 0x41, 0x04, 0x81,
	0x03, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02, 0x01, 0x03, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFE, 0x00, 0xFE, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xCF, 0xFF, 0x07, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF,
	0x03, 0xFF
};

int8 default_resize_data[] = {
	0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x3E, 0x78, 0x1E, 0x70, 0x0E, 0x68, 0x16, 0x44, 0x22, 0x02, 0x40, 0x01, 0x80, 0x01, 0x80,
	0x02, 0x40, 0x44, 0x22, 0x68, 0x16, 0x70, 0x0E, 0x78, 0x1E, 0x7C, 0x3E, 0x00, 0x00, 0xFE, 0x7F, 0xFE, 0x7F, 0xFC, 0x3F, 0xF8, 0x1F,
	0xFC, 0x3F, 0xEE, 0x77, 0xC7, 0xE3, 0x03, 0xC0, 0x03, 0xC0, 0xC7, 0xE3, 0xEE, 0x77, 0xFC, 0x3F, 0xF8, 0x1F, 0xFC, 0x3F, 0xFE, 0x7F,
	0xFE, 0x7F
};

int8 default_resize_ew_data[] = {
	0x10, 0x01, 0x07, 0x07, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x11, 0x10, 0x31, 0x18, 0x71, 0x1C, 0x71, 0x1C,
	0x31, 0x18, 0x11, 0x10, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x80, 0x03, 0x80, 0x0B, 0xA0,
	0x1B, 0xB0, 0x3B, 0xB8, 0x7B, 0xBC, 0xFB, 0xBE, 0xFB, 0xBE, 0x7B, 0xBC, 0x3B, 0xB8, 0x1B, 0xB0, 0x0B, 0xA0, 0x03, 0x80, 0x03, 0x80,
	0x01, 0x00
};

int8 default_resize_ns_data[] = {
	0x10, 0x01, 0x07, 0x07, 0x00, 0x00, 0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFE, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0,
	0x1F, 0xF8, 0x00, 0x00, 0x7F, 0xFE, 0xFF, 0xFF, 0x7F, 0xFE, 0x00, 0x00, 0x1F, 0xF8, 0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80,
	0x00, 0x00
};

int8 default_resize_nwse_data[] = {
	0x10, 0x01, 0x08, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x08, 0x1E, 0x10, 0x1C, 0x20, 0x18, 0x40, 0x10, 0x80, 0x01, 0x08,
	0x02, 0x18, 0x04, 0x38, 0x08, 0x78, 0x10, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xCC, 0x3F, 0x9C,
	0x3F, 0x38, 0x3E, 0x70, 0x3C, 0xE4, 0x39, 0xCC, 0x33, 0x9C, 0x27, 0x3C, 0x0E, 0x7C, 0x1C, 0xFC, 0x39, 0xFC, 0x33, 0xFC, 0x00, 0x00,
	0x00, 0x00
};

int8 default_resize_nesw_data[] = {
	0x10, 0x01, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xF8, 0x08, 0x78, 0x04, 0x38, 0x02, 0x18, 0x01, 0x08, 0x10, 0x80,
	0x18, 0x40, 0x1C, 0x20, 0x1E, 0x10, 0x1F, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xFC, 0x39, 0xFC,
	0x1C, 0xFC, 0x0E, 0x7C, 0x27, 0x3C, 0x33, 0x9C, 0x39, 0xCC, 0x3C, 0xE4, 0x3E, 0x70, 0x3F, 0x38, 0x3F, 0x9C, 0x3F, 0xCC, 0x00, 0x00,
	0x00, 0x00
};


// known good cursor data for testing rest of system
int8 cross_cursor[] = {16,1,5,5,
14,0,4,0,4,0,4,0,128,32,241,224,128,32,4,0,
4,0,4,0,14,0,0,0,0,0,0,0,0,0,0,0,
14,0,4,0,4,0,4,0,128,32,245,224,128,32,4,0,
4,0,4,0,14,0,0,0,0,0,0,0,0,0,0,0
};
