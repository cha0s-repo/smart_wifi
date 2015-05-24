/*

  VLSI Solution generic microcontroller example player / recorder for
  VS1053.

  v1.03 2012-12-11 HH  Recording command 'p' was VS1063 only -> removed
                       Added chip type recognition
  v1.02 2012-12-04 HH  Command '_' incorrectly printed VS1063-specific fields
  v1.01 2012-11-28 HH  Untabified
  v1.00 2012-11-27 HH  First release

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "player.h"
/* Download the latest VS1053a Patches package and its
   vs1053b-patches-flac.plg. If you want to use the smaller patch set
   which doesn't contain the FLAC decoder, use vs1053b-patches.plg instead.
   The patches package is available at
   http://www.vlsi.fi/en/support/software/vs10xxpatches.html */
//#include "vs1053b-patches-flac.plg"

#include "../../cc3200_common.h"
#include "vs_spi.h"

#define FILE_BUFFER_SIZE 512
#define SDI_MAX_TRANSFER_SIZE 32
#define SDI_END_FILL_BYTES_FLAC 12288
#define SDI_END_FILL_BYTES       2050
#define REC_BUFFER_SIZE 512


#define min(a,b) (((a)<(b))?(a):(b))



enum AudioFormat {
  afUnknown,
  afRiff,
  afOggVorbis,
  afMp1,
  afMp2,
  afMp3,
  afAacMp4,
  afAacAdts,
  afAacAdif,
  afFlac,
  afWma,
  afMidi,
} audioFormat = afUnknown;

const char *afName[] = {
  "unknown",
  "RIFF",
  "Ogg",
  "MP1",
  "MP2",
  "MP3",
  "AAC MP4",
  "AAC ADTS",
  "AAC ADIF",
  "FLAC",
  "WMA",
  "MIDI",
};

typedef union
{
  char  b8[2];
  short b16;
} vs_data_t;

void WriteSci(u_int8 addr, u_int16 cmd)
{
	vs_data_t ret;

	ret.b16 = cmd;

	vs_write_cmd(0x02);

	vs_write_cmd(addr);

	vs_write_cmd(ret.b8[1]);

	vs_write_cmd(ret.b8[0]);

}

int WriteSdi(const u_int8 *data, u_int8 bytes)
{
	u_int8 i;

	if (bytes > 32)
		return -1;

	for (i = 0; i < bytes; i++)
	{
		vs_write_d(*data++);

	}

	return 0;
}
u_int16 ReadSci(u_int8 addr)
{
	vs_data_t ret;

	vs_write_cmd(0x03);

	vs_write_cmd(addr);

	ret.b8[1] = vs_write_cmd(0xff);

	ret.b8[0] = vs_write_cmd(0xff);

	return ret.b16;
}
/*
  Read 32-bit increasing counter value from addr.
  Because the 32-bit value can change while reading it,
  read MSB's twice and decide which is the correct one.
*/
u_int32 ReadVS10xxMem32Counter(u_int16 addr) {
  u_int16 msbV1, lsb, msbV2;
  u_int32 res;

  WriteSci(SCI_WRAMADDR, addr+1);
  msbV1 = ReadSci(SCI_WRAM);
  WriteSci(SCI_WRAMADDR, addr);
  lsb = ReadSci(SCI_WRAM);
  msbV2 = ReadSci(SCI_WRAM);
  if (lsb < 0x8000U) {
    msbV1 = msbV2;
  }
  res = ((u_int32)msbV1 << 16) | lsb;
  
  return res;
}


/*
  Read 32-bit non-changing value from addr.
*/
u_int32 ReadVS10xxMem32(u_int16 addr) {
  u_int16 lsb;
  WriteSci(SCI_WRAMADDR, addr);
  lsb = ReadSci(SCI_WRAM);
  return lsb | ((u_int32)ReadSci(SCI_WRAM) << 16);
}


/*
  Read 16-bit value from addr.
*/
u_int16 ReadVS10xxMem(u_int16 addr) {
  WriteSci(SCI_WRAMADDR, addr);
  return ReadSci(SCI_WRAM);
}


/*
  Write 16-bit value to given VS10xx address
*/
void WriteVS10xxMem(u_int16 addr, u_int16 data) {
  WriteSci(SCI_WRAMADDR, addr);
  WriteSci(SCI_WRAM, data);
}

/*
  Write 32-bit value to given VS10xx address
*/
void WriteVS10xxMem32(u_int16 addr, u_int32 data) {
  WriteSci(SCI_WRAMADDR, addr);
  WriteSci(SCI_WRAM, (u_int16)data);
  WriteSci(SCI_WRAM, (u_int16)(data>>16));
}




static const u_int16 linToDBTab[5] = {36781, 41285, 46341, 52016, 58386};


/*

  Loads a plugin.

  This is a slight modification of the LoadUserCode() example
  provided in many of VLSI Solution's program packages.

*/
#if 0  
void LoadPlugin(const u_int16 *d, u_int16 len) {
  int i = 0;

  while (i<len) {
    unsigned short addr, n, val;
    addr = d[i++];
    n = d[i++];
    if (n & 0x8000U) { /* RLE run, replicate n samples */
      n &= 0x7FFF;
      val = d[i++];
      while (n--) {
        WriteSci(addr, val);
      }
    } else {           /* Copy run, copy n samples */
      while (n--) {
        val = d[i++];
        WriteSci(addr, val);
      }
    }
  }
}
#endif //0


void VS1053Play(u_int8 *buf, u_int32 len) {

  u_int8 *playBuf = buf;
  u_int32 bytesInBuffer = len;        // How many bytes in buffer left
  u_int32 pos=0;                
  int endFillByte = 0;          // What byte value to send after file
  int endFillBytes = SDI_END_FILL_BYTES; // How many of those to send

  int i;

  WriteSci(SCI_DECODE_TIME, 0);         // Reset DECODE_TIME


  /* Main playback loop */

    while (bytesInBuffer) {
        int t = min(SDI_MAX_TRANSFER_SIZE, bytesInBuffer);

        // This is the heart of the algorithm: on the following line
        // actual audio data gets sent to VS10xx.
        WriteSdi(playBuf, t);

        playBuf += t;
        bytesInBuffer -= t;
      }

  
  endFillByte = ReadVS10xxMem(PAR_END_FILL_BYTE);
  /* Earlier we collected endFillByte. Now, just in case the file was
     broken, or if a cancel playback command has been given, write
     lots of endFillBytes. */
  memset(playBuf, endFillByte, sizeof(playBuf));
  for (i=0; i<endFillBytes; i+=SDI_MAX_TRANSFER_SIZE) {
    WriteSdi(playBuf, SDI_MAX_TRANSFER_SIZE);
  }

  return 0;
}


/*

  Hardware Initialization for VS1053.
  
*/
int VSTestInitHardware(void) {
  /* Write here your microcontroller code which puts VS10xx in hardware
     reset anc back (set xRESET to 0 for at least a few clock cycles,
     then to 1). */
	vs_spi_open();

	vs_rst(0);
	os_Sleep(1);
	vs_rst(1);


  return 0;
}



/* Note: code SS_VER=2 is used for both VS1002 and VS1011e */
const u_int16 chipNumber[16] = {
  1001, 1011, 1011, 1003, 1053, 1033, 1063, 1103,
  0, 0, 0, 0, 0, 0, 0, 0
};

/*

  Software Initialization for VS1053.

  Note that you need to check whether SM_SDISHARE should be set in
  your application or not.
  
*/
int VSTestInitSoftware(void) {
  u_int16 ssVer;

  /* Start initialization with a dummy read, which makes sure our
     microcontoller chips selects and everything are where they
     are supposed to be and that VS10xx's SCI bus is in a known state. */
  ReadSci(SCI_MODE);

  /* First real operation is a software reset. After the software
     reset we know what the status of the IC is. You need, depending
     on your application, either set or not set SM_SDISHARE. See the
     Datasheet for details. */
  WriteSci(SCI_MODE, SM_SDINEW);

  /* A quick sanity check: write to two registers, then test if we
     get the same results. Note that if you use a too high SPI
     speed, the MSB is the most likely to fail when read again. */
  WriteSci(SCI_HDAT0, 0xABAD);
  WriteSci(SCI_HDAT1, 0x1DEA);
  if (ReadSci(SCI_HDAT0) != 0xABAD || ReadSci(SCI_HDAT1) != 0x1DEA) {
    UART_PRINT("There is something wrong with VS10xx\n");
    return 1;
  }

  /* Check VS10xx type */
  ssVer = ((ReadSci(SCI_STATUS) >> 4) & 15);
  if (chipNumber[ssVer]) {
    UART_PRINT("Chip is VS%d\n", chipNumber[ssVer]);
    if (chipNumber[ssVer] != 1053) {
      UART_PRINT("Incorrect chip\n");
      return 1;
    }
  } else {
    UART_PRINT("Unknown VS10xx SCI_MODE field SS_VER = %d\n", ssVer);
    return 1;
  }

  /* Set the clock. Until this point we need to run SPI slow so that
     we do not exceed the maximum speeds mentioned in
     Chapter SPI Timing Diagram in the Datasheet. */
  WriteSci(SCI_CLOCKF,
           HZ_TO_SC_FREQ(12288000) | SC_MULT_53_35X | SC_ADD_53_10X);


  /* Now when we have upped the VS10xx clock speed, the microcontroller
     SPI bus can run faster. Do that before you start playing or
     recording files. */

  WriteSci(SCI_CLOCKF, 0x9800);
  /* Set up other parameters. */
  WriteVS10xxMem(PAR_CONFIG1, PAR_CONFIG1_AAC_SBR_SELECTIVE_UPSAMPLE);

  /* Set volume level at -6 dB of maximum */
  WriteSci(SCI_VOL, 0x0c0c);

  /* Now it's time to load the proper patch set. */
  //LoadPlugin(plugin, sizeof(plugin)/sizeof(plugin[0]));

  /* We're ready to go. */
  return 0;
}

