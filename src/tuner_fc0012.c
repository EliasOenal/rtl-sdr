/*
 * fc0012 tuner support for rtl-sdr
 *
 * Based on tuner_fc0012.c found as part of the (seemingly GPLed)
 * rtl2832u Linux DVB driver.
 *
 * Rewritten and hacked into rtl-sdr by David Basden <davidb-sdr@rcpt.to>
 */

#include <stdio.h>
#include <stdint.h>

#include "rtlsdr_i2c.h"
#include "tuner_fc0012.h"

#define CRYSTAL_FREQ		28800000

#define FC0012_LNAGAIN	FC0012_LNA_GAIN_HI

/* Incomplete list of register settings:
 *
 * Name			Reg	Bits	Desc
 * CHIP_ID		0x00	0-7	Chip ID (constant 0xA1)
 * RF_A			0x01	0-3	Number of count-to-9 cycles in RF
 *					divider (suggested: 2..9)
 * RF_M			0x02	0-7	Total number of cycles (to-8 and to-9)
 *					in RF divider
 * RF_K_HIGH		0x03	0-6	Bits 8..14 of fractional divider
 * RF_K_LOW		0x04	0-7	Bits 0..7 of fractional RF divider
 * RF_OUTDIV_A		0x05	3-7	Power of two required?
 * LNA_POWER_DOWN	0x06	0	Set to 1 to switch off low noise amp
 * RF_OUTDIV_B		0x06	1	Set to select 3 instead of 2 for the
 *                                      RF output divider
 * VCO_SPEED		0x06	3	Select tuning range of VCO:
 *					 0 = Low range, (ca. 1.1 - 1.5GHz)
 *					 1 = High range (ca. 1.4 - 1.8GHz)
 * BANDWIDTH		0x06	6-7	Set bandwidth. 6MHz = 0x80, 7MHz=0x40
 *					8MHz=0x00
 * XTAL_SPEED		0x07	5	Set to 1 for 28.8MHz Crystal input
 *					or 0 for 36MHz
 * <agc params>		0x08	0-7
 * EN_CAL_RSSI		0x09	4 	Enable calibrate RSSI
 *					(Receive Signal Strength Indicator)
 * LNA_FORCE		0x0d	0
 * AGC_FORCE		0x0d	?
 * LNA_GAIN		0x13	3-4	Low noise amp gain
 * LNA_COMPS		0x15	3	?
 * VCO_CALIB		0x0e	7	Set high then low to calibrate VCO
 *					 (fast lock?)
 * VCO_VOLTAGE		0x0e	0-6	Read Control voltage of VCO
 *					 (big value -> low freq)
 */

#define REG_SEC_GAIN	0x12	// Probably some secondary gain control
#define REG_LNA_GAIN	0x13	// Low noise amplifier gain
#define REG_AGC_MODE	0x0C	// Contriols AGC mode and something else
#define AGC_MODE_ON	0xFC
#define AGC_MODE_OFF	0xEC

/* glue functions to rtl-sdr code */
int FC0012_Write(void *pTuner, unsigned char RegAddr, unsigned char Byte)
{
	uint8_t data[2];

	data[0] = RegAddr;
	data[1] = Byte;

	if (rtlsdr_i2c_write_fn(pTuner, FC0012_I2C_ADDR, data, 2) < 0)
		return FC0012_ERROR;

	return FC0012_OK;
}

int FC0012_Read(void *pTuner, unsigned char RegAddr, unsigned char *pByte)
{
	uint8_t data = RegAddr;

	if (rtlsdr_i2c_write_fn(pTuner, FC0012_I2C_ADDR, &data, 1) < 0)
		return FC0012_ERROR;

	if (rtlsdr_i2c_read_fn(pTuner, FC0012_I2C_ADDR, &data, 1) < 0)
		return FC0012_ERROR;

	*pByte = data;

	return FC0012_OK;
}

#ifdef DEBUG
#define DEBUGF printf
#else
#define DEBUGF(...)	()
#endif
#if 0
void FC0012_Dump_Registers()
{
#ifdef DEBUG
	unsigned char regBuf;
	int ret;
	int i;

	DEBUGF("\nFC0012 registers:\n");
	for (i=0; i<=0x15; ++i)
	{
		ret = FC0012_Read(pTuner, i, &regBuf);
		if (ret) DEBUGF("\nCouldn't read register %02x\n", i);
		DEBUGF("R%x=%02x ",i,regBuf);
	}
	DEBUGF("\n");
	FC0012_Read(pTuner, 0x06, &regBuf);
	DEBUGF("LNA_POWER_DOWN:\t%s\n", regBuf & 1 ? "Powered down" : "Not Powered Down");
	DEBUGF("VCO_SPEED:\t%s\n", regBuf & 0x8 ? "High speed" : "Slow speed");
	DEBUGF("Bandwidth:\t%s\n", (regBuf & 0xC) ? "8MHz" : "less than 8MHz");
	FC0012_Read(pTuner, 0x07, &regBuf);
	DEBUGF("Crystal Speed:\t%s\n", (regBuf & 0x20) ? "28.8MHz" : "36MHZ<!>");
	FC0012_Read(pTuner, 0x09, &regBuf);
	DEBUGF("RSSI calibration mode:\t%s\n", (regBuf & 0x10) ? "RSSI CALIBRATION IN PROGRESS<!>" : "Disabled");
	FC0012_Read(pTuner, 0x0d, &regBuf);
	DEBUGF("LNA Force:\t%s\n", (regBuf & 0x1) ? "Forced" : "Not Forced");
	FC0012_Read(pTuner, LNA_GAIN, &regBuf);
	DEBUGF("LNA Gain:\t");
	switch (regBuf & 0x18) {
		case (0x00): DEBUGF("Low\n"); break;
		case (0x08): DEBUGF("Middle\n"); break;
		case (0x10): DEBUGF("High\n"); break;
		default: DEBUGF("unknown gain value 0x18\n");
	}
#endif
}
#endif

int FC0012_Open(void *pTuner)
{
//	DEBUGF("FC0012_Open start");
	if (FC0012_Write(pTuner, 0x01, 0x05)) return -1;
	if (FC0012_Write(pTuner, 0x02, 0x10)) return -1;
	if (FC0012_Write(pTuner, 0x03, 0x00)) return -1;
	if (FC0012_Write(pTuner, 0x04, 0x00)) return -1;
	if (FC0012_Write(pTuner, 0x05, 0x0F)) return -1;
	if (FC0012_Write(pTuner, 0x06, 0x00)) return -1; // divider 2, VCO slow
	if (FC0012_Write(pTuner, 0x07, 0x20)) return -1; // change to 0x00 for a 36MHz crystal
	if (FC0012_Write(pTuner, 0x08, 0xFF)) return -1; // AGC Clock divide by 254, AGC gain 1/256, Loop Bw 1/8
	if (FC0012_Write(pTuner, 0x09, 0x6E)) return -1; // Disable LoopThrough
	if (FC0012_Write(pTuner, 0x0A, 0xB8)) return -1; // Disable LO Test Buffer
	if (FC0012_Write(pTuner, 0x0B, 0x82)) return -1; // Output Clock is same as clock frequency
	//if (FC0012_Write(pTuner, REG_AGC_MODE, 0xF8)) return -1;
	if (FC0012_Write(pTuner, REG_AGC_MODE, AGC_MODE_ON)) return -1;
	if (FC0012_Write(pTuner, 0x0D, 0x02)) return -1;      // AGC Not Forcing & LNA Forcing
	if (FC0012_Write(pTuner, 0x0E, 0x00)) return -1;
	if (FC0012_Write(pTuner, 0x0F, 0x00)) return -1;
	if (FC0012_Write(pTuner, 0x10, 0x00)) return -1;
	if (FC0012_Write(pTuner, 0x11, 0x00)) return -1;
	if (FC0012_Write(pTuner, REG_SEC_GAIN, 0x1F)) return -1; // 0x00-0x1F
	if (FC0012_Write(pTuner, REG_LNA_GAIN, FC0012_LNAGAIN)) return -1;
	if (FC0012_Write(pTuner, 0x14, 0x00)) return -1;
	if (FC0012_Write(pTuner, 0x15, 0x04)) return -1;	   // Enable LNA COMPS
	
	/* Black magic from nim_rtl2832_fc0012.c in DVB driver. 
	   Even though we've set 0x11 to 0x00 above, this needs to happen to have
	   it go back
	   */
	if (FC0012_Write(pTuner, 0x0d, 0x02)) return -1;
	if (FC0012_Write(pTuner, 0x11, 0x00)) return -1;
	if (FC0012_Write(pTuner, 0x15, 0x04)) return -1;

	return FC0012_OK;
}

# if 0
// Frequency is in kHz. Bandwidth is in MHz
// This is pseudocode to set GPIO6 for VHF/UHF filter switching.
// Trying to do this in reality leads to fail currently. I'm probably doing it wrong.
void FC0012_Frequency_Control(unsigned int Frequency, unsigned short Bandwidth)
{
	if( Frequency < 260000 && Frequency > 150000 )
	{
		// set GPIO6 = low

		//	1. Set tuner frequency
		//	2. if the program quality is not good enough, switch to frequency + 500kHz
		//	3. if the program quality is still no good, switch to frequency - 500kHz
	}
	else
	{
		// set GPIO6 = high

		// set tuner frequency
	}
}
#endif

int FC0012_SetFrequency(void *pTuner, unsigned long Frequency, unsigned short Bandwidth)
{
	int VCO_band = 0;
	unsigned long doubleVCO;
	unsigned short xin, xdiv;
	unsigned char reg[21], am, pm, multi;
	unsigned char read_byte;

	unsigned long CrystalFreqKhz;

//	DEBUGF("FC0012_SetFrequency start");

	CrystalFreqKhz = (rtlsdr_get_tuner_clock(pTuner) + 500) / 1000;

	//===================================== Select frequency divider and the frequency of VCO
	if (Frequency * 96 < 3560000)
	{
		multi = 96; reg[5] = 0x82; reg[6] = 0x00;
	}
	else if (Frequency * 64 < 3560000)
	{
		multi = 64; reg[5] = 0x82; reg[6] = 0x02;
	}
	else if (Frequency * 48 < 3560000)
	{
		multi = 48; reg[5] = 0x42; reg[6] = 0x00;
	}
	else if (Frequency * 32 < 3560000)
	{
		multi = 32; reg[5] = 0x42; reg[6] = 0x02;
	}
	else if (Frequency * 24 < 3560000)
	{
		multi = 24; reg[5] = 0x22; reg[6] = 0x00;
	}
	else if (Frequency * 16 < 3560000)
	{
		multi = 16; reg[5] = 0x22; reg[6] = 0x02;
	}
	else if (Frequency * 12 < 3560000)
	{
		multi = 12; reg[5] = 0x12; reg[6] = 0x00;
	}
	else if (Frequency * 8 < 3560000)
	{
		multi = 8; reg[5] = 0x12; reg[6] = 0x02;
	}
	else if (Frequency * 6 < 3560000)
	{
		multi = 6; reg[5] = 0x0A; reg[6] = 0x00;
	}
	else
	{
		multi = 4; reg[5] = 0x0A; reg[6] = 0x02;
	}

	doubleVCO = Frequency * multi;

	reg[6] = reg[6] | 0x08;
	VCO_band = 1;
	xdiv = (unsigned short)(doubleVCO / (CrystalFreqKhz / 2));
	if( (doubleVCO - xdiv * (CrystalFreqKhz / 2)) >= (CrystalFreqKhz / 4) )
		xdiv = xdiv + 1;

	pm = (unsigned char)( xdiv / 8 );
	am = (unsigned char)( xdiv - (8 * pm));

	if (am < 2) {
		reg[1] = am + 8;
		reg[2] = pm - 1;
	} else {
		reg[1] = am;
		reg[2] = pm;
	}

	// From VCO frequency determines the XIN ( fractional part of Delta Sigma PLL) and divided value (XDIV).
	xin = (unsigned short)(doubleVCO - ((unsigned short)(doubleVCO / (CrystalFreqKhz / 2))) * (CrystalFreqKhz / 2));
	xin = ((xin << 15)/(unsigned short)(CrystalFreqKhz / 2));
	if( xin >= (unsigned short) 16384 )
		xin = xin + (unsigned short) 32768;

	reg[3] = (unsigned char)(xin >> 8);
	reg[4] = (unsigned char)(xin & 0x00FF);

//	DEBUGF("Frequency: %lu, Fa: %d, Fp: %d, Xin:%d \n", Frequency, am, pm, xin);

	switch(Bandwidth)
	{
		case 6: reg[6] = 0x80 | reg[6]; break;
		case 7: reg[6] = (~0x80 & reg[6]) | 0x40; break;
		case 8: default: reg[6] = ~0xC0 & reg[6]; break;
	}

	if (FC0012_Write(pTuner, 0x01, reg[1])) return -1;
	if (FC0012_Write(pTuner, 0x02, reg[2])) return -1;
	if (FC0012_Write(pTuner, 0x03, reg[3])) return -1;
	if (FC0012_Write(pTuner, 0x04, reg[4])) return -1;
	//reg[5] = reg[5] | 0x07; // This is really not cool. Why is it there?
	if (FC0012_Write(pTuner, 0x05, reg[5])) return -1;
	if (FC0012_Write(pTuner, 0x06, reg[6])) return -1;

	// VCO Calibration
	if (FC0012_Write(pTuner, 0x0E, 0x80)) return -1;
	if (FC0012_Write(pTuner, 0x0E, 0x00)) return -1;

	// Read resulting VCO control voltage
	if (FC0012_Write(pTuner, 0x0E, 0x00)) return -1;
	if (FC0012_Read(pTuner, 0x0E, &read_byte)) return -1;
	reg[14] = 0x3F & read_byte;

	// Adjust VCO range if control voltage is at the limit
	if (VCO_band)
	{
		// high-band VCO hitting low frequency bound
		if (reg[14] > 0x3C)
		{
			// select low-band VCO
			reg[6] = ~0x08 & reg[6];

			if (FC0012_Write(pTuner, 0x06, reg[6])) return -1;
			if (FC0012_Write(pTuner, 0x0E, 0x80)) return -1;
			if (FC0012_Write(pTuner, 0x0E, 0x00)) return -1;
		}
	}
	else
	{
		// low-band VCO hitting high frequency bound
		if (reg[14] < 0x02) {
			// select high-band VCO
			reg[6] = 0x08 | reg[6];

			if (FC0012_Write(pTuner, 0x06, reg[6])) return -1;
			if (FC0012_Write(pTuner, 0x0E, 0x80)) return -1;
			if (FC0012_Write(pTuner, 0x0E, 0x00)) return -1;
		}
	}

//	DEBUGF("FC0012_SetFrequency SUCCESS"); FC0012_Dump_Registers();
	return FC0012_OK;
}

static int perr()
{
	printf("E");
	return -1;
}


int FC0012_ManualGain(void *pTuner, int manual)
{
	printf("FC0012_ManualGain: %i\n", manual);
	if (manual) {
		if (FC0012_Write(pTuner, REG_AGC_MODE, AGC_MODE_OFF)) return perr();

		return 0;
	}

	if (FC0012_Write(pTuner, REG_SEC_GAIN, 0x1F)) return perr();
	if (FC0012_Write(pTuner, REG_LNA_GAIN, FC0012_LNA_GAIN_MAX)) return perr();
	if (FC0012_Write(pTuner, REG_AGC_MODE, AGC_MODE_ON)) return perr();

	return 0;
}

int FC0012_SetGain(void *pTuner, int gain)
{
	printf(" GAIN %i\n", gain);
	int sec_gain = 0;
	int lna_gain;

	if (gain >= 19) {
		lna_gain = FC0012_LNA_GAIN_MAX;
		if (gain >= 20)
			sec_gain = gain - 20;
		if (sec_gain > 0x1F)
			sec_gain = 0x1F;
	}
	else if (gain >= 18)
		lna_gain = FC0012_LNA_GAIN_HI;
	else if (gain >= 7)
		lna_gain = FC0012_LNA_GAIN_MID;
	else if (gain >= -4)
		lna_gain = FC0012_LNA_GAIN_LOW;
	else
		lna_gain = FC0012_LNA_GAIN_LOWEST;

//	if (FC0012_Write(pTuner, REG_AGC_MODE, AGC_MODE_OFF)) return perr();
	if (FC0012_Write(pTuner, REG_SEC_GAIN, sec_gain)) return -1;
	if (FC0012_Write(pTuner, REG_LNA_GAIN, lna_gain)) return -1;

	return 0;
}
