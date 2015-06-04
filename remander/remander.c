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

#define REQ_URL "http://wx-1196398119.ap-southeast-1.elb.amazonaws.com/static/t/tmp.mp3"

static void delay_m(int m)
{
	osi_Sleep(m);
}

int remander_launcher(void)
{
	int len, i, ret;
	char *buf;

	len = http_open(REQ_URL);

	while(len > 0)
	{
		i = http_read(&buf);

		audio_player(buf, i);

		len -= i;
	}
	audio_play_end();

	return 0;
}
