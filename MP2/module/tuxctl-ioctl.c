/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>
#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

#define MASK_LEDS 0x000F0000 /* 0000 0000 0000 1111 0000 0000 0000 0000 */
#define MASK_DECIMALS 0x0F000000 /* 0000 1111 0000 0000 0000 0000 0000 0000 */
#define MASK_LED_3 0x0000F000 /* 0000 0000 0000 0000 1111 0000 0000 0000 */
#define MASK_LED_2 0x00000F00 /* 0000 0000 0000 0000 0000 1111 0000 0000 */
#define MASK_LED_1 0x000000F0 /* 0000 0000 0000 0000 0000 0000 1111 0000 */
#define MASK_LED_0 0x0000000F /* 0000 0000 0000 0000 0000 0000 0000 1111 */
#define MASK_4_BITS 0x8		  /* 1000 */
#define MASK_1_BITS 0x1  	  /* 0001 */
#define MASK_LOW_BITS 0x0000FFFF /* 0000 0000 0000 0000 FFFF FFFF FFFF FFFF */
#define LED_FOUR 4
#define BUTTONS_NUM 4

// MACROS for hex to LED
#define ZERO  0xE7
#define ONE   0x06
#define TWO   0xCB
#define THREE 0x8F
#define FOUR  0x2E
#define FIVE  0xAD
#define SIX   0xED
#define SEVEN 0x86
#define EIGHT 0xEF
#define NINE  0xAE
#define NUM_A 0xEE
#define NUM_B 0x6D
#define NUM_C 0xE1
#define NUM_D 0x4F
#define NUM_E 0xE9
#define NUM_F 0xE8
#define DEC_POINT_ON 0x10

// MACROS for buttons
#define CLEAR_MASK 0x0F // clear the bytes
#define D_MASK 0x40     // used to check if D is on or off
#define L_MASK 0x20     // used to check if L is on or off

// functions
int tux_init(struct tty_struct* tty);
int tux_Buttons(struct tty_struct* tty, unsigned long arg);
int tux_led_info(struct tty_struct* tty, unsigned long arg, unsigned char led_info);
int tux_reset_helper(struct tty_struct* tty, unsigned long arg);

// global variables for RESET, and flag for spamming
unsigned long prev_arg; // USED FOR RESET
int spamming_flag = 0;

// spinlock and spinlock flags for global buffer for buttons
spinlock_t my_spin_lock = SPIN_LOCK_UNLOCKED; // USED for BUTTONS
unsigned long flags;
unsigned char buffer_button[2];

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)


/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in
 * tuxctl-ld.c. It calls this function, so all warnings there apply
 * here as well.
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
    unsigned a, b, c;

    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];

	switch(a)
	{
		case MTCP_ACK:
			spamming_flag = 0; // Flag for spamming on buttons
			break;
		case MTCP_RESET:
		{
			tux_init(tty); // init the tux,
			tux_reset_helper(tty, prev_arg); // set leds to prev_arg
			break;
		}
		case MTCP_BIOC_EVENT:
		{
			spin_lock_irqsave(&my_spin_lock, flags); // use locks when writing to this global array
			buffer_button[0] = b;
			buffer_button[1] = c;
			spin_unlock_irqrestore(&my_spin_lock, flags); // unlock it now
			break;
		}
		default:
			return;
	}
	return;
    /*printk("packet : %x %x %x\n", a, b, c); */
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/
int
tuxctl_ioctl (struct tty_struct* tty, struct file* file,
	      unsigned cmd, unsigned long arg)
{
    switch (cmd) {
	case TUX_INIT:
	{
		return tux_init(tty); // return  0
	}
	case TUX_BUTTONS:
	{
		if(arg == 0)
			return -EINVAL; // if arg is 0 then -EINVAL
		else
			 return tux_Buttons(tty, arg);
	}
	case TUX_SET_LED:
	{
		unsigned char led_on_info = (MASK_LEDS & arg) >> 16; // 0000 0000 0000 XXXX 0000 0000 0000 0000
		prev_arg = arg; // save the arg, for REST
		if(led_on_info)
		{
			tux_led_info(tty, arg, led_on_info); // calll to set LED
			return 0;
		}
		return 0;

	}
	case TUX_LED_ACK:
	case TUX_LED_REQUEST:
	case TUX_READ_LED:
	default:
	    return -EINVAL;
    }
}

/*
 * tux_init
 * 	DESCRIPTION : Initialize whats needed for IOCTL
 * INPUT: tty_struct
 * OUTPUT: none
 * RETURN VALUE: success (0) or failure (-1)
 * SIDE EFFECTS: intialize's variables
*/
int tux_init(struct tty_struct* tty)
{
	unsigned char buffer_array[2];

	buffer_array[0] = MTCP_BIOC_ON;
	buffer_array[1] = MTCP_LED_USR;

	tuxctl_ldisc_put(tty, buffer_array, 2);
	return 0;
}

/*
 * tux_Buttons
 * 	DESCRIPTION : Get info about the buttons from device
 * INPUT: tty_struct, arg
 * OUTPUT: none
 * RETURN VALUE: 0
 * SIDE EFFECTS: send data to user about which button was pressed
*/
int tux_Buttons(struct tty_struct* tty, unsigned long arg)
{
	unsigned char CBASt = 0;  // WE'll put in the byte 1 info here
	unsigned char RDLU = 0; // we'll put in the byte 2 info here
	unsigned char ready_buttons = 0; // we'll put the info in this 1 byte
	unsigned char temp_val_D = 0;
	unsigned char temp_val_L = 0;

	spin_lock_irqsave(&my_spin_lock, flags);
	CBASt = buffer_button[0]; // this is the C B A Start byte
	RDLU = buffer_button[1];  // this is the R D L U byte
	spin_unlock_irqrestore(&my_spin_lock, flags);

	// buttons are active low, so 0 is ON, 1 is OFF
	CBASt = ~CBASt; // binary NOT to make it 0 is OFF, 1 is ON
	RDLU = ~RDLU;   // same as above

	CBASt = (CBASt & CLEAR_MASK);
	RDLU = (RDLU & CLEAR_MASK);
	// now we have the actual values properly. 1 is ON, 0 is OFF

	ready_buttons |= CBASt; // XXXX C B A Start

	// 4 bit shift to the left for RDLU
	RDLU <<= 4;

	ready_buttons |= RDLU; // R D L U | C B A Start

	// check if the D bit is on or off, if OFF we dont care
	temp_val_D |= (ready_buttons & D_MASK); // get the D value
	temp_val_L |= (ready_buttons & L_MASK); // get the L value

	if(temp_val_D)
	{
		temp_val_D = temp_val_D >> 1; // shift it right
		ready_buttons |= temp_val_D; // change it
		ready_buttons &= ~D_MASK; // get rid of the copy part
	}

	if(temp_val_L && temp_val_D == 0)
	{
		ready_buttons &= ~L_MASK;
		temp_val_L = temp_val_L << 1; // shift it left once
		ready_buttons |= temp_val_L; // change it
	}

	copy_to_user((void*) arg, (void*) &ready_buttons, sizeof(ready_buttons));
	return 0;
}

/*
 * tux_reset_helper
 * 	DESCRIPTION : Helps with RESET
 * INPUT: tty_struct, argumnet,
 * OUTPUT: LEDS turned on the device
 * RETURN VALUE: success (0)
 * SIDE EFFECTS: Communication with the tux to set the LEDS
*/
int tux_reset_helper(struct tty_struct* tty, unsigned long arg)
{
	unsigned char led_on_info = (MASK_LEDS & arg) >> 16; // 0000 0000 0000 XXXX 0000 0000 0000 0000
	if(led_on_info)
	{
		prev_arg = arg;
		tux_led_info(tty, arg, led_on_info);
		return 0;
	}
	return 0;
}

/*
 * tux_led_info
 * 	DESCRIPTION : Set the LEDS on
 * INPUT: tty_struct, argumnet, led_info(leds that are on)
 * OUTPUT: LEDS turned on the device
 * RETURN VALUE: success (0) or failure (-1)
 * SIDE EFFECTS: Communication with the tux to set the LEDS
*/
int tux_led_info(struct tty_struct* tty, unsigned long arg, unsigned char led_info)
{
	if(spamming_flag == 0) // helps with RESET, and making sure we dont SPAM!!!!!
	{
		spamming_flag = 1;
		int bit_mask = MASK_1_BITS; // 0x1
		int i = 0;
		unsigned char led_buffer[6];
		int ON_INDEX = 1; // used for putting into it buffer
		int count = 2; // we put in the OPCODE, and LEDS that are on, so THATS's 2
		unsigned long dec_on_info = (arg >> 24); // 0000 XXXX 0000 ... 0000 , shift the XXXX values to the lowest 4 bits
		unsigned long dec_on_mask = MASK_1_BITS; // mask is 0x1 = 0001

		while(i < 6)
		{
			led_buffer[i] = 0;
			i++;
		}
		i = 0; // make it zero again for next while loop
		led_buffer[0] = MTCP_LED_SET; // send the OPCODE first

		// Check for ON or OFF... CREATED 1 BYTE PACKAGE for LED INFO
		while(i < LED_FOUR)
		{
			if(led_info & MASK_1_BITS) // ex:- led_info 0101 and bit_mask 0001, we get 0x0001
			{
				// led will be set in 3 2 1 0 order. same with decimal point
				led_buffer[ON_INDEX] |= bit_mask; // update bit_mask and led_info
				if(dec_on_info & dec_on_mask)
					led_buffer[count] |= DEC_POINT_ON;
				unsigned long temp = arg >> (4 * i); // bit shift to get led_info
				unsigned long temp_mask = 0xF; // get the values of led info
				switch(temp & temp_mask) // what is being displayed
				{
					case 0x0:
					{
						led_buffer[count] |= ZERO;
						break;
					}
					case 0x1:
					{
						led_buffer[count] |= ONE;
						break;
					}
					case 0x2:
					{
						led_buffer[count] |= TWO;
						break;
					}
					case 0x3:
					{
						led_buffer[count] |= THREE;
						break;
					}
					case 0x4:
					{
						led_buffer[count] |= FOUR;
						break;
					}
					case 0x5:
					{
						led_buffer[count] |= FIVE;
						break;
					}
					case 0x6:
					{
						led_buffer[count] |= SIX;
						break;
					}
					case 0x7:
					{
						led_buffer[count] |= SEVEN;
						break;
					}
					case 0x8:
					{
						led_buffer[count] |= EIGHT;
						break;
					}
					case 0x9:
					{
						led_buffer[count] |= NINE;
						break;
					}
					case 0xA:
					{
						led_buffer[count] |= NUM_A;
						break;
					}
					case 0xB:
					{
						led_buffer[count] |= NUM_B;
						break;
					}
					case 0xC:
					{
						led_buffer[count] |= NUM_C;
						break;
					}
					case 0xD:
					{
						led_buffer[count] |= NUM_D;
						break;
					}
					case 0xE:
					{
						led_buffer[count] |= NUM_E;
						break;
					}
					case 0xF:
					{
						led_buffer[count] |= NUM_F;
						break;
					}
				}
				count++; // incriment the led_info
			}
			led_info >>= 1; // shift the led info to the right
			bit_mask <<= 1; // shift left to make it 0010 from 0001, and so on
			dec_on_info >>= 1; // shift decimal point value to the right
			i++;
		}
		return tuxctl_ldisc_put(tty, led_buffer, count);
	}
	else
		return 0;

}
