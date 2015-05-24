/*

  VLSI Solution generic microcontroller example player / recorder definitions.
  v1.00.

  See VS10xx AppNote: Playback and Recording for details.

  v1.00 2012-11-23 HH  First release

*/
#ifndef PLAYER_RECORDER_H
#define PLAYER_RECORDER_H

#include "vs10xx_uc.h"

int VSTestInitHardware(void);
int VSTestInitSoftware(void);

void VS1053Play(u_int8 *buf, u_int32 len);

#endif
