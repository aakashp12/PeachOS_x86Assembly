/*									tab:8
 *
 * mazegame.c - main source file for ECE398SSL maze game (F04 MP2)
 *
 * "Copyright (c) 2004 by Steven S. Lumetta."
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
 * Version:	    1
 * Creation Date:   Fri Sep 10 09:57:54 2004
 * Filename:	    mazegame.c
 * History:
 *	SL	1	Fri Sep 10 09:57:54 2004
 *		First written.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "module/tuxctl-ioctl.h"
#include "blocks.h"
#include "maze.h"
#include "modex.h"
#include "text.h"

// New Includes and Defines
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/io.h>
#include <termios.h>
#include <pthread.h>


#define BACKQUOTE 96

#define UP 65
#define DOWN 66
#define RIGHT 67
#define LEFT 68

// MACROS for buttons
#define T_UP 0x10
#define T_DOWN 0x20
#define T_LEFT 0x40
#define T_RIGHT 0x80
#define T_LED_ALL 0x000F0000 // set 4 leds on
#define T_LED_3 0x00070000 // set 3 leds on
#define T_DEC 0x04000000 // set decimal point for time

#define TIMER_FRUIT 5

#define APPLE 1
#define GRAPE 2
#define WHITE_PEACH 3
#define STAWBERRY 4
#define BANANA 5
#define WATERMELON 6
#define HONEY_DEW 7
#define HIDDEN_FRUIT 8


/*
 * If NDEBUG is not defined, we execute sanity checks to make sure that
 * changes to enumerations, bit maps, etc., have been made consistently.
 */
#if defined(NDEBUG)
#define sanity_check() 0
#else
static int sanity_check ();
#endif


/* a few constants */
#define PAN_BORDER      5  /* pan when border in maze squares reaches 5    */
#define MAX_LEVEL      10  /* maximum level number                         */

/* outcome of each level, and of the game as a whole */
typedef enum {GAME_WON, GAME_LOST, GAME_QUIT} game_condition_t;


/* structure used to hold game information */
typedef struct {
    /* parameters varying by level   */
    int number;                  /* starts at 1...                   */
    int maze_x_dim, maze_y_dim;  /* min to max, in steps of 2        */
    int initial_fruit_count;     /* 1 to 6, in steps of 1/2          */
    int time_to_first_fruit;     /* 300 to 120, in steps of -30      */
    int time_between_fruits;     /* 300 to 60, in steps of -30       */
    int tick_usec;		 /* 20000 to 5000, in steps of -1750 */

    /* dynamic values within a level -- you may want to add more... */
    unsigned int map_x, map_y;   /* current upper left display pixel */
} game_info_t;

static game_info_t game_info;


/* local functions--see function headers for details */
static int prepare_maze_level (int level);
static void move_up (int* ypos);
static void move_right (int* xpos);
static void move_down (int* ypos);
static void move_left (int* xpos);
static int unveil_around_player (int play_x, int play_y);
static void *rtc_thread(void *arg);
static void *keyboard_thread(void *arg);
static void *tux_thread(void *arg);

/*
 * prepare_maze_level
 *   DESCRIPTION: Prepare for a maze of a given level.  Fills the game_info
 *		  structure, creates a maze, and initializes the display.
 *   INPUTS: level -- level to be used for selecting parameter values
 *   OUTPUTS: none
 *   RETURN VALUE: 0 on success, -1 on failure
 *   SIDE EFFECTS: writes entire game_info structure; changes maze;
 *                 initializes display
 */
static int
prepare_maze_level (int level)
{
    int i; /* loop index for drawing display */

    /*
     * Record level in game_info; other calculations use offset from
     * level 1.
     */
    game_info.number = level--;

    /* Set per-level parameter values. */
    if ((game_info.maze_x_dim = MAZE_MIN_X_DIM + 2 * level) >
	MAZE_MAX_X_DIM)
	game_info.maze_x_dim = MAZE_MAX_X_DIM;
    if ((game_info.maze_y_dim = MAZE_MIN_Y_DIM + 2 * level) >
	MAZE_MAX_Y_DIM)
	game_info.maze_y_dim = MAZE_MAX_Y_DIM;
    if ((game_info.initial_fruit_count = 1 + level / 2) > 6)
	game_info.initial_fruit_count = 6;
    if ((game_info.time_to_first_fruit = 300 - 30 * level) < 120)
	game_info.time_to_first_fruit = 120;
    if ((game_info.time_between_fruits = 300 - 60 * level) < 60)
	game_info.time_between_fruits = 60;
    if ((game_info.tick_usec = 20000 - 1750 * level) < 5000)
	game_info.tick_usec = 5000;

    /* Initialize dynamic values. */
    game_info.map_x = game_info.map_y = SHOW_MIN;

    /* Create a maze. */
    if (make_maze (game_info.maze_x_dim, game_info.maze_y_dim,
		   game_info.initial_fruit_count) != 0)
	return -1;

    /* Set logical view and draw initial screen. */
    set_view_window (game_info.map_x, game_info.map_y);
    for (i = 0; i < SCROLL_Y_DIM; i++)
	(void)draw_horiz_line (i);

    /* Return success. */
    return 0;
}


/*
 * move_up
 *   DESCRIPTION: Move the player up one pixel (assumed to be a legal move)
 *   INPUTS: ypos -- pointer to player's y position (pixel) in the maze
 *   OUTPUTS: *ypos -- reduced by one from initial value
 *   RETURN VALUE: none
 *   SIDE EFFECTS: pans display by one pixel when appropriate
 */
static void
move_up (int* ypos)
{
    /*
     * Move player by one pixel and check whether display should be panned.
     * Panning is necessary when the player moves past the upper pan border
     * while the top pixels of the maze are not on-screen.
     */
    if (--(*ypos) < game_info.map_y + BLOCK_Y_DIM * PAN_BORDER &&
	game_info.map_y > SHOW_MIN) {
	/*
	 * Shift the logical view upwards by one pixel and draw the
	 * new line.
	 */
	set_view_window (game_info.map_x, --game_info.map_y);
	(void)draw_horiz_line (0);
    }
}


/*
 * move_right
 *   DESCRIPTION: Move the player right one pixel (assumed to be a legal move)
 *   INPUTS: xpos -- pointer to player's x position (pixel) in the maze
 *   OUTPUTS: *xpos -- increased by one from initial value
 *   RETURN VALUE: none
 *   SIDE EFFECTS: pans display by one pixel when appropriate
 */
static void
move_right (int* xpos)
{
    /*
     * Move player by one pixel and check whether display should be panned.
     * Panning is necessary when the player moves past the right pan border
     * while the rightmost pixels of the maze are not on-screen.
     */
    if (++(*xpos) > game_info.map_x + SCROLL_X_DIM -
	    BLOCK_X_DIM * (PAN_BORDER + 1) &&
	game_info.map_x + SCROLL_X_DIM <
	    (2 * game_info.maze_x_dim + 1) * BLOCK_X_DIM - SHOW_MIN) {
	/*
	 * Shift the logical view to the right by one pixel and draw the
	 * new line.
	 */
	set_view_window (++game_info.map_x, game_info.map_y);
	(void)draw_vert_line (SCROLL_X_DIM - 1);
    }
}


/*
 * move_down
 *   DESCRIPTION: Move the player right one pixel (assumed to be a legal move)
 *   INPUTS: ypos -- pointer to player's y position (pixel) in the maze
 *   OUTPUTS: *ypos -- increased by one from initial value
 *   RETURN VALUE: none
 *   SIDE EFFECTS: pans display by one pixel when appropriate
 */
static void
move_down (int* ypos)
{
    /*
     * Move player by one pixel and check whether display should be panned.
     * Panning is necessary when the player moves past the right pan border
     * while the bottom pixels of the maze are not on-screen.
     */
    if (++(*ypos) > game_info.map_y + SCROLL_Y_DIM -
	    BLOCK_Y_DIM * (PAN_BORDER + 1) &&
	game_info.map_y + SCROLL_Y_DIM <
	    (2 * game_info.maze_y_dim + 1) * BLOCK_Y_DIM - SHOW_MIN) {
	/*
	 * Shift the logical view downwards by one pixel and draw the
	 * new line.
	 */
	set_view_window (game_info.map_x, ++game_info.map_y);
	(void)draw_horiz_line (SCROLL_Y_DIM - 1);
    }
}


/*
 * move_left
 *   DESCRIPTION: Move the player right one pixel (assumed to be a legal move)
 *   INPUTS: xpos -- pointer to player's x position (pixel) in the maze
 *   OUTPUTS: *xpos -- decreased by one from initial value
 *   RETURN VALUE: none
 *   SIDE EFFECTS: pans display by one pixel when appropriate
 */
static void
move_left (int* xpos)
{
    /*
     * Move player by one pixel and check whether display should be panned.
     * Panning is necessary when the player moves past the left pan border
     * while the leftmost pixels of the maze are not on-screen.
     */
    if (--(*xpos) < game_info.map_x + BLOCK_X_DIM * PAN_BORDER &&
	game_info.map_x > SHOW_MIN) {
	/*
	 * Shift the logical view to the left by one pixel and draw the
	 * new line.
	 */
	set_view_window (--game_info.map_x, game_info.map_y);
	(void)draw_vert_line (0);
    }
}


/*
 * unveil_around_player
 *   DESCRIPTION: Show the maze squares in an area around the player.
 *                Consume any fruit under the player.  Check whether
 *                player has won the maze level.
 *   INPUTS: (play_x,play_y) -- player coordinates in pixels
 *   OUTPUTS: none
 *   RETURN VALUE: 1 if player wins the level by entering the square
 *                 0 if not
 *   SIDE EFFECTS: draws maze squares for newly visible maze blocks,
 *                 consumed fruit, and maze exit; consumes fruit and
 *                 updates displayed fruit counts
 */
static int
unveil_around_player (int play_x, int play_y)
{
    int x = play_x / BLOCK_X_DIM; /* player's maze lattice position */
    int y = play_y / BLOCK_Y_DIM;
    int i, j;   /* loop indices for unveiling maze squares */

    /* Check for fruit at the player's position. */
    (void)check_for_fruit (x, y);

    /* Unveil spaces around the player. */
    for (i = -1; i < 2; i++)
	for (j = -1; j < 2; j++)
	    unveil_space (x + i, y + j);
    unveil_space (x, y - 2);
    unveil_space (x + 2, y);
    unveil_space (x, y + 2);
    unveil_space (x - 2, y);

    /* Check whether the player has won the maze level. */
    return check_for_win (x, y);
}


#if !defined(NDEBUG)
/*
 * sanity_check
 *   DESCRIPTION: Perform checks on changes to constants and enumerated values.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: 0 if checks pass, -1 if any fail
 *   SIDE EFFECTS: none
 */
static int
sanity_check ()
{
    /*
     * Automatically detect when fruits have been added in blocks.h
     * without allocating enough bits to identify all types of fruit
     * uniquely (along with 0, which means no fruit).
     */
    if (((2 * LAST_MAZE_FRUIT_BIT) / MAZE_FRUIT_1) < NUM_FRUIT_TYPES + 1) {
	puts ("You need to allocate more bits in maze_bit_t to encode fruit.");
	return -1;
    }
    return 0;
}
#endif /* !defined(NDEBUG) */


// Shared Global Variables
int quit_flag = 0;
int winner= 0;
int next_dir = UP;
int play_x, play_y, last_dir, dir;
int move_cnt = 0;
int fd, fd_tux;
unsigned long data;
static struct termios tio_orig;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;


/*
 * keyboard_thread
 *   DESCRIPTION: Thread that handles keyboard inputs
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void *keyboard_thread(void *arg)
{
	char key;
	int state = 0;
	// Break only on win or quit input - '`'
	while (winner == 0)
	{
		// Get Keyboard Input
		key = getc(stdin);

		// Check for '`' to quit
		if (key == BACKQUOTE)
		{
			quit_flag = 1;
			break;
		}

		// Compare and Set next_dir
		// Arrow keys deliver 27, 91, ##
		if (key == 27)
		{
			state = 1;
		}
		else if (key == 91 && state == 1)
		{
			state = 2;
		}
		else
		{
			if (key >= UP && key <= LEFT && state == 2)
			{
				pthread_mutex_lock(&mtx);
				switch(key)
				{
					case UP:
						next_dir = DIR_UP;
						break;
					case DOWN:
						next_dir = DIR_DOWN;
						break;
					case RIGHT:
						next_dir = DIR_RIGHT;
						break;
					case LEFT:
						next_dir = DIR_LEFT;
						break;
				}
				pthread_mutex_unlock(&mtx);
			}
			state = 0;
		}
	}

	return 0;
}

/*
 * tux_thread
 *   DESCRIPTION: Thread that handles TUX inputs
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void* tux_thread(void *arg)
{
	// Break only on win or quit input - '`'
	while (winner == 0)
	{

        if(quit_flag == 1)
        {
            break;
        }
        unsigned long key = 0;
        ioctl(fd_tux, TUX_BUTTONS, &key);
        key &= 0xFF;

		pthread_mutex_lock(&mtx);
		switch(key)
		{
			case T_UP:
				next_dir = DIR_UP;
				break;
			case T_DOWN:
				next_dir = DIR_DOWN;
				break;
			case T_RIGHT:
				next_dir = DIR_RIGHT;
				break;
			case T_LEFT:
				next_dir = DIR_LEFT;
				break;
		}
		pthread_mutex_unlock(&mtx);
	}
	return 0;
}
/*
 * tux_set_time
 * DESCRIPTION : Set time on the led via tux driver
 * input: time
 * output: none
 * side effect: LED shows time
*/
static void tux_set_time(int tux_time)
{
	unsigned long input = 0;
	int seconds;
	int minutes;

    // set time
	seconds = tux_time % 60;
	minutes = tux_time / 60;


	if(minutes >= 10)
	{
		input = T_LED_ALL; // 0000 0100 0000 1111 ... turns on all LED
        input |= T_DEC;
	}
	else
	{
		input = T_LED_3; // 0000 0100 0000 0111 ... turns on 2 1 0 LED
        input |= T_DEC;
		//light up all leds
	}

	// set LED 0
	input |= (seconds % 10);
	// set LED 1 ...shift 4
	input |=  ((seconds / 10) << 4);

	// set LED 2 ...shift 8
	input |= ((minutes % 10) <<8);

	// set LED 3 ...shift 12
	input |= ((minutes / 10) << 12);

	// send argument to tux
	ioctl(fd_tux, TUX_SET_LED, input);
    return;
}

/* some stats about how often we take longer than a single timer tick */
static int goodcount = 0;
static int badcount = 0;
static int total = 0;


/*
 * rtc_thread
 *   DESCRIPTION: Thread that handles updating the screen
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void *rtc_thread(void *arg)
{
	int ticks = 0;
	int level;
	int ret;
	int open[NUM_DIRS];
	int need_redraw = 0;
	int goto_next_level = 0;

	// Loop over levels until a level is lost or quit.
	for (level = 1; (level <= MAX_LEVEL) && (quit_flag == 0); level++)
	{
		// Prepare for the level.  If we fail, just let the player win.
		if (prepare_maze_level (level) != 0)
			break;
		goto_next_level = 0;

		// Start the player at (1,1)
		play_x = BLOCK_X_DIM;
		play_y = BLOCK_Y_DIM;

		// move_cnt tracks moves remaining between maze squares.
		// When not moving, it should be 0.
		move_cnt = 0;

		// Initialize last diresdasdction moved to up
		last_dir = DIR_UP;

		// Initialize the current direction of motion to stopped
		dir = DIR_STOP;
		next_dir = DIR_STOP;

		// Show maze around the player's original position
		(void)unveil_around_player (play_x, play_y);

		/*--- Call the draw_mask_block first and then draw_full_block ---*/
		//unsigned char old_background[BLOCK_X_DIM * BLOCK_Y_DIM]; /* 144 because the block is 12 pixels by 12 pixels. So we need buffer of 144 pixels to hold values */
		int x;
		x = play_x;
		int y;
		y = play_y;


		draw_mask_block(play_x, play_y, get_player_block(last_dir), get_player_mask(last_dir)); // get the whole background
		show_screen();
		draw_full_block(play_x, play_y, old_values);


		/* ------------------ START COLOR STUFF ------------------ */

		/* These 2 char array will be used to change Wall/FILL/PLAYER color */
		unsigned char dark_color[10];
		unsigned char light_color[10];
		int color = 0;
		/* Populate the dark_color and light_color*/
		for(color = 0; color < 10; color++)
		{
			dark_color[color] = 125 + ((color + 1) * 12.5);
		}

		color = 0;
		for(color = 10; color > 0; color--)
		{
			light_color[color] = 125 - ((color + 1) * 12.5);
		}

		/* Now call the set_palette fnx to set these values onto VGA memory.. ports..*/
		/* VGA ports... 0x3c6 VGA video DAC PEL mask, 0x3C8 DAC PEL address, 0x3C9 DAC */

		/* These 2 int values used for text_color and background_color for textToScreen fnx */
		int status_text;
		int status_background;

		/* Array of pre-chosen values of color for status_text and status_background */
		int array_text[10] = {0x0A, 0x09, 0x12, 0x09, 0x23, 0x30, 0x01, 0x35, 0x0B, 0x01};
		int array_background[10] = {0x0C, 0x35, 0x3F, 0x3D, 0x02, 0x03, 0x0B ,0x09, 0x23, 0x05};

		/* Set the status_text and status_background to the array */
		status_text = array_text[level - 1];
		status_background = array_background[level - 1];

		/* Set new colors everytime there's a new level*/
		unsigned char backWall_red = light_color[level]; // * 10 % 160; /* Using light_color to make sure our pixel's are more green  */
		unsigned char backWall_green = dark_color[level]; // * 10 % 250;
		unsigned char backWall_blue = light_color[level]; //* 10 % 150;

		unsigned char wall_red = dark_color[level] * 5 % 250; /* Using dark_color to make sure we get the higher end of the spectrum */
		unsigned char wall_green = dark_color[level * 5 % 160];
		unsigned char wall_blue = dark_color[level] * 5 % 150;
		palette_color_selector(backWall_red, backWall_green, backWall_blue, WALL_FILL_COLOR); /* This is the backWall */
		palette_color_selector(wall_red, wall_green, wall_blue, WALL_OUTLINE_COLOR); /* This is the wall */

		/* ------------------ DONE WITH COLOR STUFF ------------------ */

		/* ------------------ START WITH FRUIT STUFF ------------------ */

		int fruit_identifier = 0; /* Will be used later to identify the fruit */
		char fruit_name[26]; /* Store the fruit name here */

		fruit_identifier = check_for_fruit(play_x, play_y);
		if(fruit_identifier != 0) /* -- If return value is 0 then it means we aren't on a fruit -- */
		{
			switch(fruit_identifier)
			{					/* -- Convert from fruit # to string -- */
				case APPLE:
					sprintf(fruit_name, " Apple Pen Pineapple Pen ");
				case GRAPE:
					sprintf(fruit_name, " You Look Grape ;) ");
				case WHITE_PEACH:
					sprintf(fruit_name, " How Are You? Peachy :P ");
				case STAWBERRY:
					sprintf(fruit_name, " You Are Berry Special ");
				case BANANA:
					sprintf(fruit_name, " Ba Ba Ba Na Na Na ");
				case WATERMELON:
					sprintf(fruit_name, " Just A Watermelon! ");
				case HONEY_DEW:
					sprintf(fruit_name, " Cant-Elope ");
				case HIDDEN_FRUIT:
					sprintf(fruit_name, " Found The Hidden Chest ");
			}						/* -- Send this string to draw_Text_Block to put it on the screen... similiar to status_bar functions -- */
		}

		/* ------------------ DONE WITH FRUIT STUFF ------------------ */

		// get first Periodic Interrupt
		ret = read(fd, &data, sizeof(unsigned long));



		while ((quit_flag == 0) && (goto_next_level == 0))
		{
			// Wait for Periodic Interrupt
			ret = read(fd, &data, sizeof(unsigned long));

			/* ------------------ START FRUIT STUFF ------------------ */
			int fruits = 0;
			fruits = getFruits(); /* Helper function to get fruit value in MAZE.C*/
			/* ------------------ DONE FRUIT STUFF ------------------ */


			// Update tick to keep track of time.  If we missed some
			// interrupts we want to update the player multiple times so
			// that player velocity is smooth
			ticks = data >> 8;

			total += ticks;

			/* ------------------ FINALLY CALLED FNX ------------------ */
			showStatus_bar(status_text, status_background, level, fruits, (total / 32) ); // ADDED
            tux_set_time(total / 32); // get the led to display TIME

			/* Set new color for the player's head every level */
			unsigned char playerBall_red = dark_color[total / 25 % 10];
			unsigned char playerBall_green = light_color[total / 25 % 10];
			unsigned char playerBall_blue = light_color[total / 25 % 10];
			palette_color_selector(playerBall_red, playerBall_green, playerBall_blue, PLAYER_CENTER_COLOR); /* This is the player */

			// If the system is completely overwhelmed we better slow down:
			if (ticks > 1) ticks = 1;

			if (ticks > 1) {
				badcount++;
			}
			else {
				goodcount++;
			}

			while (ticks--)
			{

				// Lock the mutex
				pthread_mutex_lock(&mtx);

				// Check to see if a key has been pressed
				if (next_dir != dir)
				{
					// Check if new direction is backwards...if so, do immediately
					if ((dir == DIR_UP && next_dir == DIR_DOWN) ||
					    (dir == DIR_DOWN && next_dir == DIR_UP) ||
     					    (dir == DIR_LEFT && next_dir == DIR_RIGHT) ||
					    (dir == DIR_RIGHT && next_dir == DIR_LEFT))
					{
						if (move_cnt > 0)
						{
							if (dir == DIR_UP || dir == DIR_DOWN)
			    				move_cnt = BLOCK_Y_DIM - move_cnt;
							else
			    				move_cnt = BLOCK_X_DIM - move_cnt;
	    					}
						dir = next_dir;
					}
				}
				// New Maze Square!
				if (move_cnt == 0)
				{
	  				// The player has reached a new maze square; unveil nearby maze
	    				// squares and check whether the player has won the level.

	  				if (unveil_around_player (play_x, play_y))
					{
						pthread_mutex_unlock(&mtx);
	    					goto_next_level = 1;
						break;
					}

					draw_mask_block(play_x, play_y, get_player_block(last_dir), get_player_mask(last_dir)); // get the whole background
					show_screen();
					draw_full_block(play_x, play_y, old_values);

					// Record directions open to motion.
					find_open_directions (play_x / BLOCK_X_DIM,
								play_y / BLOCK_Y_DIM,
							 	open);

					// Change dir to next_dir if next_dir is open
 					if (open[next_dir])
					{
						dir = next_dir;
		   			}

					// The direction may not be open to motion...
			    		//   1) ran into a wall
			     		//   2) initial direction and its opposite both face walls
	     				if (dir != DIR_STOP)
					{
	      					if (!open[dir])
						{
			  				dir = DIR_STOP;
						}
	       					else if (dir == DIR_UP || dir == DIR_DOWN)
						{
		    					move_cnt = BLOCK_Y_DIM;
						}
						else
						{
	    						move_cnt = BLOCK_X_DIM;
						}
    					}
				}
				// Unlock the mutex
				pthread_mutex_unlock(&mtx);

				if (dir != DIR_STOP)
				{
	    				// move in chosen direction
	    				last_dir = dir;
	    				move_cnt--;
	    				switch (dir)
					{
					case DIR_UP:
						move_up (&play_y);
						break;
					case DIR_RIGHT:
						move_right (&play_x);
						break;
					case DIR_DOWN:
						move_down (&play_y);
						break;
					case DIR_LEFT:
						move_left (&play_x);
						break;
		   			}
		   			draw_mask_block(play_x, play_y, get_player_block(last_dir), get_player_mask(last_dir)); // get the whole background
					need_redraw = 1;
				}
			}
			if(need_redraw)
			{
				show_screen();
				draw_full_block(play_x, play_y, old_values);
			}
			need_redraw = 0;
		}
		total = 0; // refresh the time
	}
	if (quit_flag == 0) winner = 1;
	return 0;
}

/*
 * main
 *   DESCRIPTION: Initializes and runs the two threads
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: 0 on success, -1 on failure
 *   SIDE EFFECTS: none
 */
int main()
{
	int ret;
   	struct termios tio_new;
	unsigned long update_rate = 32; /* in Hz */

	pthread_t tid1;
	pthread_t tid2;
    pthread_t tid3;

	// Initialize RTC
	fd = open("/dev/rtc", O_RDONLY, 0);

    // Initialize tux
    fd_tux = open("/dev/ttyS0", O_RDWR | O_NOCTTY);
    int ldisc_num = N_MOUSE;
    ret = ioctl(fd_tux, TIOCSETD, &ldisc_num);
    ret = ioctl(fd_tux, TUX_INIT);

	// Enable RTC periodic interrupts at update_rate Hz
	// Default max is 64...must change in /proc/sys/dev/rtc/max-user-freq
	ret = ioctl(fd, RTC_IRQP_SET, update_rate);
	ret = ioctl(fd, RTC_PIE_ON, 0);

	// Initialize Keyboard
	// Turn on non-blocking mode
    	if (fcntl (fileno (stdin), F_SETFL, O_NONBLOCK) != 0)
	    {
        	perror ("fcntl to make stdin non-blocking");
		return -1;
    	}

	// Save current terminal attributes for stdin.
    	if (tcgetattr (fileno (stdin), &tio_orig) != 0)
	{
		perror ("tcgetattr to read stdin terminal settings");
		return -1;
	}

	// Turn off canonical (line-buffered) mode and echoing of keystrokes
 	// Set minimal character and timing parameters so as
        tio_new = tio_orig;
    	tio_new.c_lflag &= ~(ICANON | ECHO);
    	tio_new.c_cc[VMIN] = 1;
    	tio_new.c_cc[VTIME] = 0;
    	if (tcsetattr (fileno (stdin), TCSANOW, &tio_new) != 0)
	{
		perror ("tcsetattr to set stdin terminal settings");
		return -1;
	}


	// Perform Sanity Checks and then initialize input and display
	if ((sanity_check () != 0) || (set_mode_X (fill_horiz_buffer, fill_vert_buffer) != 0))
	{
		return 3;
	}

	// Create the threads
	pthread_create(&tid1, NULL, rtc_thread, NULL);
	pthread_create(&tid2, NULL, keyboard_thread, NULL);
    pthread_create(&tid3, NULL, tux_thread, NULL);

	// Wait for all the threads to end
	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);

	// Shutdown Display
	clear_mode_X();

	// Close Keyboard
	(void)tcsetattr (fileno (stdin), TCSANOW, &tio_orig);

	// Close RTC
	close(fd);
    close(fd_tux);

	// Print outcome of the game
	if (winner == 1)
	{
		printf ("You win the game!  CONGRATULATIONS!\n");
	}
	else if (quit_flag == 1)
	{
		printf ("Quitter!\n");
	}
	else
	{
		printf ("Sorry, you lose...\n");
	}

	// Return success
	return 0;
}
