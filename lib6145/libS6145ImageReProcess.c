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

#define LIB_VERSION "0.5.0"

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

struct lib6145_ctx;  /* Forward-declaration */

static void SetTableData(void *src, void *dest, uint16_t words);
static int32_t CheckPrintParam(struct imageCorrParam *corrdata);
static uint16_t LinePrintCalcBit(uint16_t val);

static void GetInfo(struct lib6145_ctx *ctx);
static void Global_Init(struct lib6145_ctx *ctx);
static void SetTableColor(struct lib6145_ctx *ctx, uint8_t plane);
static void LinePrintPreProcess(struct lib6145_ctx *ctx);
static void CTankResetParameter(struct lib6145_ctx *ctx, int32_t *params);
static void CTankResetTank(struct lib6145_ctx *ctx);
static void PagePrintPreProcess(struct lib6145_ctx *ctx);
static void PagePrintProcess(struct lib6145_ctx *ctx);
static void CTankProcess(struct lib6145_ctx *ctx);
static void SendData(struct lib6145_ctx *ctx);
static void PulseTrans(struct lib6145_ctx *ctx);
static void CTankUpdateTankVolumeInterDot(struct lib6145_ctx *ctx, uint8_t tank);
static void CTankUpdateTankVolumeInterRay(struct lib6145_ctx *ctx);
static void CTankHoseiPreread(struct lib6145_ctx *ctx);
static void CTankHosei(struct lib6145_ctx *ctx);
static void LineCorrection(struct lib6145_ctx *ctx);

static void PulseTransPreReadOP(struct lib6145_ctx *ctx);
static void PulseTransPreReadYMC(struct lib6145_ctx *ctx);
static void CTankProcessPreRead(struct lib6145_ctx *ctx);
static void CTankProcessPreReadDummy(struct lib6145_ctx *ctx);
static void RecieveDataOP_GLOSS(struct lib6145_ctx *ctx);
static void RecieveDataYMC(struct lib6145_ctx *ctx);
static void RecieveDataOP_MATTE(struct lib6145_ctx *ctx);

#ifdef S6145_UNUSED
static void SetTable(struct lib6145_ctx *ctx);
static void ImageLevelAddition(struct lib6145_ctx *ctx);
static void ImageLevelAdditionEx(struct lib6145_ctx *ctx, uint32_t *a1, uint32_t a2, int32_t a3);
static void RecieveDataOP_Post(struct lib6145_ctx *ctx);
static void RecieveDataYMC_Post(struct lib6145_ctx *ctx);
static void RecieveDataOPLevel_Post(struct lib6145_ctx *ctx);
static void RecieveDataOPMatte_Post(struct lib6145_ctx *ctx);
static void SideEdgeCorrection(struct lib6145_ctx *ctx);
static void LeadEdgeCorrection(struct lib6145_ctx *ctx);
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

/* global context */
struct lib6145_ctx {
	void (*pfRecieveData)(struct lib6145_ctx *ctx);
	void (*pfPulseTransPreRead)(struct lib6145_ctx *ctx);
	void (*pfTankProcessPreRead)(struct lib6145_ctx *ctx);

	uint8_t pusInLineBuf0[BUF_SIZE];
	uint8_t pusInLineBuf1[BUF_SIZE];
	uint8_t pusInLineBuf2[BUF_SIZE];
	uint8_t pusInLineBuf3[BUF_SIZE];
	uint8_t pusInLineBuf4[BUF_SIZE];
	uint8_t pusInLineBuf5[BUF_SIZE];
	uint8_t pusInLineBuf6[BUF_SIZE];
	uint8_t pusInLineBuf7[BUF_SIZE];
	uint8_t pusInLineBuf8[BUF_SIZE];
	uint8_t pusInLineBuf9[BUF_SIZE];
	uint8_t pusInLineBufA[BUF_SIZE];

	uint16_t pusOutLineBuf1[BUF_SIZE];
	uint16_t *pusOutLineBufTab[2]; // XXX actually [1]

	uint8_t *pusPreReadLineBufTab[12]; // XXX actually [11]
	uint8_t *pusPulseTransLineBufTab[4];
	uint16_t pusPreReadOutLineBuf[BUF_SIZE];

	 int16_t psMtfPreCalcTable[512];
	uint16_t pusTankMinusMaxEnegyTable[256];
	uint16_t pusTankPlusMaxEnegyTable[256];
	uint16_t pusPulseTransTable[256];
	uint16_t pusLineHistCoefTable[256];

	 int32_t piTankParam[128];   // should be struct tankParam[4]
	 int32_t pulRandomTable[32]; // should be u32

	uint8_t  *pucInputImageBuf;
	struct imageCorrParam *pSPrintParam;
	uint16_t *pusOutputImageBuf;

	uint8_t  ucRandomBaseLevel[4];
	 int16_t sPrintSideOffset;
	uint16_t usHeadDots;
	 int32_t iLineCorrectPulse;
	uint32_t uiMtfSlice;    // really u16?
	uint32_t uiMtfWeightV;  // really u16?
	uint32_t uiMtfWeightH;  // really u16?
	uint16_t usLineCorrect_Env_A;
	uint16_t usLineCorrect_Env_B;
	uint16_t usLineCorrect_Env_C;

	uint32_t uiOutputImageIndex;
	uint32_t uiInputImageIndex;

	 int32_t iMaxPulseValue;
	uint32_t uiMaxPulseBit;

	uint16_t usPrintMaxPulse;
	uint16_t usPrintOpLevel;
	uint16_t usMatteSize;
	uint32_t uiLineCorrectSlice;
	uint32_t uiLineCorrectSlice1Line;
	uint16_t usPrintSizeHeight;
	uint32_t uiLineCorrectBase1Line;
	uint32_t uiLineCorrectSum;
	uint32_t uiLineCorrectBase;
	 int16_t sCorrectSw;
	uint16_t usMatteMode;
	 int32_t iLineCorrectPulseMax;
	uint16_t usSheetSizeWidth;
	uint16_t usPrintSizeWidth;
	uint16_t usPrintColor;
	uint32_t uiSendToHeadCounter;
	uint32_t uiLineCopyCounter;

	/* Not sure the significance of 'm_' */
	 int32_t m_piTrdTankArray[TANK_SIZE];
	 int32_t m_piFstTankArray[TANK_SIZE];
	 int32_t m_piSndTankArray[TANK_SIZE];

	 int32_t m_iTrdTankSize;
	 int32_t m_iTrdSndConductivity;
	 int32_t m_iSndTankSize;
	 int32_t m_iTankKeisuSndFstDivFst;
	 int32_t m_iSndSndConductivity;
	 int32_t m_iTrdTrdConductivity;
	 int32_t m_iTankKeisuTrdSndDivSnd;
	 int32_t m_iTankKeisuTrdSndDivTrd;
	 int32_t m_iSndFstConductivity;
	 int32_t m_iFstTankSize;
	 int32_t m_iTrdTankIniEnergy;
	 int32_t m_iFstTankIniEnergy;
	 int32_t m_iTankKeisuSndFstDivSnd;
	 int32_t m_iSndTankIniEnergy;
	 int32_t m_iPreReadLevelDiff;
	 int32_t m_iMinusMaxEnergyPreRead;
	 int32_t m_iOutTrdConductivity;
	 int32_t m_iFstOutConductivity;
	 int32_t m_iFstFstConductivity;
	 int32_t m_iTankKeisuFstOutDivFst;
	 int32_t m_iTankKeisuOutTrdDivTrd;

#ifdef S6145_UNUSED
	void (*pfRecieveData_Post)(struct lib6145_ctx *ctx);  /* all users are no-ops */

/* Set but never referenced */
	uint32_t uiDataTransCounter;
	uint32_t uiTudenLineCounter;

/* Only ever set to 0 */
	uint16_t usPrintDummyLevel;
	uint16_t usPrintDummyLine;
	uint16_t usRearDummyPrintLine;
	uint16_t usRearDeleteLine;

/* Appear unused */
	uint16_t usCancelCheckLinesForPRec;

	uint16_t usPrintSizeLHeight;
	uint16_t usPrintSizeLWidth;

	uint16_t pusSideEdgeLvCoefTable[256];
	uint16_t pusSideEdgeCoefTable[128];
	 int32_t iLeadEdgeCorrectPulse;

	 int32_t m_iMinusMaxEnergy;
	 int32_t m_iPlusMaxEnergy;
	 int32_t m_iPlusMaxEnergyPreRead;

	uint16_t usCenterHeadToColSen;
	uint16_t usThearmEnv;
	uint16_t usThearmHead;
	uint16_t usMatteGloss;
	uint16_t usMatteDeglossBlk;
	uint16_t usMatteDeglossWht;
	uint16_t usPrintOffsetWidth;
	uint16_t usCancelCheckDotsForPRec;
	uint32_t uiOffsetCancelCheckPRec;
	uint32_t uiLevelAveCounter;
	uint32_t uiLevelAveCounter2;
	uint32_t uiLevelAveAddtion;
	uint32_t uiLevelAveAddtion2;
	uint32_t uiDummyPrintCounter;
	uint16_t usRearDummyPrintLevel;
	uint16_t usLastPrintSizeHeight;
	uint16_t usLastPrintSizeWidth;
	uint16_t usLastSheetSizeWidth;

	uint16_t *pusLamiCompInLineBufTab[4];

	uint16_t pusOutLineBuf2[BUF_SIZE];
#endif
};

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
  struct lib6145_ctx *ctx;

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

  ctx = malloc(sizeof(struct lib6145_ctx));
  if (!ctx)
	  return 4;

  memset(ctx, 0, sizeof(struct lib6145_ctx));

  ctx->pucInputImageBuf = in;
  ctx->pusOutputImageBuf = out;
  ctx->pSPrintParam = (struct imageCorrParam *) corrdata;

  i = CheckPrintParam(corrdata);
  if (i)
    return i;

  Global_Init(ctx);
#ifdef S6145_UNUSED
  SetTable(ctx);
#endif

  for ( i = 0; i < 4; i++ ) {   /* Full YMCO */
    int32_t lines;
    SetTableColor(ctx, i);
    LinePrintPreProcess(ctx);
    PagePrintPreProcess(ctx);
    lines = ctx->usPrintSizeHeight;
    while ( lines-- ) {
      PagePrintProcess(ctx);
    }
    ctx->usPrintColor++;
  }

  free(ctx);

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

static void GetInfo(struct lib6145_ctx *ctx)
{
  uint32_t tmp;

#ifdef S6145_UNUSED
  ctx->usLastPrintSizeWidth = ctx->usPrintSizeWidth;
  ctx->usLastPrintSizeHeight = ctx->usPrintSizeHeight;
  ctx->usLastSheetSizeWidth = ctx->usSheetSizeWidth;
  ctx->usPrintOffsetWidth = 0;
#endif

  ctx->usPrintSizeWidth = le16_to_cpu(ctx->pSPrintParam->width);
  ctx->usPrintSizeHeight = le16_to_cpu(ctx->pSPrintParam->height);
  ctx->usSheetSizeWidth = ctx->usPrintSizeWidth;

  ctx->sPrintSideOffset = le16_to_cpu(ctx->pSPrintParam->printSideOffset);

  if ( ctx->pSPrintParam->val_1 )
	  ctx->sCorrectSw |= 1;
  if ( ctx->pSPrintParam->val_2 )
	  ctx->sCorrectSw |= 2;

  ctx->usPrintOpLevel = le16_to_cpu(ctx->pSPrintParam->printOpLevel);

  tmp = le16_to_cpu(ctx->pSPrintParam->randomBase[0]);
  ctx->ucRandomBaseLevel[0] = tmp & 0xff;
  tmp = le16_to_cpu(ctx->pSPrintParam->randomBase[1]);
  ctx->ucRandomBaseLevel[1] = tmp & 0xff;
  tmp = le16_to_cpu(ctx->pSPrintParam->randomBase[2]);
  ctx->ucRandomBaseLevel[2] = tmp & 0xff;
  tmp = le16_to_cpu(ctx->pSPrintParam->randomBase[3]);
  ctx->ucRandomBaseLevel[3] = tmp & 0xff;

  ctx->usMatteSize = le16_to_cpu(ctx->pSPrintParam->matteSize);
  ctx->usMatteMode = le16_to_cpu(ctx->pSPrintParam->matteMode);

#ifdef S6145_UNUSED
  ctx->usMatteGloss = le16_to_cpu(ctx->pSPrintParam->matteGloss);
  ctx->usMatteDeglossBlk = le16_to_cpu(ctx->pSPrintParam->matteDeglossBlk);
  ctx->usMatteDeglossWht = le16_to_cpu(ctx->pSPrintParam->matteDeglossWht);
#endif

  switch (ctx->usPrintColor) {
  case 0:
    ctx->usPrintMaxPulse = le16_to_cpu(ctx->pSPrintParam->printMaxPulse_Y);
    ctx->uiMtfWeightH = le16_to_cpu(ctx->pSPrintParam->mtfWeightH_Y);
    ctx->uiMtfWeightV = le16_to_cpu(ctx->pSPrintParam->mtfWeightV_Y);
    ctx->uiMtfSlice = le16_to_cpu(ctx->pSPrintParam->mtfSlice_Y);
    ctx->usLineCorrect_Env_A = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvA_Y);
    ctx->usLineCorrect_Env_B = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvB_Y);
    ctx->usLineCorrect_Env_C = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvC_Y);
    ctx->uiLineCorrectSlice = le32_to_cpu(ctx->pSPrintParam->lineCorrectSlice_Y);
    ctx->uiLineCorrectSlice1Line = le32_to_cpu(ctx->pSPrintParam->lineCorrectSlice1Line_Y);
    ctx->iLineCorrectPulseMax = le32_to_cpu(ctx->pSPrintParam->lineCorrectPulseMax_Y);
    break;
  case 1:
    ctx->usPrintMaxPulse = le16_to_cpu(ctx->pSPrintParam->printMaxPulse_M);
    ctx->uiMtfWeightH = le16_to_cpu(ctx->pSPrintParam->mtfWeightH_M);
    ctx->uiMtfWeightV = le16_to_cpu(ctx->pSPrintParam->mtfWeightV_M);
    ctx->uiMtfSlice = le16_to_cpu(ctx->pSPrintParam->mtfSlice_M);
    ctx->usLineCorrect_Env_A = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvA_M);
    ctx->usLineCorrect_Env_B = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvB_M);
    ctx->usLineCorrect_Env_C = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvC_M);
    ctx->uiLineCorrectSlice = le32_to_cpu(ctx->pSPrintParam->lineCorrectSlice_M);
    ctx->uiLineCorrectSlice1Line = le32_to_cpu(ctx->pSPrintParam->lineCorrectSlice1Line_M);
    ctx->iLineCorrectPulseMax = le32_to_cpu(ctx->pSPrintParam->lineCorrectPulseMax_M);
    break;
  case 2:
    ctx->usPrintMaxPulse = le16_to_cpu(ctx->pSPrintParam->printMaxPulse_C);
    ctx->uiMtfWeightH = le16_to_cpu(ctx->pSPrintParam->mtfWeightH_C);
    ctx->uiMtfWeightV = le16_to_cpu(ctx->pSPrintParam->mtfWeightV_C);
    ctx->uiMtfSlice = le16_to_cpu(ctx->pSPrintParam->mtfSlice_C);
    ctx->usLineCorrect_Env_A = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvA_C);
    ctx->usLineCorrect_Env_B = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvB_C);
    ctx->usLineCorrect_Env_C = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvC_C);
    ctx->uiLineCorrectSlice = le32_to_cpu(ctx->pSPrintParam->lineCorrectSlice_C);
    ctx->uiLineCorrectSlice1Line = le32_to_cpu(ctx->pSPrintParam->lineCorrectSlice1Line_C);
    ctx->iLineCorrectPulseMax = le32_to_cpu(ctx->pSPrintParam->lineCorrectPulseMax_C);
    break;
  case 3:
    ctx->usPrintMaxPulse = le16_to_cpu(ctx->pSPrintParam->printMaxPulse_O);
    ctx->uiMtfWeightH = le16_to_cpu(ctx->pSPrintParam->mtfWeightH_O);
    ctx->uiMtfWeightV = le16_to_cpu(ctx->pSPrintParam->mtfWeightV_O);
    ctx->uiMtfSlice = le16_to_cpu(ctx->pSPrintParam->mtfSlice_O);;
    ctx->usLineCorrect_Env_A = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvA_O);
    ctx->usLineCorrect_Env_B = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvB_O);
    ctx->usLineCorrect_Env_C = le16_to_cpu(ctx->pSPrintParam->lineCorrectEnvC_O);
    ctx->uiLineCorrectSlice = le32_to_cpu(ctx->pSPrintParam->lineCorrectSlice_O);
    ctx->uiLineCorrectSlice1Line = le32_to_cpu(ctx->pSPrintParam->lineCorrectSlice1Line_O);
    ctx->iLineCorrectPulseMax = le32_to_cpu(ctx->pSPrintParam->lineCorrectPulseMax_O);
    break;
  default:
    printf("ERROR: bad ctx->usPrintColor %d\n", ctx->usPrintColor);
    break;
  }

  ctx->usHeadDots = le16_to_cpu(ctx->pSPrintParam->headDots);
}

static void Global_Init(struct lib6145_ctx *ctx)
{
  ctx->usPrintColor = 0;
  ctx->usPrintSizeWidth = 0;
  ctx->usPrintSizeHeight = 0;
  ctx->usSheetSizeWidth = 0;
  ctx->sPrintSideOffset = 0;
  ctx->sCorrectSw = 0;
  ctx->usPrintOpLevel = 0;
  ctx->uiMtfWeightH = 0;
  ctx->uiMtfWeightV = 0;
  ctx->uiMtfSlice = 0;
  ctx->usPrintMaxPulse = MAX_PULSE;
  ctx->usMatteMode = 0;
  ctx->usLineCorrect_Env_A = 0;
  ctx->usLineCorrect_Env_B = 0;
  ctx->usLineCorrect_Env_C = 0;
  ctx->uiLineCorrectSum = 0;
  ctx->uiLineCorrectBase = 0;
  ctx->uiLineCorrectBase1Line = 0;
  ctx->iLineCorrectPulse = 0;
  ctx->iLineCorrectPulseMax = MAX_PULSE;
  ctx->pulRandomTable[0] = 3;
  ctx->pulRandomTable[1] = -1708027847;
  ctx->pulRandomTable[2] = 853131300;
  ctx->pulRandomTable[3] = -1687801470;
  ctx->pulRandomTable[4] = 1570894658;
  ctx->pulRandomTable[5] = -566525472;
  ctx->pulRandomTable[6] = -552964171;
  ctx->pulRandomTable[7] = -251413502;
  ctx->pulRandomTable[8] = 1223901435;
  ctx->pulRandomTable[9] = 1950999915;
  ctx->pulRandomTable[10] = -1095640144;
  ctx->pulRandomTable[11] = -1420011240;
  ctx->pulRandomTable[12] = -1805298435;
  ctx->pulRandomTable[13] = -1943115761;
  ctx->pulRandomTable[14] = -348292705;
  ctx->pulRandomTable[15] = -1323376457;
  ctx->pulRandomTable[16] = 759393158;
  ctx->pulRandomTable[17] = -630772182;
  ctx->pulRandomTable[18] = 361286280;
  ctx->pulRandomTable[19] = -479628451;
  ctx->pulRandomTable[20] = -1873857033;
  ctx->pulRandomTable[21] = -686452778;
  ctx->pulRandomTable[22] = 1873211473;
  ctx->pulRandomTable[23] = 1634626454;
  ctx->pulRandomTable[24] = -1399525412;
  ctx->pulRandomTable[25] = 910245779;
  ctx->pulRandomTable[26] = -970800488;
  ctx->pulRandomTable[27] = -173790536;
  ctx->pulRandomTable[28] = -1970743429;
  ctx->pulRandomTable[29] = -173171442;
  ctx->pulRandomTable[30] = -1986452981;
  ctx->pulRandomTable[31] = 670779321;
  ctx->uiInputImageIndex = 0;
  ctx->uiOutputImageIndex = 0;
  ctx->usHeadDots = 0;

#ifdef S6145_UNUSED

  ctx->usPrintDummyLevel = 0;
  ctx->usPrintDummyLine = 0;
  ctx->usRearDummyPrintLine = 0;
  ctx->usRearDeleteLine = 0;

  ctx->usPrintSizeLWidth = 0;
  ctx->usPrintSizeLHeight = 0;

  ctx->usThearmHead = 0;
  ctx->usThearmEnv = 0;
  ctx->usRearDummyPrintLevel = 0;

  ctx->usLastPrintSizeWidth = 0;
  ctx->usLastPrintSizeHeight = 0;
  ctx->usLastSheetSizeWidth = 0;

  ctx->iLeadEdgeCorrectPulse = 0;

  ctx->usCancelCheckLinesForPRec = 118;

  ctx->pusLamiCompInLineBufTab[0] = (uint16_t*)ctx->pusInLineBuf0;
  ctx->pusLamiCompInLineBufTab[1] = (uint16_t*)ctx->pusInLineBuf2;
  ctx->pusLamiCompInLineBufTab[2] = ctx->pusOutLineBuf1;
  ctx->pusLamiCompInLineBufTab[3] = ctx->pusOutLineBuf2;

  ctx->usMatteGloss = 105;
  ctx->usMatteDeglossBlk = 150;
  ctx->usMatteDeglossWht = 175;

  ctx->usPrintOffsetWidth = 0;
  ctx->usCenterHeadToColSen = 268;

  ctx->uiLevelAveAddtion = 0;
  ctx->uiLevelAveCounter = 0;
  ctx->uiLevelAveCounter2 = 0;
  ctx->uiLevelAveAddtion2 = 0;
  ctx->usCancelCheckDotsForPRec = 236;
#endif
}

#ifdef S6145_UNUSED
static void SetTable(struct lib6145_ctx *ctx)
{
  SetTableData(ctx->pSPrintParam->SideEdgeCoefTable, ctx->pusSideEdgeCoefTable, 128);
  SetTableData(ctx->pSPrintParam->SideEdgeLvCoefTable, ctx->pusSideEdgeLvCoefTable, 256);
}
#endif

static void SetTableColor(struct lib6145_ctx *ctx, uint8_t plane)
{
  switch (plane) {
  case 0:
    SetTableData(ctx->pSPrintParam->pulseTransTable_Y, ctx->pusPulseTransTable, 256);
    SetTableData(ctx->pSPrintParam->lineHistCoefTable_Y, ctx->pusLineHistCoefTable, 256);
    SetTableData(ctx->pSPrintParam->tankPlusMaxEnergyTable_Y, ctx->pusTankPlusMaxEnegyTable, 256);
    SetTableData(ctx->pSPrintParam->tankMinusMaxEnergy_Y, ctx->pusTankMinusMaxEnegyTable, 256);
    memcpy(ctx->piTankParam, &ctx->pSPrintParam->tableTankParam_Y, 128);
    break;
  case 1:
    SetTableData(ctx->pSPrintParam->pulseTransTable_M, ctx->pusPulseTransTable, 256);
    SetTableData(ctx->pSPrintParam->lineHistCoefTable_M, ctx->pusLineHistCoefTable, 256);
    SetTableData(ctx->pSPrintParam->tankPlusMaxEnergyTable_M, ctx->pusTankPlusMaxEnegyTable, 256);
    SetTableData(ctx->pSPrintParam->tankMinusMaxEnergy_M, ctx->pusTankMinusMaxEnegyTable, 256);
    memcpy(&ctx->piTankParam[32], &ctx->pSPrintParam->tableTankParam_M, 128);
    break;
  case 2:
    SetTableData(ctx->pSPrintParam->pulseTransTable_C, ctx->pusPulseTransTable, 256);
    SetTableData(ctx->pSPrintParam->lineHistCoefTable_C, ctx->pusLineHistCoefTable, 256);
    SetTableData(ctx->pSPrintParam->tankPlusMaxEnergyTable_C, ctx->pusTankPlusMaxEnegyTable, 256);
    SetTableData(ctx->pSPrintParam->tankMinusMaxEnergy_C, ctx->pusTankMinusMaxEnegyTable, 256);
    memcpy(&ctx->piTankParam[64], &ctx->pSPrintParam->tableTankParam_C, 128);
    break;
  case 3:
    SetTableData(ctx->pSPrintParam->pulseTransTable_O, ctx->pusPulseTransTable, 256);
    SetTableData(ctx->pSPrintParam->lineHistCoefTable_O, ctx->pusLineHistCoefTable, 256);
    SetTableData(ctx->pSPrintParam->tankPlusMaxEnergyTable_O, ctx->pusTankPlusMaxEnegyTable, 256);
    SetTableData(ctx->pSPrintParam->tankMinusMaxEnergy_O, ctx->pusTankMinusMaxEnegyTable, 256);
    memcpy(&ctx->piTankParam[96], &ctx->pSPrintParam->tableTankParam_O, 128);
    break;
  default:
    printf("ERROR: Bad plane in SetTableColor (%d)\n", plane);
    break;
  }
}

static int32_t CheckPrintParam(struct imageCorrParam *corrdata)
{
  int i;

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
static void LinePrintPreProcess(struct lib6145_ctx *ctx)
{
  int16_t i;

  GetInfo(ctx);

  if ( !(ctx->sCorrectSw & 1) )
  {
    ctx->uiMtfWeightH = 0;
    ctx->uiMtfWeightV = 0;
    ctx->uiMtfSlice = 0;
  }

  for ( i = -256; i < 256; i++ )
  {
    if ( (uint32_t)(i * i) >= (uint32_t)(ctx->uiMtfSlice * ctx->uiMtfSlice) )
	    ctx->psMtfPreCalcTable[i+256] = i;
    else
	    ctx->psMtfPreCalcTable[i+256] = -i;
  }

  ctx->pusPreReadLineBufTab[0] = ctx->pusInLineBuf0;
  ctx->pusPreReadLineBufTab[1] = ctx->pusInLineBuf1;
  ctx->pusPreReadLineBufTab[2] = ctx->pusInLineBuf2;
  ctx->pusPreReadLineBufTab[3] = ctx->pusInLineBuf3;
  ctx->pusPreReadLineBufTab[4] = ctx->pusInLineBuf4;
  ctx->pusPreReadLineBufTab[5] = ctx->pusInLineBuf5;
  ctx->pusPreReadLineBufTab[6] = ctx->pusInLineBuf6;
  ctx->pusPreReadLineBufTab[7] = ctx->pusInLineBuf7;
  ctx->pusPreReadLineBufTab[8] = ctx->pusInLineBuf8;
  ctx->pusPreReadLineBufTab[9] = ctx->pusInLineBuf9;
  ctx->pusPreReadLineBufTab[10] = ctx->pusInLineBufA;

  memset(ctx->pusInLineBuf0, 0, sizeof(ctx->pusInLineBuf0));
  memset(ctx->pusInLineBuf1, 0, sizeof(ctx->pusInLineBuf1));
  memset(ctx->pusInLineBuf2, 0, sizeof(ctx->pusInLineBuf2));
  memset(ctx->pusInLineBuf3, 0, sizeof(ctx->pusInLineBuf3));
  memset(ctx->pusInLineBuf4, 0, sizeof(ctx->pusInLineBuf4));
  memset(ctx->pusInLineBuf5, 0, sizeof(ctx->pusInLineBuf5));
  memset(ctx->pusInLineBuf6, 0, sizeof(ctx->pusInLineBuf6));
  memset(ctx->pusInLineBuf7, 0, sizeof(ctx->pusInLineBuf7));
  memset(ctx->pusInLineBuf8, 0, sizeof(ctx->pusInLineBuf8));
  memset(ctx->pusInLineBuf9, 0, sizeof(ctx->pusInLineBuf9));
  memset(ctx->pusInLineBufA, 0, sizeof(ctx->pusInLineBufA));

  ctx->pusPulseTransLineBufTab[0] = ctx->pusInLineBuf0;
  ctx->pusPulseTransLineBufTab[1] = ctx->pusInLineBuf1;
  ctx->pusPulseTransLineBufTab[2] = ctx->pusInLineBuf2;
  ctx->pusPulseTransLineBufTab[3] = ctx->pusInLineBuf3;

  memset(ctx->pusOutLineBuf1, 0, sizeof(ctx->pusOutLineBuf1));
  ctx->pusOutLineBufTab[0] = ctx->pusOutLineBuf1;

#ifdef S6145_UNUSED
  memset(ctx->pusInLineBuf3, ctx->usPrintDummyLevel, sizeof(ctx->pusInLineBuf3)); // XXX redundant with memset above, printDummyLevel is always 0 anyway.
#endif

  ctx->uiSendToHeadCounter = ctx->usPrintSizeHeight;
  ctx->uiLineCopyCounter = ctx->usPrintSizeHeight;

#ifdef S6145_UNUSED
  ctx->uiDataTransCounter = ctx->usPrintSizeHeight;
  ctx->uiDataTransCounter += ctx->usPrintDummyLine;
  ctx->uiDataTransCounter -= ctx->usRearDeleteLine;
  ctx->uiDataTransCounter += ctx->usRearDummyPrintLine;

  ctx->uiSendToHeadCounter += ctx->usPrintDummyLine;
  ctx->uiSendToHeadCounter -= ctx->usRearDeleteLine;
  ctx->uiSendToHeadCounter += ctx->usRearDummyPrintLine;

  ctx->uiTudenLineCounter = ctx->usPrintSizeHeight;
  ctx->uiTudenLineCounter += ctx->usRearDummyPrintLine;
  ctx->uiTudenLineCounter -= ctx->usRearDeleteLine;

  ctx->uiLineCopyCounter -= ctx->usRearDeleteLine;

  if ( ctx->usPrintColor != 3 )
    ctx->usRearDummyPrintLevel = 255;

  ctx->iLeadEdgeCorrectPulse = 0;
#endif

  switch (ctx->usPrintColor) {
  case 0:
    CTankResetParameter(ctx, &ctx->piTankParam[0]);
    ctx->iMaxPulseValue = ctx->usPrintMaxPulse;
    ctx->uiMaxPulseBit = LinePrintCalcBit(ctx->usPrintMaxPulse);
    ctx->pfRecieveData = RecieveDataYMC;
#ifdef S6145_UNUSED
    ctx->pfRecieveData_Post = RecieveDataYMC_Post;
#endif
    ctx->pfPulseTransPreRead = PulseTransPreReadYMC;
    ctx->pfTankProcessPreRead = CTankProcessPreRead;
    break;
  case 1:
    CTankResetParameter(ctx, &ctx->piTankParam[32]);
    ctx->iMaxPulseValue = ctx->usPrintMaxPulse;
    ctx->uiMaxPulseBit = LinePrintCalcBit(ctx->usPrintMaxPulse);
    ctx->pfRecieveData = RecieveDataYMC;
#ifdef S6145_UNUSED
    ctx->pfRecieveData_Post = RecieveDataYMC_Post;
#endif
    ctx->pfPulseTransPreRead = PulseTransPreReadYMC;
    ctx->pfTankProcessPreRead = CTankProcessPreRead;
    break;
  case 2:
    CTankResetParameter(ctx, &ctx->piTankParam[64]);
    ctx->iMaxPulseValue = ctx->usPrintMaxPulse;
    ctx->uiMaxPulseBit = LinePrintCalcBit(ctx->usPrintMaxPulse);
    ctx->pfRecieveData = RecieveDataYMC;
#ifdef S6145_UNUSED
    ctx->pfRecieveData_Post = RecieveDataYMC_Post;
#endif
    ctx->pfPulseTransPreRead = PulseTransPreReadYMC;
    ctx->pfTankProcessPreRead = CTankProcessPreRead;
    break;
  case 3:
    CTankResetParameter(ctx, &ctx->piTankParam[96]);
    ctx->iMaxPulseValue = ctx->usPrintMaxPulse;
    ctx->uiMaxPulseBit = LinePrintCalcBit(ctx->usPrintMaxPulse);
    if ( ctx->usMatteMode ) {
      ctx->pfRecieveData = RecieveDataOP_MATTE;
#ifdef S6145_UNUSED
      ctx->pfRecieveData_Post = RecieveDataOPMatte_Post;
#endif
    } else {
      ctx->pfRecieveData = RecieveDataOP_GLOSS;
#ifdef S6145_UNUSED
      ctx->pfRecieveData_Post = RecieveDataOPLevel_Post;
#endif
    }
    ctx->pfPulseTransPreRead = PulseTransPreReadOP;
    ctx->pfTankProcessPreRead = CTankProcessPreReadDummy;
#ifdef S6145_UNUSED
    if ( ctx->usMatteMode )
      ctx->iLeadEdgeCorrectPulse = 120;
#endif
    break;
  default:
    printf("ERROR: Bad ctx->usPrintColor %d\n", ctx->usPrintColor);
    return;
  }

  ctx->uiLineCorrectSum = 0;
  ctx->iLineCorrectPulse = 0;

  if ( ctx->uiLineCorrectSlice ) {
    ctx->uiLineCorrectBase = ctx->uiLineCorrectSlice * ctx->usLineCorrect_Env_A;
    ctx->uiLineCorrectBase >>= 15;
    ctx->uiLineCorrectBase *= ctx->usSheetSizeWidth;
  } else {
    ctx->uiLineCorrectBase = -1;
  }

  if ( ctx->uiLineCorrectSlice1Line ) {
    ctx->uiLineCorrectBase1Line = ctx->uiLineCorrectSlice1Line * ctx->usLineCorrect_Env_B;
    ctx->uiLineCorrectBase1Line >>= 15;
    ctx->uiLineCorrectBase1Line *= ctx->usSheetSizeWidth;
  }

  if ( ctx->iLineCorrectPulseMax ) {
    ctx->iLineCorrectPulseMax *= ctx->usLineCorrect_Env_C;
    ctx->iLineCorrectPulseMax /= 1024;
  } else {
    ctx->iLineCorrectPulseMax = MAX_PULSE;
  }

  CTankResetTank(ctx);

#ifdef S6145_UNUSED
  ctx->uiDummyPrintCounter = 0;
#endif
}

static void CTankResetParameter(struct lib6145_ctx *ctx, int32_t *params)
{
  ctx->m_iTrdTankSize = le32_to_cpu(params[0]);
  ctx->m_iSndTankSize = le32_to_cpu(params[1]);
  ctx->m_iFstTankSize = le32_to_cpu(params[2]);
  ctx->m_iTrdTankIniEnergy = le32_to_cpu(params[3]);
  ctx->m_iSndTankIniEnergy = le32_to_cpu(params[4]);
  ctx->m_iFstTankIniEnergy = le32_to_cpu(params[5]);
  ctx->m_iTrdTrdConductivity = le32_to_cpu(params[6]);
  ctx->m_iSndSndConductivity = le32_to_cpu(params[7]);
  ctx->m_iFstFstConductivity = le32_to_cpu(params[8]);
  ctx->m_iOutTrdConductivity = le32_to_cpu(params[9]);
  ctx->m_iTrdSndConductivity = le32_to_cpu(params[10]);
  ctx->m_iSndFstConductivity = le32_to_cpu(params[11]);
  ctx->m_iFstOutConductivity = le32_to_cpu(params[12]);
#ifdef S6145_UNUSED
  ctx->m_iPlusMaxEnergy = le32_to_cpu(params[13]);
  ctx->m_iMinusMaxEnergy = le32_to_cpu(params[14]);
  ctx->m_iPlusMaxEnergyPreRead = le32_to_cpu(params[15]);
#endif

  ctx->m_iMinusMaxEnergyPreRead = le32_to_cpu(params[16]);
  ctx->m_iPreReadLevelDiff = le32_to_cpu(params[17]);

  ctx->m_iTankKeisuOutTrdDivTrd = (int64_t)ctx->m_iOutTrdConductivity * (int64_t)0x10000 / (int64_t)ctx->m_iTrdTankSize;
  ctx->m_iTankKeisuTrdSndDivTrd = (int64_t)ctx->m_iTrdSndConductivity * (int64_t)0x10000 / (int64_t)ctx->m_iTrdTankSize;
  ctx->m_iTankKeisuTrdSndDivSnd = (int64_t)ctx->m_iTrdSndConductivity * (int64_t)0x10000 / (int64_t)ctx->m_iSndTankSize;
  ctx->m_iTankKeisuSndFstDivSnd = (int64_t)ctx->m_iSndFstConductivity * (int64_t)0x10000 / (int64_t)ctx->m_iSndTankSize;
  ctx->m_iTankKeisuSndFstDivFst = (int64_t)ctx->m_iSndFstConductivity * (int64_t)0x10000 / (int64_t)ctx->m_iFstTankSize;
  ctx->m_iTankKeisuFstOutDivFst = (int64_t)ctx->m_iFstOutConductivity * (int64_t)0x10000 / (int64_t)ctx->m_iFstTankSize;

  return;
}

static void CTankResetTank(struct lib6145_ctx *ctx)
{
  int i;

  for (i = 0 ; i < TANK_SIZE; i++) {
    ctx->m_piTrdTankArray[i] = ctx->m_iTrdTankIniEnergy;
    ctx->m_piSndTankArray[i] = ctx->m_iSndTankIniEnergy;
    ctx->m_piFstTankArray[i] = ctx->m_iFstTankIniEnergy;
  }
}

/* This primes the preprocessing pipeline prior to starting the first
   actual row of image data */
static void PagePrintPreProcess(struct lib6145_ctx *ctx)
{
  uint32_t i;

  ctx->pusPulseTransLineBufTab[3] = ctx->pusPreReadLineBufTab[1];
  ctx->pfRecieveData(ctx);
  ctx->pusPulseTransLineBufTab[1] = ctx->pusPulseTransLineBufTab[3];
  ctx->uiLineCopyCounter++;
  ctx->uiInputImageIndex -= ctx->usPrintSizeWidth;
  ctx->pusPulseTransLineBufTab[3] = ctx->pusPreReadLineBufTab[2];
  ctx->pfRecieveData(ctx);
  ctx->pusPulseTransLineBufTab[2] = ctx->pusPulseTransLineBufTab[3];
  ctx->pusPulseTransLineBufTab[3] = ctx->pusPreReadLineBufTab[3];
  ctx->pfRecieveData(ctx);
  for ( i = 0; i < 7; i++ )
  {
    ctx->pusPulseTransLineBufTab[3] = ctx->pusPreReadLineBufTab[i + 4];
    ctx->pfRecieveData(ctx);
  }
  ctx->pusPulseTransLineBufTab[0] = ctx->pusPreReadLineBufTab[0];
}

/* Process a single scanline,
   From reading the input data to writing the output.
 */
static void PagePrintProcess(struct lib6145_ctx *ctx)
{
  uint32_t i;

  /* First, rotate the input buffers... */
  if ( ctx->usPrintColor != 3 || ctx->usMatteMode != 1 || ctx->usMatteSize != 2 ) {
    /* If we're not printing a matte layer... */
    uint8_t *v4 = ctx->pusPreReadLineBufTab[0];
    for ( i = 0; i < 10; i++ )
      ctx->pusPreReadLineBufTab[i] = ctx->pusPreReadLineBufTab[i + 1];
    ctx->pusPreReadLineBufTab[10] = v4;
    ctx->pusPulseTransLineBufTab[0] = ctx->pusPreReadLineBufTab[0];
    ctx->pusPulseTransLineBufTab[1] = ctx->pusPreReadLineBufTab[1];
    ctx->pusPulseTransLineBufTab[2] = ctx->pusPreReadLineBufTab[2];
    ctx->pusPulseTransLineBufTab[3] = ctx->pusPreReadLineBufTab[10];
  } else if ( ctx->uiLineCopyCounter & 1 ) {
    /* in other words, every other line when printing a matte layer..  */
    uint8_t *v4 = ctx->pusPreReadLineBufTab[0];
    for ( i = 0; i < 10; i++ )
      ctx->pusPreReadLineBufTab[i] = ctx->pusPreReadLineBufTab[i + 1];
    ctx->pusPreReadLineBufTab[10] = v4;
    ctx->pusPulseTransLineBufTab[0] = ctx->pusPreReadLineBufTab[0];
    ctx->pusPulseTransLineBufTab[1] = ctx->pusPreReadLineBufTab[1];
    ctx->pusPulseTransLineBufTab[2] = ctx->pusPreReadLineBufTab[2];
    ctx->pusPulseTransLineBufTab[3] = ctx->pusPreReadLineBufTab[10];
  }

#ifdef S6145_UNUSED
  ctx->uiTudenLineCounter--;
#endif
  ctx->pfRecieveData(ctx); /* Read another scanline */
  PulseTrans(ctx);
  ctx->pfPulseTransPreRead(ctx);
#ifdef S6145_UNUSED
  ctx->pfRecieveData_Post();  /* Clean up after the receive */
#endif
  CTankProcess(ctx);  /* Update thermal tank state */
  ctx->pfTankProcessPreRead(ctx);
  LineCorrection(ctx); /* Final output compensation */
  SendData(ctx);      /* Write scanline output */
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
static void CTankProcess(struct lib6145_ctx *ctx)
{
  if ( ctx->sCorrectSw & 2 ) {
    CTankHosei(ctx);
    CTankUpdateTankVolumeInterRay(ctx);
    CTankUpdateTankVolumeInterDot(ctx, 0);
    CTankUpdateTankVolumeInterDot(ctx, 1);
    CTankUpdateTankVolumeInterDot(ctx, 2);
  }
  return;
}

static void CTankProcessPreRead(struct lib6145_ctx *ctx)
{
  if (ctx->sCorrectSw & 2)
     CTankHoseiPreread(ctx);
}

static void CTankProcessPreReadDummy(struct lib6145_ctx *ctx)
{
  (void)ctx;
  return;
}

/* This will generate one line worth of "gloss" OC data.
   It only covers the imageable area, rather than the head width */
static void RecieveDataOP_GLOSS(struct lib6145_ctx *ctx)
{
  if ( ctx->uiLineCopyCounter ) {
     memset(ctx->pusPulseTransLineBufTab[3] + ((ctx->usHeadDots - ctx->usSheetSizeWidth) / 2),
	    ctx->usPrintOpLevel,
	    ctx->usSheetSizeWidth);

    ctx->uiLineCopyCounter--;
  }

  return;
}

/* This reads a single line worth of input image data.
 */
static void RecieveDataYMC(struct lib6145_ctx *ctx)
{
  uint8_t *v1;
  int16_t i;

  v1 = ctx->pusPulseTransLineBufTab[3] + ((ctx->usHeadDots - ctx->usSheetSizeWidth) / 2);

  if ( ctx->uiLineCopyCounter ) {
     /* Read the next line */
    for ( i = 0; i < ctx->usPrintSizeWidth; i++ )
      v1[i] = ctx->pucInputImageBuf[ctx->uiInputImageIndex++];
    --ctx->uiLineCopyCounter;
  } else {
    /* Re-read the previous line */
    ctx->uiInputImageIndex -= ctx->usPrintSizeWidth;
    for ( i = 0; i < ctx->usPrintSizeWidth ; i++ )
      v1[i] = ctx->pucInputImageBuf[ctx->uiInputImageIndex++];
  }
}

/* this will generate one scanline (ie 16b * BUF_SIZE) worth of
   "random" data for the matte overcoat */
static void RecieveDataOP_MATTE(struct lib6145_ctx *ctx)
{
  if ( ctx->uiLineCopyCounter ) {
    int32_t v1;
    uint32_t v5;
    int32_t v6;

    int16_t matteCtr;
    uint8_t *outPtr = ctx->pusPulseTransLineBufTab[3];

    if ( ctx->usMatteSize == 2 )
      matteCtr = 256;
    else
      matteCtr = 512;

    while ( matteCtr-- ) {
      if ( ctx->pulRandomTable[0] >= 31 )
        v6 = 1;
      else
        v6 = ctx->pulRandomTable[0] + 1;
      ctx->pulRandomTable[0] = v6;
      if ( v6 <= 3 )
        v1 = ctx->pulRandomTable[v6 + 28];
      else
        v1 = ctx->pulRandomTable[v6 - 3];
      ctx->pulRandomTable[v6] += v1;

      v5 = (uint32_t)ctx->pulRandomTable[v6] >> 1;
      if ( ctx->usMatteSize == 2 ) {
	*outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 1) & 3];
        *outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 1) & 3];
        *outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 5) & 3];
        *outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 5) & 3];
        *outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 9) & 3];
        *outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 9) & 3];
        *outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 13) & 3];
        *outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 13) & 3];
      } else {
	*outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 1) & 3];
        *outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 5) & 3];
        *outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 9) & 3];
        *outPtr++ = ctx->ucRandomBaseLevel[(v5 >> 13) & 3];
      }
    }
    --ctx->uiLineCopyCounter;
  }
}

/* This writes a single scanline to the output buffer */
static void SendData(struct lib6145_ctx *ctx)
{
  uint16_t i;

  if ( ctx->uiSendToHeadCounter ) {
    for ( i = 0; i < ctx->usHeadDots; i++ )
      ctx->pusOutputImageBuf[ctx->uiOutputImageIndex++] = cpu_to_le16(ctx->pusOutLineBufTab[0][i]);
    --ctx->uiSendToHeadCounter;
  }
}

/* Use the previous two rows to generate the needed impulse for
   the current row. */
static void PulseTrans(struct lib6145_ctx *ctx)
{
  int32_t overHang;
  int32_t sheetSizeWidth;

  uint8_t *prevPrevRow;
  uint8_t *prevRow;
  uint8_t *currentRow;

  uint16_t *out;

  sheetSizeWidth = ctx->usSheetSizeWidth;
  overHang = (ctx->usHeadDots - ctx->usSheetSizeWidth) / 2;

  currentRow = ctx->pusPulseTransLineBufTab[0] + overHang;
  prevRow = ctx->pusPulseTransLineBufTab[1] + overHang;
  prevPrevRow = ctx->pusPulseTransLineBufTab[2] + overHang;
  out = ctx->pusOutLineBufTab[0] + ctx->sPrintSideOffset + overHang;

  if ( out >= ctx->pusOutLineBufTab[0] ) {
    int32_t offset = ctx->sPrintSideOffset
        + ctx->usSheetSizeWidth
        + overHang;
    if ( offset > BUF_SIZE )
      sheetSizeWidth = ctx->usSheetSizeWidth - (offset - BUF_SIZE);
  } else {
    int32_t offset = (ctx->pusOutLineBufTab[0] - out);
    out = ctx->pusOutLineBufTab[0];
    sheetSizeWidth = ctx->usSheetSizeWidth - offset;
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
    v4 = ctx->psMtfPreCalcTable[256 + v12 - *prevRow] + ctx->psMtfPreCalcTable[256 + v12 - v3];
    v6 = ctx->psMtfPreCalcTable[256 + v12 - *currentRow++];

    tableOffset = v12 + ((v4 * ctx->uiMtfWeightH + (ctx->psMtfPreCalcTable[256 + v12 - *prevPrevRow++] + v6) * ctx->uiMtfWeightV) >> 7);
    if ( tableOffset > 255 )
      tableOffset = 255;
    if ( tableOffset <= 0 )
      tableOffset = 1;
    if ( !v12 )
      tableOffset = 0;

    compVal = ctx->pusPulseTransTable[tableOffset];
    if ( compVal > MAX_PULSE )
      compVal = MAX_PULSE;

    *out++ = compVal;
  }

#ifdef S6145_UNUSED
  ctx->uiDataTransCounter--;
#endif
}

static void PulseTransPreReadOP(struct lib6145_ctx *ctx)
{
  (void)ctx;
}

static void PulseTransPreReadYMC(struct lib6145_ctx *ctx)
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

  printSizeWidth = ctx->usPrintSizeWidth;
  overHang = (ctx->usHeadDots - ctx->usPrintSizeWidth) / 2;
  v17 = ctx->pusPreReadLineBufTab[2] + overHang;
  v16 = ctx->pusPreReadLineBufTab[3] + overHang;
  v15 = ctx->pusPreReadLineBufTab[4] + overHang;
  v14 = ctx->pusPreReadLineBufTab[5] + overHang;

#ifdef S6145_UNUSED
  v1 = ctx->pusPreReadLineBufTab[6] + overHang;
  v2 = ctx->pusPreReadLineBufTab[7] + overHang;
  v3 = ctx->pusPreReadLineBufTab[8] + overHang;
  v4 = ctx->pusPreReadLineBufTab[9] + overHang;
#endif

  out = ctx->pusPreReadOutLineBuf + overHang + ctx->sPrintSideOffset;

  if ( out < ctx->pusPreReadOutLineBuf ) {
    int32_t offset = (ctx->pusPreReadOutLineBuf - out);
    out = ctx->pusPreReadOutLineBuf;
    printSizeWidth = ctx->usPrintSizeWidth - offset;
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
    int32_t pixel = ctx->pusPulseTransTable[v9 / 4];
    if ( pixel > MAX_PULSE )
      pixel = MAX_PULSE;
    *out++ = pixel;
  }
}

static void CTankUpdateTankVolumeInterDot(struct lib6145_ctx *ctx, uint8_t tank)
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
    tankIn = ctx->m_piFstTankArray;
    tankOut = ctx->m_piFstTankArray + 2;
    conductivity = ctx->m_iFstFstConductivity / 2;
    break;
  case 1:
    tankIn = ctx->m_piSndTankArray;
    tankOut = ctx->m_piSndTankArray + 2;
    conductivity = ctx->m_iSndSndConductivity / 2;
    break;
  case 2:
    tankIn = ctx->m_piTrdTankArray;
    tankOut = ctx->m_piTrdTankArray + 2;
    conductivity = ctx->m_iTrdTrdConductivity / 2;
    break;
  default:
    printf("ERROR: Bad Tank %d in CTankUpdateVolumeInterDot\n", tank);
    return;
  }

  /* This code basically takes a running average of three running
     averages, and uses that as the basis for the output */

  tankIn[0] = tankIn[1] = tankIn[2];
  v1 = ctx->usSheetSizeWidth + 1;
  tankIn[v1+1] = tankIn[v1+2] = tankIn[v1];
  v2 = *tankIn++;
  v4 = *tankIn++;
  v5 = *tankIn++;
  v8 = *tankIn++;
  v20 = *tankIn++;

  v19 = conductivity * (v5 + v2  - 2 * v4);
  v18 = conductivity * (v8 + v4  - 2 * v5);
  v17 = conductivity * (v20 + v5 - 2 * v8);
  sheetSizeWidth = ctx->usSheetSizeWidth;

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

static void CTankUpdateTankVolumeInterRay(struct lib6145_ctx *ctx)
{
  uint16_t sheetWidth = ctx->usSheetSizeWidth;
  int32_t *fstTankPtr = ctx->m_piFstTankArray + 2;
  int32_t *sndTankPtr = ctx->m_piSndTankArray + 2;
  int32_t *trdTankPtr = ctx->m_piTrdTankArray + 2;

  while ( sheetWidth-- ) {
    int32_t v2, v3;

    v2 = (*sndTankPtr * ctx->m_iTankKeisuSndFstDivSnd - *fstTankPtr * ctx->m_iTankKeisuSndFstDivFst) >> 17;
    *fstTankPtr = v2 + *fstTankPtr - (*fstTankPtr * ctx->m_iTankKeisuFstOutDivFst >> 17);
    fstTankPtr++;

    v3 = (*trdTankPtr * ctx->m_iTankKeisuTrdSndDivTrd - *sndTankPtr * ctx->m_iTankKeisuTrdSndDivSnd) >> 17;
    *sndTankPtr = v3 + *sndTankPtr - v2;
    sndTankPtr++;

    *trdTankPtr = *trdTankPtr - v3 - (*trdTankPtr * ctx->m_iTankKeisuOutTrdDivTrd >> 17);
    trdTankPtr++;
  }
}

static void CTankHoseiPreread(struct lib6145_ctx *ctx)
{
  uint16_t sheetWidth;
  int16_t overHang;
  int32_t *fstTankPtr;
  uint16_t *outPtr;
  int16_t *inPtr;  /* Must treat this as SIGNED! */

  int32_t v4;

  overHang = (ctx->usHeadDots - ctx->usSheetSizeWidth) / 2;
  inPtr = (int16_t*)ctx->pusPreReadOutLineBuf + overHang + ctx->sPrintSideOffset;
  outPtr = ctx->pusOutLineBufTab[0] + overHang + ctx->sPrintSideOffset;
  if ( outPtr < ctx->pusOutLineBufTab[0] )
    outPtr = ctx->pusOutLineBufTab[0];
  fstTankPtr = ctx->m_piFstTankArray + 2;
  v4 = (1 << (ctx->uiMaxPulseBit + 20)) / ctx->m_iFstTankSize;

  /* Walk forward through the line to compute the necessary delta */
  sheetWidth = ctx->usSheetSizeWidth;
  while ( sheetWidth-- ) {
    int32_t v5 = *inPtr - (v4 * (*inPtr + *fstTankPtr++) >> 20);
    int32_t v6 = 0;
    if ( v5 < ctx->m_iPreReadLevelDiff )
      v6 = -(ctx->m_iMinusMaxEnergyPreRead * v5 * v5) >> ctx->uiMaxPulseBit;
    *inPtr++ = v6;
  }

  /* Now walk backwards through the line to derive the desired pixel
     values, adding the actual value with the necessary delta.. */
  outPtr += ctx->usSheetSizeWidth;

  sheetWidth = ctx->usSheetSizeWidth;
  while ( sheetWidth-- ) {
    int32_t pixel;

    inPtr--;
    outPtr--;

    pixel = *inPtr + *outPtr;
    if ( pixel < 0 )
      pixel = 0;
    if ( pixel > ctx->iMaxPulseValue )
      pixel = ctx->iMaxPulseValue;
    *outPtr = pixel;
  }
}

/* Apply the correction needed based on the thermal tanks */
static void CTankHosei(struct lib6145_ctx *ctx)
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

  sheetSizeWidth = ctx->usSheetSizeWidth;
  overHang = (ctx->usHeadDots - ctx->usSheetSizeWidth) / 2;
  out = ctx->pusOutLineBufTab[0] + (overHang + ctx->sPrintSideOffset);
  in = ctx->pusPulseTransLineBufTab[1] + overHang;

  if ( out >= ctx->pusOutLineBufTab[0] ) {
    int32_t offset = ctx->sPrintSideOffset + sheetSizeWidth + overHang;
    if ( offset > BUF_SIZE ) {
      offset -= BUF_SIZE;
      sheetSizeWidth -= offset;
    }
  } else {
    int32_t offset = (ctx->pusOutLineBufTab[0] - out);
    sheetSizeWidth -= offset;
    in += (out - ctx->pusOutLineBufTab[0]); // XXX was: in += out;
    out = ctx->pusOutLineBufTab[0];
    printf("WARN: CTankHosei() alt path\n");
  }
  tankPtr = ctx->m_piFstTankArray + 2;

#ifdef S6145_UNUSED
  v2 = ctx->m_iPlusMaxEnergy;
  v12 = &v2;
#endif

  v4 = (1 << (ctx->uiMaxPulseBit + 20)) / ctx->m_iFstTankSize;

  while ( sheetSizeWidth-- ) {
    int32_t v5;
    int32_t v8;
    uint16_t v11;
    uint32_t v3 = *in++;
    v5 = *out - ((v4 * (*out + *tankPtr)) >> 20);
    if ( v5 < 0 )
      v11 = ctx->pusTankMinusMaxEnegyTable[v3];
    else
      v11 = ctx->pusTankPlusMaxEnegyTable[v3];
    v8 = *out + ((v5 * v11) >> ctx->uiMaxPulseBit);
    if ( v8 < 0 )
      v8 = 0;
    if ( v8 > ctx->iMaxPulseValue )
      v8 = ctx->iMaxPulseValue;
    *out++ = v8;
    *tankPtr++ += v8;
  }
}

/* Apply final corrections to the output. */
#define LINECORR_BUCKETS 4
static void LineCorrection(struct lib6145_ctx *ctx)
{
  uint16_t sheetSizeWidth;
  uint16_t overHang;
  uint8_t *in;
  uint16_t *out;
  uint32_t bucket[LINECORR_BUCKETS];
  uint32_t correct;
  uint8_t i;

  sheetSizeWidth = ctx->usSheetSizeWidth;
  overHang = (ctx->usHeadDots - ctx->usSheetSizeWidth) / 2;
  in = ctx->pusPulseTransLineBufTab[1] + overHang;
  out = ctx->pusOutLineBufTab[0] + overHang + ctx->sPrintSideOffset;
  if ( out >= ctx->pusOutLineBufTab[0] ) {
    uint32_t tmp = ctx->sPrintSideOffset + sheetSizeWidth + overHang;
    if ( tmp > BUF_SIZE ) {
      tmp -= BUF_SIZE;
      sheetSizeWidth -= tmp;
    }
  } else {
    uint32_t tmp = ctx->pusOutLineBufTab[0] - out;
    sheetSizeWidth -= tmp;
    in += (out - ctx->pusOutLineBufTab[0]); // XXX was: in += out;
    out = ctx->pusOutLineBufTab[0];
    printf("WARN: LineCorrection() alt path\n");
  }

  /* Apply the correction compensation */
  bucket[0] = bucket[1] = bucket[2] = bucket[3] = 0;
  for ( i = 0; i < LINECORR_BUCKETS; i++ ) {
    uint16_t j = sheetSizeWidth / LINECORR_BUCKETS;
    while ( j-- ) {
      int32_t pixel = *out;
      bucket[i] += pixel;
      pixel -= ctx->pusLineHistCoefTable[*in++] * ctx->iLineCorrectPulse / 1024;
      if ( pixel < 0 )
        pixel = 0;
      *out++ = pixel;
    }
  }

  /* See if we need to increase the correction compensation */
  correct = 0;
  for ( i = 0; i < LINECORR_BUCKETS; i++ ) {
    if ( ctx->uiLineCorrectBase1Line / LINECORR_BUCKETS <= bucket[i] )
      correct++;
  }
  if ( correct ) {
    for ( i = 0; i < LINECORR_BUCKETS; i++ )
      ctx->uiLineCorrectSum += bucket[i];
  }
  if ( ctx->uiLineCorrectSum > ctx->uiLineCorrectBase ) {
    ctx->uiLineCorrectSum -= ctx->uiLineCorrectBase;
    if ( ctx->iLineCorrectPulse < ctx->iLineCorrectPulseMax )
      ctx->iLineCorrectPulse++;
  }

}

#ifdef S6145_UNUSED
/* XXX all of these functions are present in the library, but not actually
   referenced by anything, so there's no point in worrying about them. */
static void SideEdgeCorrection(struct lib6145_ctx *ctx)
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

  v16 = ctx->usSheetSizeWidth;
  v0 = (ctx->usHeadDots - ctx->usSheetSizeWidth) / 2;
  v6 = ctx->pusPulseTransLineBufTab[1] + v0;
  out = ctx->pusOutLineBufTab[0] + ctx->sPrintSideOffset + v0;
  v11 = 0;
  if ( out >= ctx->pusOutLineBufTab[0] ) {
    v10 = ctx->sPrintSideOffset
        + ctx->usSheetSizeWidth
        + v0;
    if ( v10 > BUF_SIZE )
    {
      v10 -= BUF_SIZE;
      v16 = ctx->usSheetSizeWidth - v10;
    }
  } else {
    v1 = ctx->pusOutLineBufTab[0] - out;
    out = ctx->pusOutLineBufTab[0];
    v16 = ctx->usSheetSizeWidth - v1;
    v10 = v11;
  }
  v5 = out + 2 * v16;
  v4 = v16 + v6;
  v14 = 128 - v11;

  while ( v14 ) {
    v12 = (((1024 - ctx->pusSideEdgeLvCoefTable[*v6++] * (uint32_t)ctx->pusSideEdgeCoefTable[128 - v14--]) >> 10) * *out) >> 10;
    if ( v12 > ctx->iMaxPulseValue )
      v12 = ctx->iMaxPulseValue;
    *out++ = v12;
  }

  v9 = v5;
  v7 = v4;
  v15 = 128 - v10;

  while ( v15 ) {
    v9--;
    --v7;
    v13 = ((1024 - (ctx->pusSideEdgeLvCoefTable[*v7] * (uint32_t)ctx->pusSideEdgeCoefTable[128 - v15--]) >> 10) * *v9) >> 10;
    if ( ctx->iMaxPulseValue < v13 )
      v13 = ctx->iMaxPulseValue;
    *v9 = v13;
  }
}

static void LeadEdgeCorrection(struct lib6145_ctx *ctx)
{
  uint32_t v0;
  uint16_t *out;
  int32_t v4;
  uint32_t v5;
  int32_t v6;

  if ( ctx->iLeadEdgeCorrectPulse ) {
    v6 = ctx->usSheetSizeWidth;
    out = ctx->pusOutLineBufTab[0] + ctx->sPrintSideOffset
       + ((ctx->usHeadDots - ctx->usSheetSizeWidth) / 2);
    if ( out >= ctx->pusOutLineBufTab[0] ) {
      v5 = ctx->sPrintSideOffset
         + ctx->usSheetSizeWidth
         + ((ctx->usHeadDots - ctx->usSheetSizeWidth) / 2);
      if ( v5 > BUF_SIZE )
        v6 = ctx->usSheetSizeWidth - (v5 - BUF_SIZE);
    } else {
      v0 = ctx->pusOutLineBufTab[0] - out;
      out = ctx->pusOutLineBufTab[0];
      v6 = ctx->usSheetSizeWidth - v0;
    }

    while ( v6-- ) {
      v4 = (ctx->iLeadEdgeCorrectPulse / 4) + *out;
      if ( v4 > ctx->iMaxPulseValue )
        v4 = ctx->iMaxPulseValue;
      *out++ = v4;
    }
    --ctx->iLeadEdgeCorrectPulse;
  }
}

static void RecieveDataOP_Post(struct lib6145_ctx *ctx)
{
  return;
}

static void RecieveDataYMC_Post(struct lib6145_ctx *ctx)
{
  return;
}

static void RecieveDataOPLevel_Post(struct lib6145_ctx *ctx)
{
  return;
}

static void RecieveDataOPMatte_Post(struct lib6145_ctx *ctx)
{
  return;
}

static void ImageLevelAddition(struct lib6145_ctx *ctx)
{
  if ( ctx->uiLevelAveCounter < ctx->usPrintSizeHeight )
  {
    ImageLevelAdditionEx(ctx, &ctx->uiLevelAveAddtion, 0, ctx->usSheetSizeWidth);
    ctx->uiLevelAveCounter++;
    if ( ctx->uiLevelAveCounter2-- == 0 )
    {
        ImageLevelAdditionEx(ctx,
	      &ctx->uiLevelAveAddtion2,
	      ctx->uiOffsetCancelCheckPRec,
	      ctx->usCancelCheckDotsForPRec);
      ++ctx->uiLevelAveCounter2;
    }
  }
}

static void ImageLevelAdditionEx(struct lib6145_ctx *ctx, uint32_t *a1, uint32_t a2, int32_t a3)
{
  int32_t v3;
  uint8_t *v6;
  int32_t i;
  int32_t v8;

  v6 = ctx->pusPulseTransLineBufTab[1] + a2 +
	  ((ctx->usHeadDots - ctx->usSheetSizeWidth) / 2);
  v8 = a3;
  for ( i = 0; v8-- ; i += v3 ) {
    v3 = *v6++;
  }
  *a1 += i;
}

#endif /* S6145_UNUSED */
