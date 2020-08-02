/*
   libS6145ImageReProcess -- Re-implemented Image Processing library for
                             the Sinfonia CHC-S6145 printer family

   Copyright (c) 2015-2020 Solomon Peachy <pizza@shaftnet.org>

   ** ** ** ** Do NOT contact Sinfonia about this library! ** ** ** **

   This is intended to be a drop-in replacement for Sinfonia's proprietary
   libS6145ImageProcess library, which is necessary in order to utilize
   their CHC-S6145 printer family.

   Sinfonia Inc was not involved in the creation of this library, and
   is not responsible in any way for the library or any deficiencies in
   its output.  They will provide no support if it is used.

   If you have the appropriate permission fron Sinfonia, we recommend
   you use their official libS6145ImageProcess library instead, as it
   will generate the highest quality output. However, it is only
   available for x86/x86_64 targets on Linux. Please contact your local
   Sinfonia distributor to obtain the official library.

   ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.

   SPDX-License-Identifier: GPL-3.0+

*/

//#define S6145_UNUSED

#define LIB_VERSION "0.4.1"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//-------------------------------------------------------------------------
// Structures

struct tankParam {
	int32_t trdTankSize;
	int32_t sndTankSize;
	int32_t fstTankSize;
	int32_t trdTankIniEnergy;
	int32_t sndTankIniEnergy;
	int32_t fstTankIniEnergy;
	int32_t trdTrdConductivity;
	int32_t sndSndConductivity;
	int32_t fstFstConductivity;
	int32_t outTrdConductivity;
	int32_t trdSndConductivity;
	int32_t sndFstConductivity;
	int32_t fstOutConductivity;
	int32_t plusMaxEnergy;
	int32_t minusMaxEnergy;
	int32_t plusMaxEnergyPreRead;
	int32_t minusMaxEnergyPreRead;
	int32_t preReadLevelDiff;
	int32_t rsvd[14]; // null or unused?
} __attribute__((packed));

struct imageCorrParam {
	uint16_t pulseTransTable_Y[256];   // @0
	uint16_t pulseTransTable_M[256];   // @512
	uint16_t pulseTransTable_C[256];   // @1024
	uint16_t pulseTransTable_O[256];   // @1536

	uint16_t lineHistCoefTable_Y[256]; // @2048
	uint16_t lineHistCoefTable_M[256]; // @2560
	uint16_t lineHistCoefTable_C[256]; // @3072
	uint16_t lineHistCoefTable_O[256]; // @3584

	uint16_t lineCorrectEnvA_Y;        // @4096
	uint16_t lineCorrectEnvA_M;        // @4098
	uint16_t lineCorrectEnvA_C;        // @4100
	uint16_t lineCorrectEnvA_O;        // @4102

	uint16_t lineCorrectEnvB_Y;        // @4104
	uint16_t lineCorrectEnvB_M;        // @4106
	uint16_t lineCorrectEnvB_C;        // @4108
	uint16_t lineCorrectEnvB_O;        // @4110

	uint16_t lineCorrectEnvC_Y;        // @4112
	uint16_t lineCorrectEnvC_M;        // @4114
	uint16_t lineCorrectEnvC_C;        // @4116
	uint16_t lineCorrectEnvC_O;        // @4118

	uint32_t lineCorrectSlice_Y;       // @4120
	uint32_t lineCorrectSlice_M;       // @4124
	uint32_t lineCorrectSlice_C;       // @4128
	uint32_t lineCorrectSlice_O;       // @4132

	uint32_t lineCorrectSlice1Line_Y;  // @4136
	uint32_t lineCorrectSlice1Line_M;  // @4140
	uint32_t lineCorrectSlice1Line_C;  // @4144
	uint32_t lineCorrectSlice1Line_O;  // @4148

	int32_t lineCorrectPulseMax_Y;    // @4152
	int32_t lineCorrectPulseMax_M;    // @4156
	int32_t lineCorrectPulseMax_C;    // @4160
	int32_t lineCorrectPulseMax_O;    // @4164

	struct tankParam tableTankParam_Y; // @4168
	struct tankParam tableTankParam_M; // @4296
	struct tankParam tableTankParam_C; // @4424
	struct tankParam tableTankParam_O; // @4552

	uint16_t tankPlusMaxEnergyTable_Y[256]; // @4680
	uint16_t tankPlusMaxEnergyTable_M[256]; // @5192
	uint16_t tankPlusMaxEnergyTable_C[256]; // @5704
	uint16_t tankPlusMaxEnergyTable_O[256]; // @6216

	uint16_t tankMinusMaxEnergy_Y[256];     // @6728
	uint16_t tankMinusMaxEnergy_M[256];     // @7240
	uint16_t tankMinusMaxEnergy_C[256];     // @7752
	uint16_t tankMinusMaxEnergy_O[256];     // @8264

	uint16_t printMaxPulse_Y; // @8776
	uint16_t printMaxPulse_M; // @8778
	uint16_t printMaxPulse_C; // @8780
	uint16_t printMaxPulse_O; // @8782

	uint16_t mtfWeightH_Y;    // @8784
	uint16_t mtfWeightH_M;    // @8786
	uint16_t mtfWeightH_C;    // @8788
	uint16_t mtfWeightH_O;    // @8790

	uint16_t mtfWeightV_Y;    // @8792
	uint16_t mtfWeightV_M;    // @8794
	uint16_t mtfWeightV_C;    // @8796
	uint16_t mtfWeightV_O;    // @8798

	uint16_t mtfSlice_Y;      // @8800
	uint16_t mtfSlice_M;      // @8802
	uint16_t mtfSlice_C;      // @8804
	uint16_t mtfSlice_O;      // @8806

	uint16_t val_1;           // @8808 // 1 enables linepreprintprocess
	uint16_t val_2;		  // @8810 // 1 enables ctankprocess
	uint16_t printOpLevel;    // @8812
	uint16_t matteMode;	  // @8814 // 1 for matte

	uint16_t randomBase[4];   // @8816 [use lower byte of each]

	uint16_t matteSize;       // @8824
	uint16_t matteGloss;      // @8826
	uint16_t matteDeglossBlk; // @8828
	uint16_t matteDeglossWht; // @8830

	 int16_t printSideOffset; // @8832
	uint16_t headDots;        // @8834 [always 0x0780, ie 1920. print width

	uint16_t SideEdgeCoefTable[128];   // @8836
	uint8_t  rsvd_2[256];              // @9092, null?
	uint16_t SideEdgeLvCoefTable[256]; // @9348
	uint8_t  rsvd_3[2572];             // @9860, null?

	/* User-supplied data */
	uint16_t width;           // @12432
	uint16_t height;          // @12434
	uint8_t  pad[3948];       // @12436, null.
} __attribute__((packed)); /* 16384 bytes */

//-------------------------------------------------------------------------
// Function declarations

#define ASSERT(__COND, __TXT) if ((!__COND)) { printf(__TXT " @ %d\n", __LINE__); exit(1); }

static void SetTableData(void *src, void *dest, uint16_t words);
static int32_t CheckPrintParam(uint8_t *corrdata);
static uint16_t LinePrintCalcBit(uint16_t val);

static void GetInfo(void);
static void Global_Init(void);
static void SetTableColor(uint8_t plane);
static void LinePrintPreProcess(void);
static void CTankResetParameter(int32_t *params);
static void CTankResetTank(void);
static void PagePrintPreProcess(void);
static void PagePrintProcess(void);
static void CTankProcess(void);
static void SendData(void);
static void PulseTrans(void);
static void CTankUpdateTankVolumeInterDot(uint8_t tank);
static void CTankUpdateTankVolumeInterRay(void);
static void CTankHoseiPreread(void);
static void CTankHosei(void);
static void LineCorrection(void);

static void PulseTransPreReadOP(void);
static void PulseTransPreReadYMC(void);
static void CTankProcessPreRead(void);
static void CTankProcessPreReadDummy(void);
static void RecieveDataOP_GLOSS(void);
static void RecieveDataYMC(void);
static void RecieveDataOP_MATTE(void);

#ifdef S6145_UNUSED
static void SetTable(void);
static void ImageLevelAddition(void);
static void ImageLevelAdditionEx(uint32_t *a1, uint32_t a2, int32_t a3);
static void RecieveDataOP_Post(void);
static void RecieveDataYMC_Post(void);
static void RecieveDataOPLevel_Post(void);
static void RecieveDataOPMatte_Post(void);
static void SideEdgeCorrection(void);
static void LeadEdgeCorrection(void);
#endif

//-------------------------------------------------------------------------
// Endian Manipulation macros
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
#define le16_to_cpu(__x) __x
#define le32_to_cpu(__x) __x
#define be16_to_cpu(__x) __builtin_bswap16(__x)
#define be32_to_cpu(__x) __builtin_bswap32(__x)
#else
#define le16_to_cpu(__x) __builtin_bswap16(__x)
#define le32_to_cpu(__x) __builtin_bswap32(__x)
#define be16_to_cpu(__x) __x
#define be32_to_cpu(__x) __x
#endif

#define cpu_to_le16 le16_to_cpu
#define cpu_to_le32 le32_to_cpu
#define cpu_to_be16 be16_to_cpu
#define cpu_to_be32 be32_to_cpu

//-------------------------------------------------------------------------
// Data declarations

#define BUF_SIZE 2048
#define TANK_SIZE 2052
#define MAX_PULSE 1023
#define MIN_ROWS 100
#define MIN_COLS 100
#define MAX_ROWS 2492
#define MAX_COLS 1844

static void (*g_pfRecieveData)(void);
static void (*g_pfPulseTransPreRead)(void);
static void (*g_pfTankProcessPreRead)(void);

static uint8_t g_pusInLineBuf0[BUF_SIZE];
static uint8_t g_pusInLineBuf1[BUF_SIZE];
static uint8_t g_pusInLineBuf2[BUF_SIZE];
static uint8_t g_pusInLineBuf3[BUF_SIZE];
static uint8_t g_pusInLineBuf4[BUF_SIZE];
static uint8_t g_pusInLineBuf5[BUF_SIZE];
static uint8_t g_pusInLineBuf6[BUF_SIZE];
static uint8_t g_pusInLineBuf7[BUF_SIZE];
static uint8_t g_pusInLineBuf8[BUF_SIZE];
static uint8_t g_pusInLineBuf9[BUF_SIZE];
static uint8_t g_pusInLineBufA[BUF_SIZE];

static uint16_t g_pusOutLineBuf1[BUF_SIZE];
static uint16_t *g_pusOutLineBufTab[2]; // XXX actually [1]

static uint8_t *g_pusPreReadLineBufTab[12]; // XXX actually [11]
static uint8_t *g_pusPulseTransLineBufTab[4];

static uint16_t g_pusPreReadOutLineBuf[BUF_SIZE];

static  int32_t m_piTrdTankArray[TANK_SIZE];
static  int32_t m_piFstTankArray[TANK_SIZE];
static  int32_t m_piSndTankArray[TANK_SIZE];

static  int16_t g_psMtfPreCalcTable[512];
static uint16_t g_pusTankMinusMaxEnegyTable[256];
static uint16_t g_pusTankPlusMaxEnegyTable[256];
static uint16_t g_pusPulseTransTable[256];
static uint16_t g_pusLineHistCoefTable[256];

static  int32_t g_piTankParam[128];   // should be struct tankParam[4]
static  int32_t g_pulRandomTable[32]; // should be u32

static uint8_t  *g_pucInputImageBuf;
static struct imageCorrParam *g_pSPrintParam;
static uint16_t *g_pusOutputImageBuf;

static uint8_t  g_ucRandomBaseLevel[4];
static  int16_t g_sPrintSideOffset;
static uint16_t g_usHeadDots;
static  int32_t g_iLineCorrectPulse;
static uint32_t g_uiMtfSlice;    // really u16?
static uint32_t g_uiMtfWeightV;  // really u16?
static uint32_t g_uiMtfWeightH;  // really u16?
static uint16_t g_usLineCorrect_Env_A;
static uint16_t g_usLineCorrect_Env_B;
static uint16_t g_usLineCorrect_Env_C;

static uint32_t g_uiOutputImageIndex;
static uint32_t g_uiInputImageIndex;

static  int32_t g_iMaxPulseValue;
static uint32_t g_uiMaxPulseBit;

static uint16_t g_usPrintMaxPulse;
static uint16_t g_usPrintOpLevel;
static uint16_t g_usMatteSize;
static uint32_t g_uiLineCorrectSlice;
static uint32_t g_uiLineCorrectSlice1Line;
static uint16_t g_usPrintSizeHeight;
static uint32_t g_uiLineCorrectBase1Line;
static uint32_t g_uiLineCorrectSum;
static uint32_t g_uiLineCorrectBase;
static  int16_t g_sCorrectSw;
static uint16_t g_usMatteMode;
static  int32_t g_iLineCorrectPulseMax;
static uint16_t g_usSheetSizeWidth;
static uint16_t g_usPrintSizeWidth;
static uint16_t g_usPrintColor;
static uint32_t g_uiSendToHeadCounter;
static uint32_t g_uiLineCopyCounter;

static  int32_t m_iTrdTankSize;
static  int32_t m_iTrdSndConductivity;
static  int32_t m_iSndTankSize;
static  int32_t m_iTankKeisuSndFstDivFst;
static  int32_t m_iSndSndConductivity;
static  int32_t m_iTrdTrdConductivity;
static  int32_t m_iTankKeisuTrdSndDivSnd;
static  int32_t m_iTankKeisuTrdSndDivTrd;
static  int32_t m_iSndFstConductivity;
static  int32_t m_iFstTankSize;
static  int32_t m_iTrdTankIniEnergy;
static  int32_t m_iFstTankIniEnergy;
static  int32_t m_iTankKeisuSndFstDivSnd;
static  int32_t m_iSndTankIniEnergy;
static  int32_t m_iPreReadLevelDiff;
static  int32_t m_iMinusMaxEnergyPreRead;
static  int32_t m_iOutTrdConductivity;
static  int32_t m_iFstOutConductivity;
static  int32_t m_iFstFstConductivity;
static  int32_t m_iTankKeisuFstOutDivFst;
static  int32_t m_iTankKeisuOutTrdDivTrd;

#ifdef S6145_UNUSED

static void (*g_pfRecieveData_Post)(void);  /* all users are no-ops */

/* Set but never referenced */
static uint32_t g_uiDataTransCounter;
static uint32_t g_uiTudenLineCounter;

/* Only ever set to 0 */
static uint16_t g_usPrintDummyLevel;
static uint16_t g_usPrintDummyLine;
static uint16_t g_usRearDummyPrintLine;
static uint16_t g_usRearDeleteLine;

/* Appear unused */
static uint16_t g_usCancelCheckLinesForPRec;

static uint16_t g_usPrintSizeLHeight;
static uint16_t g_usPrintSizeLWidth;

static uint16_t g_pusSideEdgeLvCoefTable[256];
static uint16_t g_pusSideEdgeCoefTable[128];
static  int32_t g_iLeadEdgeCorrectPulse;

static  int32_t m_iMinusMaxEnergy;
static  int32_t m_iPlusMaxEnergy;
static  int32_t m_iPlusMaxEnergyPreRead;

static uint16_t g_usCenterHeadToColSen;
static uint16_t g_usThearmEnv;
static uint16_t g_usThearmHead;
static uint16_t g_usMatteGloss;
static uint16_t g_usMatteDeglossBlk;
static uint16_t g_usMatteDeglossWht;
static uint16_t g_usPrintOffsetWidth;
static uint16_t g_usCancelCheckDotsForPRec;
static uint32_t g_uiOffsetCancelCheckPRec;
static uint32_t g_uiLevelAveCounter;
static uint32_t g_uiLevelAveCounter2;
static uint32_t g_uiLevelAveAddtion;
static uint32_t g_uiLevelAveAddtion2;
static uint32_t g_uiDummyPrintCounter;
static uint16_t g_usRearDummyPrintLevel;
static uint16_t g_usLastPrintSizeHeight;
static uint16_t g_usLastPrintSizeWidth;
static uint16_t g_usLastSheetSizeWidth;

uint16_t g_pusOutLineBuf2[BUF_SIZE];
uint16_t *g_pusLamiCompInLineBufTab[4];
#endif

/* **************************** */

int ImageAvrCalc(uint8_t *input, uint16_t cols, uint16_t rows, uint8_t *avg)
{
  uint64_t sum;
  uint32_t offset;
  uint32_t planesize;
  uint32_t j;
  uint8_t plane;

  if ( !input )
	  return 1;
  if ( !avg )
	  return 4;
  if ( cols <= MIN_COLS || cols > MAX_COLS )
	  return 2;
  if ( rows <= MIN_ROWS || rows > MAX_ROWS )
	  return 3;

  planesize = rows * cols;
  offset = 0;

  for ( plane = 0; plane < 3; plane++ )
  {
	  sum = 0;
	  for ( j = 0; j < planesize; ++j )
		  sum += input[offset++];
	  avg[plane] = sum / planesize;
  }

  return 0;
}

int ImageProcessing(unsigned char *in, unsigned short *out, void *corrdata)
{
  uint8_t i;

  fprintf(stderr, "INFO: libS6145ImageReProcess version '%s'\n", LIB_VERSION);
  fprintf(stderr, "INFO: Copyright (c) 2015-2020 Solomon Peachy\n");
  fprintf(stderr, "INFO: This free software comes with ABSOLUTELY NO WARRANTY!\n");
  fprintf(stderr, "INFO: Licensed under the GNU GPLv3.\n");
  fprintf(stderr, "INFO: *** This code is NOT supported or endorsed by Sinfonia! ***\n");

  if (!in)
	  return 1;
  if (!out)
	  return 2;
  if (!corrdata)
	  return 3;

  g_pucInputImageBuf = in;
  g_pusOutputImageBuf = out;
  g_pSPrintParam = (struct imageCorrParam *) corrdata;

  i = CheckPrintParam(corrdata);
  if (i)
    return i;

  Global_Init();
#ifdef S6145_UNUSED
  SetTable();
#endif

  for ( i = 0; i < 4; i++ ) {   /* Full YMCO */
    int32_t lines;
    SetTableColor(i);
    LinePrintPreProcess();
    PagePrintPreProcess();
    lines = g_usPrintSizeHeight;
    while ( lines-- ) {
      PagePrintProcess();
    }
    g_usPrintColor++;
  }

  return 0;
}

/* **************************** */

static void SetTableData(void *src, void *dest, uint16_t words)
{
  uint16_t *in = src;
  uint16_t *out = dest;
  while (words--) {
    out[words] = le16_to_cpu(in[words]);
  }
}

static void GetInfo(void)
{
  uint32_t tmp;

#ifdef S6145_UNUSED
  g_usLastPrintSizeWidth = g_usPrintSizeWidth;
  g_usLastPrintSizeHeight = g_usPrintSizeHeight;
  g_usLastSheetSizeWidth = g_usSheetSizeWidth;
  g_usPrintOffsetWidth = 0;
#endif

  g_usPrintSizeWidth = le16_to_cpu(g_pSPrintParam->width);
  g_usPrintSizeHeight = le16_to_cpu(g_pSPrintParam->height);
  g_usSheetSizeWidth = g_usPrintSizeWidth;

  g_sPrintSideOffset = le16_to_cpu(g_pSPrintParam->printSideOffset);

  if ( g_pSPrintParam->val_1 )
	  g_sCorrectSw |= 1;
  if ( g_pSPrintParam->val_2 )
	  g_sCorrectSw |= 2;

  g_usPrintOpLevel = le16_to_cpu(g_pSPrintParam->printOpLevel);

  tmp = le16_to_cpu(g_pSPrintParam->randomBase[0]);
  g_ucRandomBaseLevel[0] = tmp & 0xff;
  tmp = le16_to_cpu(g_pSPrintParam->randomBase[1]);
  g_ucRandomBaseLevel[1] = tmp & 0xff;
  tmp = le16_to_cpu(g_pSPrintParam->randomBase[2]);
  g_ucRandomBaseLevel[2] = tmp & 0xff;
  tmp = le16_to_cpu(g_pSPrintParam->randomBase[3]);
  g_ucRandomBaseLevel[3] = tmp & 0xff;

  g_usMatteSize = le16_to_cpu(g_pSPrintParam->matteSize);
  g_usMatteMode = le16_to_cpu(g_pSPrintParam->matteMode);

#ifdef S6145_UNUSED
  g_usMatteGloss = le16_to_cpu(g_pSPrintParam->matteGloss);
  g_usMatteDeglossBlk = le16_to_cpu(g_pSPrintParam->matteDeglossBlk);
  g_usMatteDeglossWht = le16_to_cpu(g_pSPrintParam->matteDeglossWht);
#endif

  switch (g_usPrintColor) {
  case 0:
    g_usPrintMaxPulse = le16_to_cpu(g_pSPrintParam->printMaxPulse_Y);
    g_uiMtfWeightH = le16_to_cpu(g_pSPrintParam->mtfWeightH_Y);
    g_uiMtfWeightV = le16_to_cpu(g_pSPrintParam->mtfWeightV_Y);
    g_uiMtfSlice = le16_to_cpu(g_pSPrintParam->mtfSlice_Y);
    g_usLineCorrect_Env_A = le16_to_cpu(g_pSPrintParam->lineCorrectEnvA_Y);
    g_usLineCorrect_Env_B = le16_to_cpu(g_pSPrintParam->lineCorrectEnvB_Y);
    g_usLineCorrect_Env_C = le16_to_cpu(g_pSPrintParam->lineCorrectEnvC_Y);
    g_uiLineCorrectSlice = le32_to_cpu(g_pSPrintParam->lineCorrectSlice_Y);
    g_uiLineCorrectSlice1Line = le32_to_cpu(g_pSPrintParam->lineCorrectSlice1Line_Y);
    g_iLineCorrectPulseMax = le32_to_cpu(g_pSPrintParam->lineCorrectPulseMax_Y);
    break;
  case 1:
    g_usPrintMaxPulse = le16_to_cpu(g_pSPrintParam->printMaxPulse_M);
    g_uiMtfWeightH = le16_to_cpu(g_pSPrintParam->mtfWeightH_M);
    g_uiMtfWeightV = le16_to_cpu(g_pSPrintParam->mtfWeightV_M);
    g_uiMtfSlice = le16_to_cpu(g_pSPrintParam->mtfSlice_M);
    g_usLineCorrect_Env_A = le16_to_cpu(g_pSPrintParam->lineCorrectEnvA_M);
    g_usLineCorrect_Env_B = le16_to_cpu(g_pSPrintParam->lineCorrectEnvB_M);
    g_usLineCorrect_Env_C = le16_to_cpu(g_pSPrintParam->lineCorrectEnvC_M);
    g_uiLineCorrectSlice = le32_to_cpu(g_pSPrintParam->lineCorrectSlice_M);
    g_uiLineCorrectSlice1Line = le32_to_cpu(g_pSPrintParam->lineCorrectSlice1Line_M);
    g_iLineCorrectPulseMax = le32_to_cpu(g_pSPrintParam->lineCorrectPulseMax_M);
    break;
  case 2:
    g_usPrintMaxPulse = le16_to_cpu(g_pSPrintParam->printMaxPulse_C);
    g_uiMtfWeightH = le16_to_cpu(g_pSPrintParam->mtfWeightH_C);
    g_uiMtfWeightV = le16_to_cpu(g_pSPrintParam->mtfWeightV_C);
    g_uiMtfSlice = le16_to_cpu(g_pSPrintParam->mtfSlice_C);
    g_usLineCorrect_Env_A = le16_to_cpu(g_pSPrintParam->lineCorrectEnvA_C);
    g_usLineCorrect_Env_B = le16_to_cpu(g_pSPrintParam->lineCorrectEnvB_C);
    g_usLineCorrect_Env_C = le16_to_cpu(g_pSPrintParam->lineCorrectEnvC_C);
    g_uiLineCorrectSlice = le32_to_cpu(g_pSPrintParam->lineCorrectSlice_C);
    g_uiLineCorrectSlice1Line = le32_to_cpu(g_pSPrintParam->lineCorrectSlice1Line_C);
    g_iLineCorrectPulseMax = le32_to_cpu(g_pSPrintParam->lineCorrectPulseMax_C);
    break;
  case 3:
    g_usPrintMaxPulse = le16_to_cpu(g_pSPrintParam->printMaxPulse_O);
    g_uiMtfWeightH = le16_to_cpu(g_pSPrintParam->mtfWeightH_O);
    g_uiMtfWeightV = le16_to_cpu(g_pSPrintParam->mtfWeightV_O);
    g_uiMtfSlice = le16_to_cpu(g_pSPrintParam->mtfSlice_O);;
    g_usLineCorrect_Env_A = le16_to_cpu(g_pSPrintParam->lineCorrectEnvA_O);
    g_usLineCorrect_Env_B = le16_to_cpu(g_pSPrintParam->lineCorrectEnvB_O);
    g_usLineCorrect_Env_C = le16_to_cpu(g_pSPrintParam->lineCorrectEnvC_O);
    g_uiLineCorrectSlice = le32_to_cpu(g_pSPrintParam->lineCorrectSlice_O);
    g_uiLineCorrectSlice1Line = le32_to_cpu(g_pSPrintParam->lineCorrectSlice1Line_O);
    g_iLineCorrectPulseMax = le32_to_cpu(g_pSPrintParam->lineCorrectPulseMax_O);
    break;
  default:
    printf("ERROR: bad g_usPrintColor %d\n", g_usPrintColor);
    break;
  }

  g_usHeadDots = le16_to_cpu(g_pSPrintParam->headDots);
}

static void Global_Init(void)
{
  g_usPrintColor = 0;
  g_usPrintSizeWidth = 0;
  g_usPrintSizeHeight = 0;
  g_usSheetSizeWidth = 0;
  g_sPrintSideOffset = 0;
  g_sCorrectSw = 0;
  g_usPrintOpLevel = 0;
  g_uiMtfWeightH = 0;
  g_uiMtfWeightV = 0;
  g_uiMtfSlice = 0;
  g_usPrintMaxPulse = MAX_PULSE;
  g_usMatteMode = 0;
  g_usLineCorrect_Env_A = 0;
  g_usLineCorrect_Env_B = 0;
  g_usLineCorrect_Env_C = 0;
  g_uiLineCorrectSum = 0;
  g_uiLineCorrectBase = 0;
  g_uiLineCorrectBase1Line = 0;
  g_iLineCorrectPulse = 0;
  g_iLineCorrectPulseMax = MAX_PULSE;
  g_pulRandomTable[0] = 3;
  g_pulRandomTable[1] = -1708027847;
  g_pulRandomTable[2] = 853131300;
  g_pulRandomTable[3] = -1687801470;
  g_pulRandomTable[4] = 1570894658;
  g_pulRandomTable[5] = -566525472;
  g_pulRandomTable[6] = -552964171;
  g_pulRandomTable[7] = -251413502;
  g_pulRandomTable[8] = 1223901435;
  g_pulRandomTable[9] = 1950999915;
  g_pulRandomTable[10] = -1095640144;
  g_pulRandomTable[11] = -1420011240;
  g_pulRandomTable[12] = -1805298435;
  g_pulRandomTable[13] = -1943115761;
  g_pulRandomTable[14] = -348292705;
  g_pulRandomTable[15] = -1323376457;
  g_pulRandomTable[16] = 759393158;
  g_pulRandomTable[17] = -630772182;
  g_pulRandomTable[18] = 361286280;
  g_pulRandomTable[19] = -479628451;
  g_pulRandomTable[20] = -1873857033;
  g_pulRandomTable[21] = -686452778;
  g_pulRandomTable[22] = 1873211473;
  g_pulRandomTable[23] = 1634626454;
  g_pulRandomTable[24] = -1399525412;
  g_pulRandomTable[25] = 910245779;
  g_pulRandomTable[26] = -970800488;
  g_pulRandomTable[27] = -173790536;
  g_pulRandomTable[28] = -1970743429;
  g_pulRandomTable[29] = -173171442;
  g_pulRandomTable[30] = -1986452981;
  g_pulRandomTable[31] = 670779321;
  g_uiInputImageIndex = 0;
  g_uiOutputImageIndex = 0;
  g_usHeadDots = 0;

#ifdef S6145_UNUSED

  g_usPrintDummyLevel = 0;
  g_usPrintDummyLine = 0;
  g_usRearDummyPrintLine = 0;
  g_usRearDeleteLine = 0;

  g_usPrintSizeLWidth = 0;
  g_usPrintSizeLHeight = 0;

  g_usThearmHead = 0;
  g_usThearmEnv = 0;
  g_usRearDummyPrintLevel = 0;

  g_usLastPrintSizeWidth = 0;
  g_usLastPrintSizeHeight = 0;
  g_usLastSheetSizeWidth = 0;

  g_iLeadEdgeCorrectPulse = 0;

  g_usCancelCheckLinesForPRec = 118;

  g_pusLamiCompInLineBufTab[0] = (uint16_t*)g_pusInLineBuf0;
  g_pusLamiCompInLineBufTab[1] = (uint16_t*)g_pusInLineBuf2;
  g_pusLamiCompInLineBufTab[2] = g_pusOutLineBuf1;
  g_pusLamiCompInLineBufTab[3] = g_pusOutLineBuf2;

  g_usMatteGloss = 105;
  g_usMatteDeglossBlk = 150;
  g_usMatteDeglossWht = 175;

  g_usPrintOffsetWidth = 0;
  g_usCenterHeadToColSen = 268;

  g_uiLevelAveAddtion = 0;
  g_uiLevelAveCounter = 0;
  g_uiLevelAveCounter2 = 0;
  g_uiLevelAveAddtion2 = 0;
  g_usCancelCheckDotsForPRec = 236;
#endif
}

#ifdef S6145_UNUSED
static void SetTable(void)
{
  SetTableData(g_pSPrintParam->SideEdgeCoefTable, g_pusSideEdgeCoefTable, 128);
  SetTableData(g_pSPrintParam->SideEdgeLvCoefTable, g_pusSideEdgeLvCoefTable, 256);
}
#endif

static void SetTableColor(uint8_t plane)
{
  switch (plane) {
  case 0:
    SetTableData(g_pSPrintParam->pulseTransTable_Y, g_pusPulseTransTable, 256);
    SetTableData(g_pSPrintParam->lineHistCoefTable_Y, g_pusLineHistCoefTable, 256);
    SetTableData(g_pSPrintParam->tankPlusMaxEnergyTable_Y, g_pusTankPlusMaxEnegyTable, 256);
    SetTableData(g_pSPrintParam->tankMinusMaxEnergy_Y, g_pusTankMinusMaxEnegyTable, 256);
    memcpy(g_piTankParam, &g_pSPrintParam->tableTankParam_Y, 128);
    break;
  case 1:
    SetTableData(g_pSPrintParam->pulseTransTable_M, g_pusPulseTransTable, 256);
    SetTableData(g_pSPrintParam->lineHistCoefTable_M, g_pusLineHistCoefTable, 256);
    SetTableData(g_pSPrintParam->tankPlusMaxEnergyTable_M, g_pusTankPlusMaxEnegyTable, 256);
    SetTableData(g_pSPrintParam->tankMinusMaxEnergy_M, g_pusTankMinusMaxEnegyTable, 256);
    memcpy(&g_piTankParam[32], &g_pSPrintParam->tableTankParam_M, 128);
    break;
  case 2:
    SetTableData(g_pSPrintParam->pulseTransTable_C, g_pusPulseTransTable, 256);
    SetTableData(g_pSPrintParam->lineHistCoefTable_C, g_pusLineHistCoefTable, 256);
    SetTableData(g_pSPrintParam->tankPlusMaxEnergyTable_C, g_pusTankPlusMaxEnegyTable, 256);
    SetTableData(g_pSPrintParam->tankMinusMaxEnergy_C, g_pusTankMinusMaxEnegyTable, 256);
    memcpy(&g_piTankParam[64], &g_pSPrintParam->tableTankParam_C, 128);
    break;
  case 3:
    SetTableData(g_pSPrintParam->pulseTransTable_O, g_pusPulseTransTable, 256);
    SetTableData(g_pSPrintParam->lineHistCoefTable_O, g_pusLineHistCoefTable, 256);
    SetTableData(g_pSPrintParam->tankPlusMaxEnergyTable_O, g_pusTankPlusMaxEnegyTable, 256);
    SetTableData(g_pSPrintParam->tankMinusMaxEnergy_O, g_pusTankMinusMaxEnegyTable, 256);
    memcpy(&g_piTankParam[96], &g_pSPrintParam->tableTankParam_O, 128);
    break;
  default:
    printf("ERROR: Bad plane in SetTableColor (%d)\n", plane);
    break;
  }
}

static int32_t CheckPrintParam(uint8_t *corrdataraw)
{
  int i;
  struct imageCorrParam *corrdata = (struct imageCorrParam *) corrdataraw;

  for (i = 0 ; i < 256 ; i++) {
    if (le16_to_cpu(corrdata->pulseTransTable_Y[i]) > le16_to_cpu(corrdata->printMaxPulse_Y) ||
	le16_to_cpu(corrdata->pulseTransTable_M[i]) > le16_to_cpu(corrdata->printMaxPulse_M) ||
	le16_to_cpu(corrdata->pulseTransTable_C[i]) > le16_to_cpu(corrdata->printMaxPulse_C) ||
	le16_to_cpu(corrdata->pulseTransTable_O[i]) > le16_to_cpu(corrdata->printMaxPulse_O)) {
      return 10;
    }
  }

  if (!corrdata->tableTankParam_Y.trdTankSize ||
      !corrdata->tableTankParam_M.trdTankSize ||
      !corrdata->tableTankParam_C.trdTankSize ||
      !corrdata->tableTankParam_O.trdTankSize) {
    return 14;
  }
  if (!corrdata->tableTankParam_Y.sndTankSize ||
      !corrdata->tableTankParam_M.sndTankSize ||
      !corrdata->tableTankParam_C.sndTankSize ||
      !corrdata->tableTankParam_O.sndTankSize) {
    return 15;
  }
  if (!corrdata->tableTankParam_Y.fstTankSize ||
      !corrdata->tableTankParam_M.fstTankSize ||
      !corrdata->tableTankParam_C.fstTankSize ||
      !corrdata->tableTankParam_O.fstTankSize) {
    return 16;
  }
  if (le16_to_cpu(corrdata->val_1) > 1 ||
      le16_to_cpu(corrdata->val_2) > 1 ||
      le16_to_cpu(corrdata->printOpLevel) > 0xff ||
      le16_to_cpu(corrdata->matteMode) > 1) {
    return 17;
  }
  if (le16_to_cpu(corrdata->randomBase[0]) > 0xff ||
      le16_to_cpu(corrdata->randomBase[1]) > 0xff ||
      le16_to_cpu(corrdata->randomBase[2]) > 0xff ||
      le16_to_cpu(corrdata->randomBase[3]) > 0xff) {
    return 18;
  }
  if (!corrdata->matteSize ||
      corrdata->matteSize > 2) {
    return 19;
  }

  if ( le16_to_cpu(corrdata->width) <= MIN_ROWS ||
       le16_to_cpu(corrdata->width) > MAX_ROWS )
    return 20;

  if ( le16_to_cpu(corrdata->height) <= MIN_ROWS ||
       le16_to_cpu(corrdata->height) > MAX_ROWS )
    return 21;

  return 0;
}

/* This resets the preprocess pipeline at the start of a new image
   plane. */
static void LinePrintPreProcess(void)
{
  int16_t i;

  GetInfo();

  if ( !(g_sCorrectSw & 1) )
  {
    g_uiMtfWeightH = 0;
    g_uiMtfWeightV = 0;
    g_uiMtfSlice = 0;
  }

  for ( i = -256; i < 256; i++ )
  {
    if ( (uint32_t)(i * i) >= (uint32_t)(g_uiMtfSlice * g_uiMtfSlice) )
	    g_psMtfPreCalcTable[i+256] = i;
    else
	    g_psMtfPreCalcTable[i+256] = -i;
  }

  g_pusPreReadLineBufTab[0] = g_pusInLineBuf0;
  g_pusPreReadLineBufTab[1] = g_pusInLineBuf1;
  g_pusPreReadLineBufTab[2] = g_pusInLineBuf2;
  g_pusPreReadLineBufTab[3] = g_pusInLineBuf3;
  g_pusPreReadLineBufTab[4] = g_pusInLineBuf4;
  g_pusPreReadLineBufTab[5] = g_pusInLineBuf5;
  g_pusPreReadLineBufTab[6] = g_pusInLineBuf6;
  g_pusPreReadLineBufTab[7] = g_pusInLineBuf7;
  g_pusPreReadLineBufTab[8] = g_pusInLineBuf8;
  g_pusPreReadLineBufTab[9] = g_pusInLineBuf9;
  g_pusPreReadLineBufTab[10] = g_pusInLineBufA;

  memset(g_pusInLineBuf0, 0, sizeof(g_pusInLineBuf0));
  memset(g_pusInLineBuf1, 0, sizeof(g_pusInLineBuf1));
  memset(g_pusInLineBuf2, 0, sizeof(g_pusInLineBuf2));
  memset(g_pusInLineBuf3, 0, sizeof(g_pusInLineBuf3));
  memset(g_pusInLineBuf4, 0, sizeof(g_pusInLineBuf4));
  memset(g_pusInLineBuf5, 0, sizeof(g_pusInLineBuf5));
  memset(g_pusInLineBuf6, 0, sizeof(g_pusInLineBuf6));
  memset(g_pusInLineBuf7, 0, sizeof(g_pusInLineBuf7));
  memset(g_pusInLineBuf8, 0, sizeof(g_pusInLineBuf8));
  memset(g_pusInLineBuf9, 0, sizeof(g_pusInLineBuf9));
  memset(g_pusInLineBufA, 0, sizeof(g_pusInLineBufA));

  g_pusPulseTransLineBufTab[0] = g_pusInLineBuf0;
  g_pusPulseTransLineBufTab[1] = g_pusInLineBuf1;
  g_pusPulseTransLineBufTab[2] = g_pusInLineBuf2;
  g_pusPulseTransLineBufTab[3] = g_pusInLineBuf3;

  memset(g_pusOutLineBuf1, 0, sizeof(g_pusOutLineBuf1));
  g_pusOutLineBufTab[0] = g_pusOutLineBuf1;

#ifdef S6145_UNUSED
  memset(g_pusInLineBuf3, g_usPrintDummyLevel, sizeof(g_pusInLineBuf3)); // XXX redundant with memset above, printDummyLevel is always 0 anyway.
#endif

  g_uiSendToHeadCounter = g_usPrintSizeHeight;
  g_uiLineCopyCounter = g_usPrintSizeHeight;

#ifdef S6145_UNUSED
  g_uiDataTransCounter = g_usPrintSizeHeight;
  g_uiDataTransCounter += g_usPrintDummyLine;
  g_uiDataTransCounter -= g_usRearDeleteLine;
  g_uiDataTransCounter += g_usRearDummyPrintLine;

  g_uiSendToHeadCounter += g_usPrintDummyLine;
  g_uiSendToHeadCounter -= g_usRearDeleteLine;
  g_uiSendToHeadCounter += g_usRearDummyPrintLine;

  g_uiTudenLineCounter = g_usPrintSizeHeight;
  g_uiTudenLineCounter += g_usRearDummyPrintLine;
  g_uiTudenLineCounter -= g_usRearDeleteLine;

  g_uiLineCopyCounter -= g_usRearDeleteLine;

  if ( g_usPrintColor != 3 )
    g_usRearDummyPrintLevel = 255;

  g_iLeadEdgeCorrectPulse = 0;
#endif

  switch (g_usPrintColor) {
  case 0:
    CTankResetParameter(&g_piTankParam[0]);
    g_iMaxPulseValue = g_usPrintMaxPulse;
    g_uiMaxPulseBit = LinePrintCalcBit(g_usPrintMaxPulse);
    g_pfRecieveData = RecieveDataYMC;
#ifdef S6145_UNUSED
    g_pfRecieveData_Post = RecieveDataYMC_Post;
#endif
    g_pfPulseTransPreRead = PulseTransPreReadYMC;
    g_pfTankProcessPreRead = CTankProcessPreRead;
    break;
  case 1:
    CTankResetParameter(&g_piTankParam[32]);
    g_iMaxPulseValue = g_usPrintMaxPulse;
    g_uiMaxPulseBit = LinePrintCalcBit(g_usPrintMaxPulse);
    g_pfRecieveData = RecieveDataYMC;
#ifdef S6145_UNUSED
    g_pfRecieveData_Post = RecieveDataYMC_Post;
#endif
    g_pfPulseTransPreRead = PulseTransPreReadYMC;
    g_pfTankProcessPreRead = CTankProcessPreRead;
    break;
  case 2:
    CTankResetParameter(&g_piTankParam[64]);
    g_iMaxPulseValue = g_usPrintMaxPulse;
    g_uiMaxPulseBit = LinePrintCalcBit(g_usPrintMaxPulse);
    g_pfRecieveData = RecieveDataYMC;
#ifdef S6145_UNUSED
    g_pfRecieveData_Post = RecieveDataYMC_Post;
#endif
    g_pfPulseTransPreRead = PulseTransPreReadYMC;
    g_pfTankProcessPreRead = CTankProcessPreRead;
    break;
  case 3:
    CTankResetParameter(&g_piTankParam[96]);
    g_iMaxPulseValue = g_usPrintMaxPulse;
    g_uiMaxPulseBit = LinePrintCalcBit(g_usPrintMaxPulse);
    if ( g_usMatteMode ) {
      g_pfRecieveData = RecieveDataOP_MATTE;
#ifdef S6145_UNUSED
      g_pfRecieveData_Post = RecieveDataOPMatte_Post;
#endif
    } else {
      g_pfRecieveData = RecieveDataOP_GLOSS;
#ifdef S6145_UNUSED
      g_pfRecieveData_Post = RecieveDataOPLevel_Post;
#endif
    }
    g_pfPulseTransPreRead = PulseTransPreReadOP;
    g_pfTankProcessPreRead = CTankProcessPreReadDummy;
#ifdef S6145_UNUSED
    if ( g_usMatteMode )
      g_iLeadEdgeCorrectPulse = 120;
#endif
    break;
  default:
    printf("ERROR: Bad g_usPrintColor %d\n", g_usPrintColor);
    return;
  }

  g_uiLineCorrectSum = 0;
  g_iLineCorrectPulse = 0;

  if ( g_uiLineCorrectSlice ) {
    g_uiLineCorrectBase = g_uiLineCorrectSlice * g_usLineCorrect_Env_A;
    g_uiLineCorrectBase >>= 15;
    g_uiLineCorrectBase *= g_usSheetSizeWidth;
  } else {
    g_uiLineCorrectBase = -1;
  }

  if ( g_uiLineCorrectSlice1Line ) {
    g_uiLineCorrectBase1Line = g_uiLineCorrectSlice1Line * g_usLineCorrect_Env_B;
    g_uiLineCorrectBase1Line >>= 15;
    g_uiLineCorrectBase1Line *= g_usSheetSizeWidth;
  }

  if ( g_iLineCorrectPulseMax ) {
    g_iLineCorrectPulseMax *= g_usLineCorrect_Env_C;
    g_iLineCorrectPulseMax /= 1024;
  } else {
    g_iLineCorrectPulseMax = MAX_PULSE;
  }

  CTankResetTank();

#ifdef S6145_UNUSED
  g_uiDummyPrintCounter = 0;
#endif
}

static void CTankResetParameter(int32_t *params)
{
  m_iTrdTankSize = le32_to_cpu(params[0]);
  m_iSndTankSize = le32_to_cpu(params[1]);
  m_iFstTankSize = le32_to_cpu(params[2]);
  m_iTrdTankIniEnergy = le32_to_cpu(params[3]);
  m_iSndTankIniEnergy = le32_to_cpu(params[4]);
  m_iFstTankIniEnergy = le32_to_cpu(params[5]);
  m_iTrdTrdConductivity = le32_to_cpu(params[6]);
  m_iSndSndConductivity = le32_to_cpu(params[7]);
  m_iFstFstConductivity = le32_to_cpu(params[8]);
  m_iOutTrdConductivity = le32_to_cpu(params[9]);
  m_iTrdSndConductivity = le32_to_cpu(params[10]);
  m_iSndFstConductivity = le32_to_cpu(params[11]);
  m_iFstOutConductivity = le32_to_cpu(params[12]);
#ifdef S6145_UNUSED
  m_iPlusMaxEnergy = le32_to_cpu(params[13]);
  m_iMinusMaxEnergy = le32_to_cpu(params[14]);
  m_iPlusMaxEnergyPreRead = le32_to_cpu(params[15]);
#endif

  m_iMinusMaxEnergyPreRead = le32_to_cpu(params[16]);
  m_iPreReadLevelDiff = le32_to_cpu(params[17]);

  m_iTankKeisuOutTrdDivTrd = (int64_t)m_iOutTrdConductivity * (int64_t)0x10000 / (int64_t)m_iTrdTankSize;
  m_iTankKeisuTrdSndDivTrd = (int64_t)m_iTrdSndConductivity * (int64_t)0x10000 / (int64_t)m_iTrdTankSize;
  m_iTankKeisuTrdSndDivSnd = (int64_t)m_iTrdSndConductivity * (int64_t)0x10000 / (int64_t)m_iSndTankSize;
  m_iTankKeisuSndFstDivSnd = (int64_t)m_iSndFstConductivity * (int64_t)0x10000 / (int64_t)m_iSndTankSize;
  m_iTankKeisuSndFstDivFst = (int64_t)m_iSndFstConductivity * (int64_t)0x10000 / (int64_t)m_iFstTankSize;
  m_iTankKeisuFstOutDivFst = (int64_t)m_iFstOutConductivity * (int64_t)0x10000 / (int64_t)m_iFstTankSize;

  return;
}

static void CTankResetTank(void)
{
  int i;

  for (i = 0 ; i < TANK_SIZE; i++) {
    m_piTrdTankArray[i] = m_iTrdTankIniEnergy;
    m_piSndTankArray[i] = m_iSndTankIniEnergy;
    m_piFstTankArray[i] = m_iFstTankIniEnergy;
  }
}

/* This primes the preprocessing pipeline prior to starting the first
   actual row of image data */
static void PagePrintPreProcess(void)
{
  uint32_t i;

  g_pusPulseTransLineBufTab[3] = g_pusPreReadLineBufTab[1];
  g_pfRecieveData();
  g_pusPulseTransLineBufTab[1] = g_pusPulseTransLineBufTab[3];
  g_uiLineCopyCounter++;
  g_uiInputImageIndex -= g_usPrintSizeWidth;
  g_pusPulseTransLineBufTab[3] = g_pusPreReadLineBufTab[2];
  g_pfRecieveData();
  g_pusPulseTransLineBufTab[2] = g_pusPulseTransLineBufTab[3];
  g_pusPulseTransLineBufTab[3] = g_pusPreReadLineBufTab[3];
  g_pfRecieveData();
  for ( i = 0; i < 7; i++ )
  {
    g_pusPulseTransLineBufTab[3] = g_pusPreReadLineBufTab[i + 4];
    g_pfRecieveData();
  }
  g_pusPulseTransLineBufTab[0] = g_pusPreReadLineBufTab[0];
}

/* Process a single scanline,
   From reading the input data to writing the output.
 */
static void PagePrintProcess(void)
{
  uint32_t i;

  /* First, rotate the input buffers... */
  if ( g_usPrintColor != 3 || g_usMatteMode != 1 || g_usMatteSize != 2 ) {
    /* If we're not printing a matte layer... */
    uint8_t *v4 = g_pusPreReadLineBufTab[0];
    for ( i = 0; i < 10; i++ )
      g_pusPreReadLineBufTab[i] = g_pusPreReadLineBufTab[i + 1];
    g_pusPreReadLineBufTab[10] = v4;
    g_pusPulseTransLineBufTab[0] = g_pusPreReadLineBufTab[0];
    g_pusPulseTransLineBufTab[1] = g_pusPreReadLineBufTab[1];
    g_pusPulseTransLineBufTab[2] = g_pusPreReadLineBufTab[2];
    g_pusPulseTransLineBufTab[3] = g_pusPreReadLineBufTab[10];
  } else if ( g_uiLineCopyCounter & 1 ) {
    /* in other words, every other line when printing a matte layer..  */
    uint8_t *v4 = g_pusPreReadLineBufTab[0];
    for ( i = 0; i < 10; i++ )
      g_pusPreReadLineBufTab[i] = g_pusPreReadLineBufTab[i + 1];
    g_pusPreReadLineBufTab[10] = v4;
    g_pusPulseTransLineBufTab[0] = g_pusPreReadLineBufTab[0];
    g_pusPulseTransLineBufTab[1] = g_pusPreReadLineBufTab[1];
    g_pusPulseTransLineBufTab[2] = g_pusPreReadLineBufTab[2];
    g_pusPulseTransLineBufTab[3] = g_pusPreReadLineBufTab[10];
  }

#ifdef S6145_UNUSED
  g_uiTudenLineCounter--;
#endif
  g_pfRecieveData(); /* Read another scanline */
  PulseTrans();
  g_pfPulseTransPreRead();
#ifdef S6145_UNUSED
  g_pfRecieveData_Post();  /* Clean up after the receive */
#endif
  CTankProcess();  /* Update thermal tank state */
  g_pfTankProcessPreRead();
  LineCorrection(); /* Final output compensation */
  SendData();      /* Write scanline output */
  return;
}

static uint16_t LinePrintCalcBit(uint16_t val)
{
  uint16_t bit = 0;
  while (val) {
    val >>= 1;
    bit++;
  }
  return bit;
}

/* Update thermal tank state */
static void CTankProcess(void)
{
  if ( g_sCorrectSw & 2 ) {
    CTankHosei();
    CTankUpdateTankVolumeInterRay();
    CTankUpdateTankVolumeInterDot(0);
    CTankUpdateTankVolumeInterDot(1);
    CTankUpdateTankVolumeInterDot(2);
  }
  return;
}

static void CTankProcessPreRead(void)
{
  if (g_sCorrectSw & 2)
     CTankHoseiPreread();
}

static void CTankProcessPreReadDummy(void)
{
  return;
}

/* This will generate one line worth of "gloss" OC data.
   It only covers the imageable area, rather than the head width */
static void RecieveDataOP_GLOSS(void)
{
  if ( g_uiLineCopyCounter ) {
     memset(g_pusPulseTransLineBufTab[3] + ((g_usHeadDots - g_usSheetSizeWidth) / 2),
	    g_usPrintOpLevel,
	    g_usSheetSizeWidth);

    g_uiLineCopyCounter--;
  }

  return;
}

/* This reads a single line worth of input image data.
 */
static void RecieveDataYMC(void)
{
  uint8_t *v1;
  int16_t i;

  v1 = g_pusPulseTransLineBufTab[3] + ((g_usHeadDots - g_usSheetSizeWidth) / 2);

  if ( g_uiLineCopyCounter ) {
     /* Read the next line */
    for ( i = 0; i < g_usPrintSizeWidth; i++ )
      v1[i] = g_pucInputImageBuf[g_uiInputImageIndex++];
    --g_uiLineCopyCounter;
  } else {
    /* Re-read the previous line */
    g_uiInputImageIndex -= g_usPrintSizeWidth;
    for ( i = 0; i < g_usPrintSizeWidth ; i++ )
      v1[i] = g_pucInputImageBuf[g_uiInputImageIndex++];
  }
}

/* this will generate one scanline (ie 16b * BUF_SIZE) worth of
   "random" data for the matte overcoat */
static void RecieveDataOP_MATTE(void)
{
  if ( g_uiLineCopyCounter ) {
    int32_t v1;
    uint32_t v5;
    int32_t v6;

    int16_t matteCtr;
    uint8_t *outPtr = g_pusPulseTransLineBufTab[3];

    if ( g_usMatteSize == 2 )
      matteCtr = 256;
    else
      matteCtr = 512;

    while ( matteCtr-- ) {
      if ( g_pulRandomTable[0] >= 31 )
        v6 = 1;
      else
        v6 = g_pulRandomTable[0] + 1;
      g_pulRandomTable[0] = v6;
      if ( v6 <= 3 )
        v1 = g_pulRandomTable[v6 + 28];
      else
        v1 = g_pulRandomTable[v6 - 3];
      g_pulRandomTable[v6] += v1;

      v5 = (uint32_t)g_pulRandomTable[v6] >> 1;
      if ( g_usMatteSize == 2 ) {
	*outPtr++ = g_ucRandomBaseLevel[(v5 >> 1) & 3];
        *outPtr++ = g_ucRandomBaseLevel[(v5 >> 1) & 3];
        *outPtr++ = g_ucRandomBaseLevel[(v5 >> 5) & 3];
        *outPtr++ = g_ucRandomBaseLevel[(v5 >> 5) & 3];
        *outPtr++ = g_ucRandomBaseLevel[(v5 >> 9) & 3];
        *outPtr++ = g_ucRandomBaseLevel[(v5 >> 9) & 3];
        *outPtr++ = g_ucRandomBaseLevel[(v5 >> 13) & 3];
        *outPtr++ = g_ucRandomBaseLevel[(v5 >> 13) & 3];
      } else {
	*outPtr++ = g_ucRandomBaseLevel[(v5 >> 1) & 3];
        *outPtr++ = g_ucRandomBaseLevel[(v5 >> 5) & 3];
        *outPtr++ = g_ucRandomBaseLevel[(v5 >> 9) & 3];
        *outPtr++ = g_ucRandomBaseLevel[(v5 >> 13) & 3];
      }
    }
    --g_uiLineCopyCounter;
  }
}

/* This writes a single scanline to the output buffer */
static void SendData(void)
{
  uint16_t i;

  if ( g_uiSendToHeadCounter ) {
    for ( i = 0; i < g_usHeadDots; i++ )
      g_pusOutputImageBuf[g_uiOutputImageIndex++] = cpu_to_le16(g_pusOutLineBufTab[0][i]);
    --g_uiSendToHeadCounter;
  }
}

/* Use the previous two rows to generate the needed impulse for
   the current row. */
static void PulseTrans(void)
{
  int32_t overHang;
  int32_t sheetSizeWidth;

  uint8_t *prevPrevRow;
  uint8_t *prevRow;
  uint8_t *currentRow;

  uint16_t *out;

  sheetSizeWidth = g_usSheetSizeWidth;
  overHang = (g_usHeadDots - g_usSheetSizeWidth) / 2;

  currentRow = g_pusPulseTransLineBufTab[0] + overHang;
  prevRow = g_pusPulseTransLineBufTab[1] + overHang;
  prevPrevRow = g_pusPulseTransLineBufTab[2] + overHang;
  out = g_pusOutLineBufTab[0] + g_sPrintSideOffset + overHang;

  if ( out >= g_pusOutLineBufTab[0] ) {
    int32_t offset = g_sPrintSideOffset
        + g_usSheetSizeWidth
        + overHang;
    if ( offset > BUF_SIZE )
      sheetSizeWidth = g_usSheetSizeWidth - (offset - BUF_SIZE);
  } else {
    int32_t offset = (g_pusOutLineBufTab[0] - out);
    out = g_pusOutLineBufTab[0];
    sheetSizeWidth = g_usSheetSizeWidth - offset;
    currentRow += offset;
    prevRow += offset;
    prevPrevRow += offset;
    printf("WARN: PulseTrans() alt path\n");
  }

  while ( sheetSizeWidth-- ) {
    int32_t tableOffset;
    uint16_t compVal;

    uint8_t *v2;
    int32_t v3;
    int32_t v4;
    int32_t v6;
    int32_t v12;

    v2 = prevRow - 1;
    v3 = *v2++;
    v12 = *v2;
    prevRow = v2 + 1;
    v4 = g_psMtfPreCalcTable[256 + v12 - *prevRow] + g_psMtfPreCalcTable[256 + v12 - v3];
    v6 = g_psMtfPreCalcTable[256 + v12 - *currentRow++];

    tableOffset = v12 + ((v4 * g_uiMtfWeightH + (g_psMtfPreCalcTable[256 + v12 - *prevPrevRow++] + v6) * g_uiMtfWeightV) >> 7);
    if ( tableOffset > 255 )
      tableOffset = 255;
    if ( tableOffset <= 0 )
      tableOffset = 1;
    if ( !v12 )
      tableOffset = 0;

    compVal = g_pusPulseTransTable[tableOffset];
    if ( compVal > MAX_PULSE )
      compVal = MAX_PULSE;

    *out++ = compVal;
  }

#ifdef S6145_UNUSED
  g_uiDataTransCounter--;
#endif
}

static void PulseTransPreReadOP(void)
{

}

static void PulseTransPreReadYMC(void)
{
  uint16_t overHang;
  uint16_t printSizeWidth;
  uint16_t *out;

#ifdef S6145_UNUSED
  uint8_t *v1;
  uint8_t *v2;
  uint8_t *v3;
  uint8_t *v4;
#endif

  uint8_t *v14;
  uint8_t *v15;
  uint8_t *v16;
  uint8_t *v17;

  printSizeWidth = g_usPrintSizeWidth;
  overHang = (g_usHeadDots - g_usPrintSizeWidth) / 2;
  v17 = g_pusPreReadLineBufTab[2] + overHang;
  v16 = g_pusPreReadLineBufTab[3] + overHang;
  v15 = g_pusPreReadLineBufTab[4] + overHang;
  v14 = g_pusPreReadLineBufTab[5] + overHang;

#ifdef S6145_UNUSED
  v1 = g_pusPreReadLineBufTab[6] + overHang;
  v2 = g_pusPreReadLineBufTab[7] + overHang;
  v3 = g_pusPreReadLineBufTab[8] + overHang;
  v4 = g_pusPreReadLineBufTab[9] + overHang;
#endif

  out = g_pusPreReadOutLineBuf + overHang + g_sPrintSideOffset;

  if ( out < g_pusPreReadOutLineBuf ) {
    int32_t offset = (g_pusPreReadOutLineBuf - out);
    out = g_pusPreReadOutLineBuf;
    printSizeWidth = g_usPrintSizeWidth - offset;
    v17 += offset;
    v16 += offset;
    v15 += offset;
    v14 += offset;
    printf("WARN: PulseTransPreReadYMC alt path!\n");
  }

  while ( printSizeWidth-- ) {
    int32_t v6 = *v17++;
    int32_t v7 = *v16++ + v6;
    int32_t v8 = *v15++ + v7;
    int32_t v9 = *v14++ + v8;
    int32_t pixel = g_pusPulseTransTable[v9 / 4];
    if ( pixel > MAX_PULSE )
      pixel = MAX_PULSE;
    *out++ = pixel;
  }
}

static void CTankUpdateTankVolumeInterDot(uint8_t tank)
{
  int32_t *tankIn;
  int32_t *tankOut;
  int32_t conductivity;
  uint16_t sheetSizeWidth;

  uint16_t v1;
  int32_t v2;
  int32_t v4;
  int32_t v5;
  int32_t v8;
  int32_t v17;
  int32_t v18;
  int32_t v19;
  int32_t v20;

  switch (tank) {
  case 0:
    tankIn = m_piFstTankArray;
    tankOut = m_piFstTankArray + 2;
    conductivity = m_iFstFstConductivity / 2;
    break;
  case 1:
    tankIn = m_piSndTankArray;
    tankOut = m_piSndTankArray + 2;
    conductivity = m_iSndSndConductivity / 2;
    break;
  case 2:
    tankIn = m_piTrdTankArray;
    tankOut = m_piTrdTankArray + 2;
    conductivity = m_iTrdTrdConductivity / 2;
    break;
  default:
    printf("ERROR: Bad Tank %d in CTankUpdateVolumeInterDot\n", tank);
    return;
  }

  /* This code basically takes a running average of three running
     averages, and uses that as the basis for the output */

  tankIn[0] = tankIn[1] = tankIn[2];
  v1 = g_usSheetSizeWidth + 1;
  tankIn[v1+1] = tankIn[v1+2] = tankIn[v1];
  v2 = *tankIn++;
  v4 = *tankIn++;
  v5 = *tankIn++;
  v8 = *tankIn++;
  v20 = *tankIn++;

  v19 = conductivity * (v5 + v2  - 2 * v4);
  v18 = conductivity * (v8 + v4  - 2 * v5);
  v17 = conductivity * (v20 + v5 - 2 * v8);
  sheetSizeWidth = g_usSheetSizeWidth;

  while ( sheetSizeWidth-- ) {
    int32_t pixel = (v18 >> 6) + v5 - (conductivity * ((2 * v18 - v19 - v17) >> 7) >> 7);
    if ( pixel < 0 )
      pixel = 0;
    *tankOut++ = pixel;
    v5 = v8;
    v8 = v20;
    v20 = *tankIn++;
    v19 = v18;
    v18 = v17;
    v17 = conductivity * (v20 + v5 - 2 * v8);
  }
}

static void CTankUpdateTankVolumeInterRay(void)
{
  uint16_t sheetWidth = g_usSheetSizeWidth;
  int32_t *fstTankPtr = m_piFstTankArray + 2;
  int32_t *sndTankPtr = m_piSndTankArray + 2;
  int32_t *trdTankPtr = m_piTrdTankArray + 2;

  while ( sheetWidth-- ) {
    int32_t v2, v3;

    v2 = (*sndTankPtr * m_iTankKeisuSndFstDivSnd - *fstTankPtr * m_iTankKeisuSndFstDivFst) >> 17;
    *fstTankPtr = v2 + *fstTankPtr - (*fstTankPtr * m_iTankKeisuFstOutDivFst >> 17);
    fstTankPtr++;

    v3 = (*trdTankPtr * m_iTankKeisuTrdSndDivTrd - *sndTankPtr * m_iTankKeisuTrdSndDivSnd) >> 17;
    *sndTankPtr = v3 + *sndTankPtr - v2;
    sndTankPtr++;

    *trdTankPtr = *trdTankPtr - v3 - (*trdTankPtr * m_iTankKeisuOutTrdDivTrd >> 17);
    trdTankPtr++;
  }
}

static void CTankHoseiPreread(void)
{
  uint16_t sheetWidth;
  int16_t overHang;
  int32_t *fstTankPtr;
  uint16_t *outPtr;
  int16_t *inPtr;  /* Must treat this as SIGNED! */

  int32_t v4;

  overHang = (g_usHeadDots - g_usSheetSizeWidth) / 2;
  inPtr = (int16_t*)g_pusPreReadOutLineBuf + overHang + g_sPrintSideOffset;
  outPtr = g_pusOutLineBufTab[0] + overHang + g_sPrintSideOffset;
  if ( outPtr < g_pusOutLineBufTab[0] )
    outPtr = g_pusOutLineBufTab[0];
  fstTankPtr = m_piFstTankArray + 2;
  v4 = (1 << (g_uiMaxPulseBit + 20)) / m_iFstTankSize;

  /* Walk forward through the line to compute the necessary delta */
  sheetWidth = g_usSheetSizeWidth;
  while ( sheetWidth-- ) {
    int32_t v5 = *inPtr - (v4 * (*inPtr + *fstTankPtr++) >> 20);
    int32_t v6 = 0;
    if ( v5 < m_iPreReadLevelDiff )
      v6 = -(m_iMinusMaxEnergyPreRead * v5 * v5) >> g_uiMaxPulseBit;
    *inPtr++ = v6;
  }

  /* Now walk backwards through the line to derive the desired pixel
     values, adding the actual value with the necessary delta.. */
  outPtr += g_usSheetSizeWidth;

  sheetWidth = g_usSheetSizeWidth;
  while ( sheetWidth-- ) {
    int32_t pixel;

    inPtr--;
    outPtr--;

    pixel = *inPtr + *outPtr;
    if ( pixel < 0 )
      pixel = 0;
    if ( pixel > g_iMaxPulseValue )
      pixel = g_iMaxPulseValue;
    *outPtr = pixel;
  }
}

/* Apply the correction needed based on the thermal tanks */
static void CTankHosei(void)
{
  uint16_t overHang;
  uint16_t sheetSizeWidth;
  int32_t *tankPtr;
  uint8_t *in;
  uint16_t *out;

  int32_t v4;
#ifdef S6145_UNUSED
  int32_t v2;
  int32_t *v12;
#endif

  sheetSizeWidth = g_usSheetSizeWidth;
  overHang = (g_usHeadDots - g_usSheetSizeWidth) / 2;
  out = g_pusOutLineBufTab[0] + (overHang + g_sPrintSideOffset);
  in = g_pusPulseTransLineBufTab[1] + overHang;

  if ( out >= g_pusOutLineBufTab[0] ) {
    int32_t offset = g_sPrintSideOffset + sheetSizeWidth + overHang;
    if ( offset > BUF_SIZE ) {
      offset -= BUF_SIZE;
      sheetSizeWidth -= offset;
    }
  } else {
    int32_t offset = (g_pusOutLineBufTab[0] - out);
    sheetSizeWidth -= offset;
    in += (out - g_pusOutLineBufTab[0]); // XXX was: in += out;
    out = g_pusOutLineBufTab[0];
    printf("WARN: CTankHosei() alt path\n");
  }
  tankPtr = m_piFstTankArray + 2;

#ifdef S6145_UNUSED
  v2 = m_iPlusMaxEnergy;
  v12 = &v2;
#endif

  v4 = (1 << (g_uiMaxPulseBit + 20)) / m_iFstTankSize;

  while ( sheetSizeWidth-- ) {
    int32_t v5;
    int32_t v8;
    uint16_t v11;
    uint32_t v3 = *in++;
    v5 = *out - ((v4 * (*out + *tankPtr)) >> 20);
    if ( v5 < 0 )
      v11 = g_pusTankMinusMaxEnegyTable[v3];
    else
      v11 = g_pusTankPlusMaxEnegyTable[v3];
    v8 = *out + ((v5 * v11) >> g_uiMaxPulseBit);
    if ( v8 < 0 )
      v8 = 0;
    if ( v8 > g_iMaxPulseValue )
      v8 = g_iMaxPulseValue;
    *out++ = v8;
    *tankPtr++ += v8;
  }
}

/* Apply final corrections to the output. */
#define LINECORR_BUCKETS 4
static void LineCorrection(void)
{
  uint16_t sheetSizeWidth;
  uint16_t overHang;
  uint8_t *in;
  uint16_t *out;
  uint32_t bucket[LINECORR_BUCKETS];
  uint32_t correct;
  uint8_t i;

  sheetSizeWidth = g_usSheetSizeWidth;
  overHang = (g_usHeadDots - g_usSheetSizeWidth) / 2;
  in = g_pusPulseTransLineBufTab[1] + overHang;
  out = g_pusOutLineBufTab[0] + overHang + g_sPrintSideOffset;
  if ( out >= g_pusOutLineBufTab[0] ) {
    uint32_t tmp = g_sPrintSideOffset + sheetSizeWidth + overHang;
    if ( tmp > BUF_SIZE ) {
      tmp -= BUF_SIZE;
      sheetSizeWidth -= tmp;
    }
  } else {
    uint32_t tmp = g_pusOutLineBufTab[0] - out;
    sheetSizeWidth -= tmp;
    in += (out - g_pusOutLineBufTab[0]); // XXX was: in += out;
    out = g_pusOutLineBufTab[0];
    printf("WARN: LineCorrection() alt path\n");
  }

  /* Apply the correction compensation */
  bucket[0] = bucket[1] = bucket[2] = bucket[3] = 0;
  for ( i = 0; i < LINECORR_BUCKETS; i++ ) {
    uint16_t j = sheetSizeWidth / LINECORR_BUCKETS;
    while ( j-- ) {
      int32_t pixel = *out;
      bucket[i] += pixel;
      pixel -= g_pusLineHistCoefTable[*in++] * g_iLineCorrectPulse / 1024;
      if ( pixel < 0 )
        pixel = 0;
      *out++ = pixel;
    }
  }

  /* See if we need to increase the correction compensation */
  correct = 0;
  for ( i = 0; i < LINECORR_BUCKETS; i++ ) {
    if ( g_uiLineCorrectBase1Line / LINECORR_BUCKETS <= bucket[i] )
      correct++;
  }
  if ( correct ) {
    for ( i = 0; i < LINECORR_BUCKETS; i++ )
      g_uiLineCorrectSum += bucket[i];
  }
  if ( g_uiLineCorrectSum > g_uiLineCorrectBase ) {
    g_uiLineCorrectSum -= g_uiLineCorrectBase;
    if ( g_iLineCorrectPulse < g_iLineCorrectPulseMax )
      g_iLineCorrectPulse++;
  }

}

#ifdef S6145_UNUSED
/* XXX all of these functions are present in the library, but not actually
   referenced by anything, so there's no point in worrying about them. */
static void SideEdgeCorrection(void)
{
  int32_t v0;
  uint32_t v1;
  uint8_t *v4;
  uint16_t *v5;
  uint8_t *v6;
  uint8_t *v7;
  uint16_t *out;
  uint16_t *v9;
  uint32_t v10;
  uint32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;

  v16 = g_usSheetSizeWidth;
  v0 = (g_usHeadDots - g_usSheetSizeWidth) / 2;
  v6 = g_pusPulseTransLineBufTab[1] + v0;
  out = g_pusOutLineBufTab[0] + g_sPrintSideOffset + v0;
  v11 = 0;
  if ( out >= g_pusOutLineBufTab[0] ) {
    v10 = g_sPrintSideOffset
        + g_usSheetSizeWidth
        + v0;
    if ( v10 > BUF_SIZE )
    {
      v10 -= BUF_SIZE;
      v16 = g_usSheetSizeWidth - v10;
    }
  } else {
    v1 = g_pusOutLineBufTab[0] - out;
    out = g_pusOutLineBufTab[0];
    v16 = g_usSheetSizeWidth - v1;
    v10 = v11;
  }
  v5 = out + 2 * v16;
  v4 = v16 + v6;
  v14 = 128 - v11;

  while ( v14 ) {
    v12 = (((1024 - g_pusSideEdgeLvCoefTable[*v6++] * (uint32_t)g_pusSideEdgeCoefTable[128 - v14--]) >> 10) * *out) >> 10;
    if ( v12 > g_iMaxPulseValue )
      v12 = g_iMaxPulseValue;
    *out++ = v12;
  }

  v9 = v5;
  v7 = v4;
  v15 = 128 - v10;

  while ( v15 ) {
    v9--;
    --v7;
    v13 = ((1024 - (g_pusSideEdgeLvCoefTable[*v7] * (uint32_t)g_pusSideEdgeCoefTable[128 - v15--]) >> 10) * *v9) >> 10;
    if ( g_iMaxPulseValue < v13 )
      v13 = g_iMaxPulseValue;
    *v9 = v13;
  }
}

static void LeadEdgeCorrection(void)
{
  uint32_t v0;
  uint16_t *out;
  int32_t v4;
  uint32_t v5;
  int32_t v6;

  if ( g_iLeadEdgeCorrectPulse ) {
    v6 = g_usSheetSizeWidth;
    out = g_pusOutLineBufTab[0] + g_sPrintSideOffset
       + ((g_usHeadDots - g_usSheetSizeWidth) / 2);
    if ( out >= g_pusOutLineBufTab[0] ) {
      v5 = g_sPrintSideOffset
         + g_usSheetSizeWidth
         + ((g_usHeadDots - g_usSheetSizeWidth) / 2);
      if ( v5 > BUF_SIZE )
        v6 = g_usSheetSizeWidth - (v5 - BUF_SIZE);
    } else {
      v0 = g_pusOutLineBufTab[0] - out;
      out = g_pusOutLineBufTab[0];
      v6 = g_usSheetSizeWidth - v0;
    }

    while ( v6-- ) {
      v4 = (g_iLeadEdgeCorrectPulse / 4) + *out;
      if ( v4 > g_iMaxPulseValue )
        v4 = g_iMaxPulseValue;
      *out++ = v4;
    }
    --g_iLeadEdgeCorrectPulse;
  }
}

static void RecieveDataOP_Post(void)
{
  return;
}

static void RecieveDataYMC_Post(void)
{
  return;
}

static void RecieveDataOPLevel_Post(void)
{
  return;
}

static void RecieveDataOPMatte_Post(void)
{
  return;
}

static void ImageLevelAddition(void)
{
  if ( g_uiLevelAveCounter < g_usPrintSizeHeight )
  {
    ImageLevelAdditionEx(&g_uiLevelAveAddtion, 0, g_usSheetSizeWidth);
    g_uiLevelAveCounter++;
    if ( g_uiLevelAveCounter2-- == 0 )
    {
      ImageLevelAdditionEx(
	      &g_uiLevelAveAddtion2,
	      g_uiOffsetCancelCheckPRec,
	      g_usCancelCheckDotsForPRec);
      ++g_uiLevelAveCounter2;
    }
  }
}

static void ImageLevelAdditionEx(uint32_t *a1, uint32_t a2, int32_t a3)
{
  int32_t v3;
  uint8_t *v6;
  int32_t i;
  int32_t v8;

  v6 = g_pusPulseTransLineBufTab[1] + a2 +
	  ((g_usHeadDots - g_usSheetSizeWidth) / 2);
  v8 = a3;
  for ( i = 0; v8-- ; i += v3 ) {
    v3 = *v6++;
  }
  *a1 += i;
}

#endif /* S6145_UNUSED */
