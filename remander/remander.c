/*
 * remander.c
 *
 *  Created on: 2014Äê7ÔÂ15ÈÕ
 *      Author: Administrator
 */
#include "../cc3200_common.h"
#include "../misc/audio_spi/vs1053b.h"

#include <stdio.h>
#include <string.h>

#include "remander.h"
#include "http_if.h"

#define REQ_URL

void remander_launcher(void)
{
	int len, i, ret;
	char *buf;


	while(1)
	{
		len = http_open(REQ_URL);
		if (ret > 0)
		{
			while(len)
			{
				i = http_read(buf)
				audio_player(buf, i);
				len -= i;
			}
		}
		delay_m(2000);

	}


}
