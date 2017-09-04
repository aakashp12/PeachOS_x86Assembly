/*									tab:8
 *
 * text.h - font data and text to mode X conversion utility header file
 *
 * "Copyright (c) 2004-2009 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 * 
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO 
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL 
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, 
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE 
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE, 
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:	    Steve Lumetta
 * Version:	    2
 * Creation Date:   Thu Sep  9 22:08:16 2004
 * Filename:	    text.h
 * History:
 *	SL	1	Thu Sep  9 22:08:16 2004
 *		First written.
 *	SL	2	Sat Sep 12 13:40:11 2009
 *		Integrated original release back into main code base.
 */

#ifndef TEXT_H
#define TEXT_H

/* The default VGA text mode font is 8x16 pixels. */
#define FONT_WIDTH   8
#define FONT_HEIGHT 16

/* Standard VGA text font. */
extern unsigned char font_data[256][16];

/*
    text to graphics image generation routine.  
    Given a string, it should produce a buffer 
    that holds the graphical image of the ASCII characters in a string. 
    height of the buffer is fixed.
    either:
    a)figure out how much text fits on the status bar and produce a buffer of fixed size
    b)return the resulting image width
    in either case:
    the test must be centered on the bar
    Basically were drawing this thing in the buffer
    //put in one of two colors (color 1 or color 2)
    //depending on which bit of holder we're on
    
    INPUT:
        const char * string  -->string were reading in
    Writes to buffer given the correct index for the buffer
    
*/

/*
 *   textToScreen
 *   DESCRIPTION: We know the height and width of the status bar, so we use that info
 *				  To fill the status bar with background color info. Then we access the
 *				  The individual ascii character and the pixel values of  8 x 16 block and
 * 				  Then find if we should fill in a color for the specific pixel. That is 
 *				  How we turn text to Image. 
 *   INPUTS: text_color -- used as different color for the status bar text
 *           background_color -- used for background color. We fill the whole status bar with this first
 *			 string -- The string that has to be output on the screen
 *			 buffer -- An array. It's used to put in the pixel colors.
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: buffer is full of color values.
 */
extern void textToScreen(unsigned char text_color, unsigned char background_color, const char* string, unsigned char* buffer);

#endif /* TEXT_H */
