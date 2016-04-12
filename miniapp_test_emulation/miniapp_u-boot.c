#include <common.h>
#include <command.h>
#include <asm/arch/mux.h>
#include <asm/io.h>
#include <asm/errno.h>
#include "board/ti/reptar/reptar.h"

extern int sevenseg_putc(int index, unsigned char number);

char val_7seg[3];
unsigned int b_val;
unsigned int b_val_last;

void incr_7seg(int idx);

int main(int argc, char *argv[])
{	
	int i;
	volatile int timeout;
	printf("Start of the Miniapp U-boot Standalone Application\n");

	/* initialize 7segs to 0*/
	for(i = 0; i < 3; i++)
	{
		if(sevenseg_putc(i, 0))
		{
			return -1;
		}
		val_7seg[i] = 0;
	}

	while(1)
	{
		b_val_last = readw(SP6_PUSH_BUT);
		for(timeout = 0xFFFF; timeout > 0; timeout--);
		b_val = readw(SP6_PUSH_BUT);
		if(b_val != b_val_last)
		{
			/* check SW2 */
			if(!(b_val & 0x02) && (b_val_last & 0x02))
			{
				incr_7seg(0);
			}
			/* check SW5 */
			if(!(b_val & 0x10) && (b_val_last & 0x10))
			{
				incr_7seg(1);
			}
			/* check SW4 */
			if(!(b_val & 0x08) && (b_val_last & 0x08))
			{
				incr_7seg(2);
			}
			/* check SW3 */
			if(!(b_val & 0x04) && (b_val_last & 0x04))
			{
				break;
			}
		}
	}

	printf("Stop of the Miniapp U-boot Standalone Application\n");

	return 0;
}

void incr_7seg(int idx)
{
	if(idx > 2 || idx < 0)
	{
		return;
	}
	val_7seg[idx] = (val_7seg[idx] + 1) % 10;
	sevenseg_putc(idx, val_7seg[idx]);
}


