/*
   libS2245ImageReProcess -- Re-implemented Image Processing library for
                             the Sinfonia CHC-S2245 printer family

   Copyright (c) 2020 Solomon Peachy <pizza@shaftnet.org>

   ** ** ** ** Do NOT contact Sinfonia about this library! ** ** ** **

   This is intended to be a drop-in replacement for Sinfonia's proprietary
   libS225IP library, which is necessary in order to utilize their
   CHC-S2245 printer family.

   Sinfonia Inc was not involved in the creation of this library, and
   is not responsible in any way for the library or any deficiencies in
   its output.  They will provide no support if it is used.

   If you have the appropriate permission fron Sinfonia, we recommend
   you use their official libS2245IP library instead, as it
   will generate the highest quality output. However, it is only
   available for x86/x86_64 targets on Windows. Please contact your local
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

#define LIB_VERSION "0.1.0"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define USE_EXTRA_STUFF  // For extensions made to the base library

//-------------------------------------------------------------------------
// Exported Functions

bool ip_imageProc(uint16_t *destData, uint8_t *srcInRgb,
		  uint16_t width, uint16_t height, void *srcIpp);

bool ip_checkIpp(uint16_t width, uint16_t height, void *srcIpp);

bool ip_getMemorySize(uint32_t *szMemory,
		      uint16_t width, uint16_t height,
		      void *srcIpp);

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
// External data formats

struct ippHeatCorr {
	int32_t  tankDivisor[4];
	int32_t  tankRowInitVals[4];
	int32_t  tankRowOldInitVals[4];
	int32_t  tankCoef[5];
	int32_t  tankPlusParam;
	int32_t  tankMinusParam;
	int32_t  tankPlusLimit;
	int32_t  tankMinusLimit;
	int32_t  useHeatCorrection;
	uint8_t  pad_0x58[40];
} __attribute__((packed));

struct SIppData {
	uint16_t field_0x0; // see getMaxPulse
	uint16_t field_0x2; // see getMaxPulse
	uint16_t maxLineSum;
	uint16_t minRowSum;
	uint16_t maxLineCorrectionFactor;
	uint16_t nextWeight;
	uint16_t prevWeight;
	uint16_t mtfPreCalcCutoff;
	uint16_t autoGenMode;
	uint8_t  autoGenBase[2];
	uint8_t  pad_0x14[6];
	uint16_t pulseExMap[256];
	uint8_t  pad_0x21a[2];
	struct ippHeatCorr heatCorrection;
	int16_t  heatCorrectionMinusTable[256];
	int16_t  heatCorrectionPlusTable[256];
	uint16_t lineCorrection[256];
	uint8_t sideEdgeCorrectionComp[64];
	uint8_t sideEdgeCorrectionTable[256];
}  __attribute__((packed));

struct ippHeader {
	char  model[16];
	char  ipp[4];
	uint8_t  version;
	uint8_t  pad_0x15[43];
} __attribute__((packed));

struct ippConf {
	uint16_t width;   /* Overridden at runtime */
	uint16_t height;  /* Overridden at runtime */
	uint16_t headWidth; // Always 1920
	uint8_t  planes; // 3 or 4, can be up to 16!
	uint8_t  printmode; // Corresponds to corrdata_req field 1
	uint8_t  corrections1; // Corresponds to corrdata_req field 2
	uint8_t  corrections2; // B0,B1,B2
	uint8_t  borderCapable; // 0x02 if we can do 5x7-on-6x8
	uint8_t  pad_0xb[21];
	uint8_t  planeOrder[16];
	uint8_t  pad_0x30[16];
} __attribute__((packed));

struct ippData {
	struct ippHeader header;
	struct ippConf   conf;
	struct SIppData  plane_rc;
	struct SIppData  plane_gm;
	struct SIppData  plane_by;
	struct SIppData  plane_oc;
	// Can have up to 16 planes, think about this.
} __attribute__((packed));

//-------------------------------------------------------------------------
// Private Structures

struct CHeatCorrProc {
	struct SIppData ippData;
	struct ippHeatCorr heatCorrection;
	uint16_t width;
	uint16_t height;
	uint16_t headWidth;
//	uint16_t pad_0xa62;
	int32_t  lineStartOffset; // always 0
	uint16_t maxPulse;
	uint16_t tankWidth;
//      uint8_t  pad_0xa6c[4];
	int16_t  *tankBuf;
	int16_t  *tankRowSrc;
	int32_t  *tankRowPtrs[2];
	int32_t  *tankRowBufs[5];
	uint32_t curRow;
	uint8_t  initialized;
};

struct CIppMng {
	struct ippHeader header;
	struct ippConf   conf;
	struct SIppData  *ippData;
	uint8_t          initialized;
};

struct CImageProc {
	struct CIppMng   *ippMng;
	struct ippConf   conf;
	struct SIppData  planeIPPdata;
	uint32_t pixelsOCG; // width*height*3
	uint32_t pixelsOCM; // width*height*4
//	uint8_t  pad_0xa2c[4];
	uint16_t *imageScratchDataPtr;
	uint32_t imageDataLen; // width*height*4planes*2bpp
//	uint8_t  pad_0xa3c[4];
	uint16_t *imageDataPtr;
	uint8_t  planesOCG;
	uint8_t  planesOCM;
	uint8_t  currentPlane;
	uint8_t  correctMtf;
	uint8_t  correctHeat;
	uint8_t  correctLine;
	uint8_t  correctSideEdge;
//	uint8_t  pad_0xa4f;
	uint32_t pixels; // width*height
	int16_t  mtfCorrPlaneTable[512];
	uint16_t srcWidth;
	uint16_t srcHeight;
};

struct CMidDataGen {
	struct SIppData *planeIppData;
	uint8_t  *outDataPtr;
	uint8_t  plane;
	uint8_t  pad;
	uint16_t width;
	uint16_t height;
};

struct crop_conf {
	uint16_t startCol;
	uint16_t startRow;
	uint16_t numCols;
	uint16_t numRows;
};

struct pic_data {
	uint8_t  *srcPtr;
	uint16_t inCols;
	uint16_t inRows;
	uint16_t outCols;
	uint16_t outRows;
	uint8_t  bytes_pp;
};

// Data format Sanity checks
#define STATIC_ASSERT(test_for_true) _Static_assert((test_for_true), "(" #test_for_true ") failed")

STATIC_ASSERT(sizeof(struct ippData) == 10224);

#undef STATIC_ASSERT

//-------------------------------------------------------------------------
// Private data declarations


//-------------------------------------------------------------------------
// Private Function declarations

/***  CIppMng  ***/
static void CIppMng_Init(struct CIppMng *this)
{
	memset(this, 0, sizeof(*this));
}

static void CIppMng_Cleanup(struct CIppMng *this)
{
	if (this->ippData) {
		free(this->ippData);
		this->ippData = NULL;
	}
}

static const char s2245HeaderModel[] = "CHC-S2245       ";
static const char s2245IppStr[] = "IPP=";

static bool CIppMng_CheckHeader(struct CIppMng *this, struct ippData *ippData)

{
	memcpy(&this->header, &ippData->header, sizeof(this->header));

	if (memcmp(this->header.model, s2245HeaderModel, 16))
		return 0;
	if (memcmp(this->header.ipp, s2245IppStr, 4))
		return 0;
	if (this->header.version != 0x3)
		return 0;

	return 1;
}

static bool CIppMng_CheckConf(struct CIppMng *this, struct ippData *ippData)

{
	uint8_t tmp;

#ifndef USE_EXTRA_STUFF
	memcpy(&this->conf, &ippData->conf, sizeof(this->conf));
#else
	(void)ippData;
#endif

	if (!this->conf.headWidth || !this->conf.planes)
		return 0;

	/* OC mode, Glossy or matte */
	tmp = this->conf.printmode & 0x07;
	if (tmp != 1 && tmp != 2)
		return 0;

	/* Quality mode, 0 or 1*/
	tmp = (this->conf.printmode >> 3) & 3;
	if (tmp > 1)
		return 0;

	return 1;
}

static bool CIppMng_CheckIpp(struct CIppMng *this, struct ippData *ippData)
{
	bool rval;

	rval = CIppMng_CheckHeader(this, ippData);

	if (rval)
		rval = CIppMng_CheckConf(this, ippData);

	return rval;
}

static bool CIppMng_SetConf(struct CIppMng *this, struct ippData *ippData,
			    uint16_t width, uint16_t height)

{
	memcpy(&this->conf, &ippData->conf, sizeof(this->conf));

	if (!width || !height)
		return 0;
	this->conf.width = width;
	this->conf.height = height;

#if (__BYTE_ORDER != __LITTLE_ENDIAN)
	this->conf.headWidth = le16_to_cpu(this->conf.headWidth);
#endif

	return 1;
}

static bool CIppMng_SetData(struct CIppMng *this, struct ippData *ippData)

{
	if (this->ippData)
		free(this->ippData);

	if (!this->conf.planes)
		return 0;

	this->ippData = malloc(this->conf.planes * sizeof(*this->ippData));
	if (!this->ippData)
		return 0;

	memcpy(this->ippData, &ippData->plane_rc, this->conf.planes * sizeof(*this->ippData));

#if (__BYTE_ORDER != __LITTLE_ENDIAN)
#define SWAP16(__x) __x = le16_to_cpu(ippBlock->__x)
#define SWAP32(__x) __x = le32_to_cpu(ippBlock->__x)
	int i, j;
	for (i = 0 ; i < this->conf.planes ; i++) {
		struct SIppData *ippBlock = &ippData->plane_rc + i;
		swap16(field_0x0);
		swap16(field_0x2);
		swap16(maxLineSum);
		swap16(minRowSum);
		swap16(maxLineCorrecitonFactor);
		swap16(nextWeight);
		swap16(prevWeight);
		swap16(mtfPreCalcCutoff);
		swap16(autoGenMode);
		for (j = 0 ; j < 256 ; j++) {
			swap16(pulseExMap[j]);
			swap16(heatCorrectionMinusTable[j]);
			swap16(heatCorrectionPlusTable[j]);
			swap16(lineCorrection);
		}
		for (j = 0 ; j < 4 ; j++) {
			swap32(heatCorrection.tankDivisor[j]);
			swap32(heatCorrection.tankRowInitVals[j]);
			swap32(heatCorrection.tankRowOldInitVals[j]);
		}
		for (j = 0 ; j < 5 ; j++) {
			swap32(heatCorrection.tankCoef[j]);
		}
		swap32(tankPlusParam);
		swap32(tankMinusParam);
		swap32(tankPlusLimit);
		swap32(tankMinusLimit);
		swap32(useHeatCorrection);
	}
#undef SWAP16
#undef SWAP32
#endif

	return 1;
}

static bool CIppMng_SetIPP(struct CIppMng *this, struct ippData *ippData,
			   uint16_t width, uint16_t height)

{
	bool rval;

	this->initialized = 0;

	rval = CIppMng_SetConf(this, ippData, width, height);
	if (rval)
		rval = CIppMng_CheckIpp(this, ippData);
	if (rval)
		rval = CIppMng_SetData(this, ippData);
	if (rval)
		this->initialized = 1;

	return rval;
}

static bool CIppMng_GetNumOfColor(struct CIppMng *this, uint8_t *planesOCG, uint8_t *planesOCM)
{
	if (!this->initialized)
	    return 0;

	*planesOCG = 3;
	*planesOCM = 4;

	return 1;
}

static bool CIppMng_GetIppConf(struct CIppMng *this, struct ippConf *conf,
			       uint8_t *planesOCG, uint8_t *planesOCM)

{
	if (!this->initialized)
	    return 0;

	memcpy(conf, &this->conf, sizeof(*conf));
	CIppMng_GetNumOfColor(this, planesOCG, planesOCM);

	return 1;
}

static bool CIppMng_GetIppNo(struct CIppMng *this, uint8_t *planeOffset, uint8_t plane)
{
	if (!this->initialized || plane > 0x10)
		return 0;

	*planeOffset = this->conf.planeOrder[plane];

	return 1;
}

static bool CIppMng_GetIppData(struct CIppMng *this, struct SIppData *destPtr, uint8_t planeOffset)

{
	if (!this->initialized || planeOffset >= this->conf.planes)
		return 0;

	memcpy(destPtr, &this->ippData[planeOffset], sizeof (*destPtr));

	return 1;
}

/***  CMidDataGen  ***/

static void CMidDataGen_Init(struct CMidDataGen *this)
{
	this->planeIppData = NULL;
}

static bool CMidDataGen_CheckAutoGenNo(struct CMidDataGen *this)

{
	return (this->planeIppData->autoGenMode == 1 ||  // glossy
		this->planeIppData->autoGenMode == 2);   // matte
}

static void CMidDataGen_GD01Proc(struct CMidDataGen *this, uint8_t *outDataPtr,
				 uint16_t width, uint16_t height, uint8_t *autoGenBase)
{
	memset(outDataPtr, *autoGenBase, height * width);
	(void)this;
}

static const uint8_t GD_02_MIDDATA[288] = {
	0xB3, 0xBD, 0x93, 0x27, 0xD6, 0x9F, 0xF6, 0xBD, 0xB3, 0x7D, 0xD6, 0x98, 0x36, 0xBD, 0x33, 0xBD,
	0x5C, 0xE2, 0x66, 0xA9, 0x3A, 0x0D, 0x7D, 0xFE, 0x4C, 0xEB, 0xC4, 0xED, 0xCD, 0xEC, 0x5D, 0xEF,
	0xD9, 0x8C, 0x9F, 0x89, 0x71, 0xAF, 0x5B, 0x4E, 0xB1, 0x1B, 0x31, 0xB9, 0xDB, 0x63, 0x36, 0x7A,
	0x87, 0x33, 0x11, 0xF3, 0xFC, 0xDF, 0xEE, 0x7D, 0xB7, 0xBE, 0x5F, 0x0E, 0xED, 0x6C, 0x92, 0x9C,
	0xDF, 0x78, 0xCD, 0xCE, 0xF1, 0x8C, 0xD1, 0x5F, 0x50, 0xEA, 0x9C, 0xFD, 0x5F, 0x53, 0x66, 0xFB,
	0x9F, 0x9C, 0x76, 0xFB, 0x6E, 0x3B, 0x47, 0xCE, 0x4E, 0xF9, 0xBA, 0x53, 0xFE, 0xFF, 0x7A, 0x7D,
	0x92, 0xD3, 0x7E, 0xF2, 0x73, 0x77, 0xD7, 0xD7, 0x36, 0xC7, 0x26, 0xC7, 0x57, 0xDC, 0x92, 0x99,
	0xF6, 0x19, 0x72, 0xC8, 0xFB, 0xBC, 0xFE, 0x3C, 0x3E, 0xEB, 0xF8, 0xA6, 0x76, 0xFE, 0x26, 0x7B,
	0xD8, 0xF6, 0x16, 0x63, 0xF2, 0xCA, 0xDF, 0x24, 0x16, 0x79, 0xF9, 0xCE, 0xD3, 0xEF, 0x93, 0x7D,
	0x1F, 0xE6, 0xD8, 0xE7, 0xCF, 0x5B, 0xF2, 0xE3, 0x97, 0x35, 0xF8, 0x48, 0xF6, 0x69, 0x36, 0x9D,
	0xF3, 0x47, 0x67, 0xCF, 0x76, 0x99, 0x13, 0xE7, 0x4F, 0x97, 0xF6, 0xF9, 0xCE, 0x27, 0x57, 0x94,
	0xCE, 0x4D, 0xCE, 0x39, 0xE5, 0x90, 0xFF, 0x0C, 0x03, 0xBB, 0xEC, 0xF9, 0x32, 0x9E, 0x77, 0xB2,
	0x8E, 0x79, 0x22, 0x87, 0xD1, 0xFE, 0xBA, 0x49, 0xCF, 0xE5, 0xCF, 0x7E, 0xFB, 0x8B, 0x09, 0x95,
	0xEC, 0xE6, 0x43, 0x3B, 0xF3, 0x9D, 0x69, 0xEE, 0x7F, 0x6B, 0x13, 0xEB, 0x73, 0x08, 0x18, 0xF7,
	0xF3, 0x67, 0xF8, 0x7F, 0xDF, 0x33, 0xFB, 0x65, 0xD1, 0xE3, 0x61, 0x7C, 0xBE, 0x79, 0xDF, 0xE1,
	0x39, 0xDF, 0x33, 0xF3, 0xC7, 0xF8, 0x19, 0x17, 0x59, 0x46, 0xEE, 0x73, 0xFD, 0x2F, 0xDC, 0x3F,
	0xAB, 0x77, 0x6E, 0xE6, 0x9C, 0x9F, 0x39, 0x67, 0xEC, 0xF4, 0xDC, 0xF9, 0x7C, 0x4C, 0x6E, 0x72,
	0xDF, 0x44, 0xCE, 0xCE, 0xEC, 0x6D, 0xD9, 0x4D, 0xFA, 0xA6, 0x3F, 0xAD, 0xD9, 0x05, 0xD3, 0xB0
};

static void CMidDataGen_GD02Proc(struct CMidDataGen *this, uint8_t *outDataPtr,
				 uint16_t width, uint16_t height, uint8_t *autoGenBase)

{
	const uint8_t *scratch = GD_02_MIDDATA;
	uint16_t row;
	uint8_t *outPtr = outDataPtr;

	for (row = 0 ; row < height ; row++) {
		uint16_t col;
		for (col = 0 ; col < width ; col++) {
			uint8_t outVal;
 			if ((scratch[(col % 0x30 + (row % 0x30) * 0x30) >> 3] &
			     (1 << (((col % 0x30) % 8) & 0x1f))) == 0) {
				outVal = autoGenBase[0];
			} else {
				outVal = autoGenBase[1];
			}
			*outPtr = outVal;
			outPtr++;
		}
	}
	(void)this;
}

static void CMidDataGen_GenDataEx(struct CMidDataGen *this)

{
	uint16_t autogen;

	autogen = this->planeIppData->autoGenMode;
	if (autogen == 1) {  // glossy
		CMidDataGen_GD01Proc(this, this->outDataPtr + this->width * this->height * this->plane,
				     this->width, this->height, this->planeIppData->autoGenBase);
	} else if (autogen == 2) { // matte
		CMidDataGen_GD02Proc(this, this->outDataPtr + this->width * this->height * this->plane,
				     this->width, this->height, this->planeIppData->autoGenBase);
	}

	return;
}

static bool CMidDataGen_GenData(struct CMidDataGen *this, uint8_t *outDataPtr, uint8_t plane,
				uint16_t width, uint16_t height, struct SIppData *ippData)
{
	if (!outDataPtr || !width || !height || !ippData)
		return 0;

	this->outDataPtr = outDataPtr;
	this->plane = plane;
	this->width = width;
	this->height = height;
	this->planeIppData = ippData;

	if (!CMidDataGen_CheckAutoGenNo(this))
		return 0;
	CMidDataGen_GenDataEx(this);

	return 1;
}

/*** CHeatCorrProc ***/
static void CHeatCorrProc_Init(struct CHeatCorrProc *this)
{
	memset(this, 0, sizeof(*this));
}

static bool CHeatCorrProc_SetIppData(struct CHeatCorrProc *this, struct SIppData *ippData,
				     uint16_t width, uint16_t height, uint16_t headWidth,
				     uint16_t maxPulse)

{
	memcpy(&this->ippData, ippData, sizeof(this->ippData));
	memcpy(&this->heatCorrection, &this->ippData.heatCorrection, sizeof(this->heatCorrection));
	this->width = width;
	this->height = height;
	this->headWidth = headWidth;
	this->maxPulse = maxPulse;

	if (this->width > this->headWidth)
		return 0;

	this->lineStartOffset = 0;
	this->initialized = 1;

	return 1;
}

static bool CHeatCorrProc_InitTank(struct CHeatCorrProc *this)

{
	bool rval = 0;
	int i;

#ifdef USE_EXTRA_STUFF
	this->tankWidth = this->width + 5; /* Sinfonia algorithms read 1 element past this buffer */
#else
	this->tankWidth = this->width + 4;
#endif
	if (!this->tankRowBufs[0]) {
		this->tankRowBufs[0] = malloc(this->tankWidth * sizeof(uint32_t));
		if (!this->tankRowBufs[0])
			goto done;
	}
	for (i = 0 ; i < this->tankWidth ; i++) {
		this->tankRowBufs[0][i] = this->heatCorrection.tankRowInitVals[3];
	}
	if (!this->tankRowBufs[1]) {
		this->tankRowBufs[1] = malloc(this->tankWidth * sizeof(uint32_t));
		if (!this->tankRowBufs[1])
			goto done;
	}
	for (i = 0 ; i < this->tankWidth ; i++) {
		this->tankRowBufs[1][i] = this->heatCorrection.tankRowInitVals[2];
	}
	if (!this->tankRowBufs[2]) {
		this->tankRowBufs[2] = malloc(this->tankWidth * sizeof(uint32_t));
		if (!this->tankRowBufs[2])
			goto done;
	}
	for (i = 0 ; i < this->tankWidth ; i++) {
		this->tankRowBufs[2][i] = this->heatCorrection.tankRowInitVals[1];
	}
	if (!this->tankRowBufs[3]) {
		this->tankRowBufs[3] = malloc(this->tankWidth * sizeof(uint32_t));
		if (!this->tankRowBufs[3])
			goto done;
	}
	for (i = 0 ; i < this->tankWidth ; i++) {
		this->tankRowBufs[3][i] = this->heatCorrection.tankRowInitVals[0];
	}
	if (!this->tankRowBufs[4]) {
		this->tankRowBufs[4] = malloc(this->tankWidth * sizeof(uint32_t));
		if (!this->tankRowBufs[4])
			goto done;
	}
	for (i = 0 ; i < this->tankWidth ; i++) {
		this->tankRowBufs[4][i] = 0;
	}
	if (!this->tankRowPtrs[0]) {
		this->tankRowPtrs[0] = malloc(this->tankWidth * sizeof(uint32_t));
		if (!this->tankRowPtrs[0])
			goto done;
	}
	for (i = 0 ; i < this->tankWidth ; i++) {
		this->tankRowPtrs[0][i] = 0;
	}
	if (!this->tankRowPtrs[1]) {
		this->tankRowPtrs[1] = malloc(this->tankWidth * sizeof(uint32_t));
		if (!this->tankRowPtrs[1])
			goto done;
	}
	for (i = 0 ; i < this->tankWidth ; i++) {
		this->tankRowPtrs[1][i] = 0;
	}

#ifdef USE_EXTRA_STUFF
	this->tankWidth--;
#endif

	rval = 1;
done:
	return rval;
}

static void CHeatCorrProc_PreReadLine(struct CHeatCorrProc *this, uint16_t *tankSrc, uint16_t useHeatCorrection)

{
	int i;
	int heatStep;
	int rowStep;

	if (!useHeatCorrection)
		return;

	if (useHeatCorrection > 0xc)
		useHeatCorrection = 0xc;

	for (i = 0 ; i < this->tankWidth ; i++) {
		this->tankRowBufs[4][i] = 0;
	}

	for (heatStep = useHeatCorrection, rowStep = 0 ; heatStep > 0 && ((rowStep + this->curRow) < this->height) ; heatStep--, rowStep++) {
		int col;

		int *rowBufPtr = this->tankRowBufs[4] + this->lineStartOffset + 2;
		for (col = 0; col < this->width ; col++) {
			*rowBufPtr += heatStep * *tankSrc;
			tankSrc++;
			rowBufPtr++;
		}
	}
	/* If we stopped early, clean up! */
	if (heatStep != useHeatCorrection) {
		int col;
		int local_50;
		int local_48 = useHeatCorrection;
		int divisor = 0;
		int *rowBufPtr;

		for (local_50 = useHeatCorrection - heatStep ; local_50 > 0 ; local_50--) {
			divisor += local_48;
			local_48--;
		}
		if (divisor == 0) {
			divisor = 1;
		}
		rowBufPtr = this->tankRowBufs[4] + this->lineStartOffset + 2;
		for (col = 0 ; col < this->width ; col++) {
			*rowBufPtr /= divisor;
			rowBufPtr++;
		}
	}

	return;
}

static uint16_t CHeatCorrProc_GetBitsOfPulse(struct CHeatCorrProc *this, uint16_t maxPulse)

{
	uint16_t i;

	for (i = 0 ; maxPulse != 0 ; i++) {
		maxPulse >>= 1;
	}
	(void)this;
	return i;
}


static int32_t CHeatCorrProc_GetCorrPulsePreRead(struct CHeatCorrProc *this, int srcVal, int param_2)

{
	int32_t local_28;
	int32_t local_24;
	int32_t local_20;

	if (this->heatCorrection.useHeatCorrection == 0)
		return 0;

	param_2 -= srcVal;

	if (param_2 == 0)
		return 0;

	if (param_2 < 1) {
		local_24 = (this->heatCorrection).tankMinusLimit;
		local_28 = (this->heatCorrection).tankMinusParam;
		local_20 = -param_2;
	} else {
		local_24 = (this->heatCorrection).tankPlusLimit;
		local_28 = (this->heatCorrection).tankPlusParam;
		local_20 = param_2;
	}
	if (local_20 < local_24) {
		return 0;
	}

	return (local_28 * param_2 >> 10);
}

static void CHeatCorrProc_CorrectEx(struct CHeatCorrProc *this, uint16_t *tankBuf, uint16_t *tankRowSrc, uint8_t *scratchRowBuf)
{
	uint16_t col;
	uint8_t pulseBits;
	int32_t tankDivisor;

	int32_t pulsePreRead;
	int32_t outPulse;
	int32_t heatCorrection;
	int32_t *local_48;
	int32_t *local_40;
	uint16_t *srcPixel;
	uint8_t *scratchPtr;

	pulseBits = CHeatCorrProc_GetBitsOfPulse(this,this->maxPulse);
	tankDivisor = this->heatCorrection.tankDivisor[3];

	local_40 = this->tankRowBufs[0] + (long)this->lineStartOffset + 2;
	local_48 = this->tankRowBufs[4] + (long)this->lineStartOffset + 2;
	srcPixel = tankRowSrc;
	scratchPtr = scratchRowBuf;
	for (col = this->width ; col != 0 ; col --) {
		int iVar4 = *srcPixel -
			((((0x100000 << (pulseBits & 0x1f)) / tankDivisor) * (*srcPixel + *local_40)) >> 0x14);
		if (iVar4 == 0) {
			heatCorrection = 0;
		} else {
			if (iVar4 < 1) {
				heatCorrection = (this->ippData).heatCorrectionPlusTable[*scratchPtr];
			} else {
				heatCorrection = (this->ippData).heatCorrectionMinusTable[*scratchPtr];
			}
		}
		pulsePreRead = CHeatCorrProc_GetCorrPulsePreRead(this, *srcPixel, *local_48);
		outPulse = pulsePreRead + *srcPixel + ((iVar4 * heatCorrection) >> (pulseBits & 0x1f));
		if (outPulse < 0) {
			outPulse = 0;
		} else {
			if (this->maxPulse < outPulse) {
				outPulse = this->maxPulse;
			}
		}
		*tankBuf = outPulse;
		*local_40 += outPulse;
		srcPixel++;
		tankBuf++;
		scratchPtr++;
		local_40++;
		local_48++;
	}
	return;
}

static void CHeatCorrProc_GetTankCoefTransfer(struct CHeatCorrProc *this, int32_t *param_1, int32_t *param_2, int32_t *param_3,
					      int32_t *param_4, int32_t *param_5, int32_t *param_6, int32_t *param_7, int32_t *param_8)

{
	*param_1 = 0x10000;
	*param_1 *= this->heatCorrection.tankCoef[4];
	*param_1 /= this->heatCorrection.tankDivisor[3];
	*param_2 = 0x10000;
	*param_2 *= this->heatCorrection.tankCoef[3];
	*param_2 /= this->heatCorrection.tankDivisor[3];
	*param_3 = 0x10000;
	*param_3 *= this->heatCorrection.tankCoef[3];
	*param_3 /= this->heatCorrection.tankDivisor[2];
	*param_4 = 0x10000;
	*param_4 *= this->heatCorrection.tankCoef[2];
	*param_4 /= this->heatCorrection.tankDivisor[2];
	*param_5 = 0x10000;
	*param_5 *= this->heatCorrection.tankCoef[2];
	*param_5 /= this->heatCorrection.tankDivisor[1];
	*param_6 = 0x10000;
	*param_6 *= this->heatCorrection.tankCoef[1];
	*param_6 /= this->heatCorrection.tankDivisor[1];
	*param_7 = 0x10000;
	*param_7 *= this->heatCorrection.tankCoef[1];
	*param_7 /= this->heatCorrection.tankDivisor[0];
	*param_8 = 0x10000;
	*param_8 *= this->heatCorrection.tankCoef[0];
	*param_8 /= this->heatCorrection.tankDivisor[0];
}

static void CHeatCorrProc_TankHeatTransferEx(struct CHeatCorrProc *this, int32_t *param_1, int32_t *param_2, int32_t *param_3,
					     int32_t param_4, int32_t param_5, int32_t param_6, int32_t param_7)

{
	int local_54;
	int local_50;
	uint16_t col;
	int *local_48 = NULL;
	int *local_40;
	int *local_38 = NULL;

	if (0 < param_4) {
		local_38 = param_1 + 2;
	}

	local_40 = param_2 + 2;
	if (0 < param_7) {
		local_48 = param_3 + 2;
	}

	for (col = this->width ; col != 0 ; col--) {
		local_50 = *local_40 * param_5;
		if (0 < param_4) {
			local_50 -= *local_38 * param_4;
		}
		if (param_7 < 1) {
			local_54 = 0;
		} else {
			local_54 = *local_48 * param_7;
		}
		*local_40 = ((local_54 - *local_40 * param_6) >> 0x10) + (*local_40 - (local_50 >> 0x10));
		if (0 < param_4) {
			local_38++;
		}
		local_40++;
		if (0 < param_7) {
			local_48++;
		}
	}
}

/* Computes the heat transfer across a single horizontal scanline! */
static void CHeatCorrProc_DotHeatTransExRevOld(struct CHeatCorrProc *this, int32_t param_1, int32_t *tankRowBuf)

{
	int i;
	int32_t *outPtr;
	int32_t local_50;
	int32_t local_4c;
	int32_t local_48;
	int32_t pixelPlusTwo;
	int32_t pixelPlusOne;
	int32_t pixelCurrent;
	int32_t *local_38;

	/* Initialize shoulders at either side (-2/+2) */
	for (i = 0 ; i < 2 ; i++) {
		tankRowBuf[i] = tankRowBuf[2];
		tankRowBuf[this->tankWidth - 1 - i] = tankRowBuf[this->tankWidth - 3];
	}
#ifdef USE_EXTRA_STUFF
	/* We allocated an extra element at the end; fill it in */
	tankRowBuf[this->tankWidth] = tankRowBuf[this->tankWidth-1];
#endif
	pixelCurrent = tankRowBuf[2];

	param_1 /= 2;
	local_48 = param_1 * (*tankRowBuf + tankRowBuf[1] * -2 + pixelCurrent);
	pixelPlusOne = tankRowBuf[3];
	local_4c = param_1 * (tankRowBuf[1] + pixelCurrent * -2 + pixelPlusOne);
	local_50 = pixelCurrent + pixelPlusOne * -2 + tankRowBuf[4];
	outPtr = tankRowBuf + 2;
	pixelPlusTwo = tankRowBuf[4];
	local_38 = tankRowBuf + 5; // NOTE:  this overflows the buffer on the last element, as buffer is +- 2 but this is +3!  We work around it with USE_EXTRA_STUFF.

	for (i = this->width ; i > 0 ; i--) {
		int32_t outPixel;

		local_50 *= param_1;
		outPixel = ((local_4c >> 6) + pixelCurrent) -
			(param_1 * (((local_4c * 2 - local_48) - local_50) >> 7) >> 7);
		if (outPixel < 0) {
			outPixel = 0;
		}
		local_48 = local_4c;
		local_4c = local_50;
		local_50 = pixelPlusOne + pixelPlusTwo * -2 + *local_38;
		pixelCurrent = pixelPlusOne;
		pixelPlusOne = pixelPlusTwo;
		pixelPlusTwo = *local_38;
		*outPtr = outPixel;
		outPtr++;
		local_38++;
	}
}

static void CHeatCorrProc_HeatTransferProc(struct CHeatCorrProc *this)

{
	int32_t local_48;
	int32_t local_44;
	int32_t local_40;
	int32_t local_3c;
	int32_t local_38;
	int32_t local_34;
	int32_t local_30;
	int32_t local_2c;

	CHeatCorrProc_GetTankCoefTransfer(this, &local_2c, &local_30, &local_34, &local_38,
					  &local_3c, &local_40, &local_44, &local_48);
	memcpy(this->tankRowPtrs[0], this->tankRowBufs[0], this->tankWidth * 4);

	CHeatCorrProc_TankHeatTransferEx(this, NULL, this->tankRowBufs[0], this->tankRowBufs[1],
					 0, local_2c, local_30, local_34);
	memcpy(this->tankRowPtrs[1], this->tankRowBufs[1], this->tankWidth * 4);

	CHeatCorrProc_TankHeatTransferEx(this, this->tankRowPtrs[0], this->tankRowBufs[1], this->tankRowBufs[2],
					 local_30, local_34, local_38, local_3c);
	memcpy(this->tankRowPtrs[0], this->tankRowBufs[2], this->tankWidth * 4);

	CHeatCorrProc_TankHeatTransferEx(this, this->tankRowPtrs[1], this->tankRowBufs[2], this->tankRowBufs[3],
					 local_38, local_3c, local_40, local_44);
	CHeatCorrProc_TankHeatTransferEx(this, this->tankRowPtrs[0], this->tankRowBufs[3], NULL,
					 local_40, local_44, local_48, 0);

	CHeatCorrProc_DotHeatTransExRevOld(this, this->heatCorrection.tankRowOldInitVals[3], this->tankRowBufs[0]);
	CHeatCorrProc_DotHeatTransExRevOld(this, this->heatCorrection.tankRowOldInitVals[2], this->tankRowBufs[1]);
	CHeatCorrProc_DotHeatTransExRevOld(this, this->heatCorrection.tankRowOldInitVals[1], this->tankRowBufs[2]);
	CHeatCorrProc_DotHeatTransExRevOld(this, this->heatCorrection.tankRowOldInitVals[0], this->tankRowBufs[3]);
}

static bool CHeatCorrProc_Correction(struct CHeatCorrProc *this,
				     uint16_t *destData, uint16_t *srcData,
				     uint8_t *scratchData)
{
	bool rval = 0;
	uint32_t pixels;
	int i;

	if (!this->initialized || !this->width || !this->height)
		goto done;

	pixels = this->width * this->height;

	this->tankBuf = malloc(pixels * 2);
	if (!this->tankBuf)
		goto done;

	CHeatCorrProc_InitTank(this);
	this->tankRowSrc = (int16_t*) srcData;

	for (this->curRow = 0 ; this->curRow < this->height ; this->curRow++) {
		uint16_t width;
		uint16_t tankWidth;
		uint16_t scratchWidth;
		uint32_t row;
		uint32_t tankRow;
		uint32_t scratchRow;

		row = this->curRow;
		width = this->width;
		tankRow = this->curRow;
		tankWidth = this->width;
		scratchRow = this->curRow;
		scratchWidth = this->width;

		CHeatCorrProc_PreReadLine(this, (uint16_t *)(this->tankRowSrc + (tankRow * tankWidth)),
					  this->heatCorrection.useHeatCorrection);
		CHeatCorrProc_CorrectEx(this,(uint16_t *)(this->tankBuf + (row * width)),
			  (uint16_t *)(this->tankRowSrc + (tankRow * tankWidth)),
			  scratchData + (scratchRow * scratchWidth));
		CHeatCorrProc_HeatTransferProc(this);
	}

	memcpy(destData, this->tankBuf, pixels * 2);
	rval = 1;
done:

	if (this->tankBuf) {
		free(this->tankBuf);
		this->tankBuf = NULL;
	}
	for (i = 0 ; i < 2 ; i++) {
		if (this->tankRowPtrs[i]) {
			free(this->tankRowPtrs[i]);
			this->tankRowPtrs[i] = NULL;
		}
	}
	for (i = 0 ; i < 5 ; i++) {
		if (this->tankRowBufs[i]) {
			free(this->tankRowBufs[i]);
			this->tankRowBufs[i] = NULL;
		}
	}
	return rval;
}

/*** CImageProc ***/

static void CImageProc_Init(struct CImageProc *this, struct CIppMng *ippMng)

{
	memset(this, 0, sizeof(*this));
	this->ippMng = ippMng;
	return;
}

static bool CImageProc_Initialize(struct CImageProc *this)
{
	this->pixels = 0;

	CIppMng_GetIppConf(this->ippMng, &this->conf, &this->planesOCG, &this->planesOCM);

	/* Sanity checks */
	if (!this->conf.width || !this->conf.height || !this->conf.headWidth ||
	    !this->planesOCG || !this->planesOCM)
		return 0;

	/* If this is a 5x7 print.. */
	if (this->conf.borderCapable == 0x02 &&
	    this->conf.width == 1548 &&
	    this->conf.height == 2140) {
		this->srcWidth = this->conf.width;
		this->srcHeight = this->conf.height;
		this->conf.width = 1844;
		this->conf.height = 2434;
	}
	this->correctMtf  = this->conf.corrections1 & 1;
	this->correctHeat = this->conf.corrections2 & 1;
	this->correctLine = (this->conf.corrections2 & 2) >> 1;
	this->correctSideEdge = (this->conf.corrections2 & 4) >> 2;

	this->pixelsOCG = this->conf.width * this->conf.height * this->planesOCG;
	this->pixelsOCM = this->conf.width * this->conf.height * this->planesOCM;

	this->imageScratchDataPtr = malloc(this->pixelsOCM);
	if (!this->imageScratchDataPtr)
		return 0;

	this->pixels = this->conf.width * this->conf.height;
	this->imageDataLen = this->pixels * this->planesOCM * 2;
	this->imageDataPtr = malloc(this->imageDataLen);
	if (!this->imageDataPtr) {
		free(this->imageScratchDataPtr);
		this->imageScratchDataPtr = NULL;
		return 0;
	}
	memset(this->imageDataPtr, 0, this->imageDataLen);

	return 1;
}

static int CImageProc_CropPicDotSeq(struct CImageProc *this, uint8_t *destPtr,
				    struct pic_data *picData, struct crop_conf *cropConf)

{
	uint16_t cols, rows;
	uint8_t bypp;
	uint8_t *scratch;
	int row;
	int offset;

	cols = cropConf->numCols;
	rows = cropConf->numRows;
	bypp = picData->bytes_pp;

	scratch = malloc(cols * rows * bypp);
	if (!scratch)
		return -1;

	for (row = 0, offset = 0; row < (cropConf->startRow + cropConf->numRows) ; row++) {
		int col;
		for (col = cropConf->startCol; col < (cropConf->startCol + cropConf->numCols) ; col++) {
			uint32_t srcOffset = picData->bytes_pp * (col + row * picData->outCols);
			int p;
			for (p = 0 ; p < picData->bytes_pp ; p ++) {
				scratch[offset] = picData->srcPtr[srcOffset];
				srcOffset++;
				offset++;
			}
		}
	}

	memcpy(destPtr, scratch, cols * rows * bypp);
	if (scratch)
		free(scratch);

	(void)this; /* Shut up, compiler */

	return 0;
}

static int CImageProc_AddBorderEx(struct CImageProc *this, uint8_t *destPtr, uint8_t *srcPtr,
				  struct pic_data *picData)

{
	uint32_t outBufLen;
	uint8_t  *outBuf;
	uint32_t outOffset, srcOffset;
	int row;

	if (picData->outCols < picData->inCols ||
	    picData->outRows < picData->inRows)
		return -2;

	outBufLen = picData->outCols * picData->outRows * picData->bytes_pp;
	outBuf = malloc(outBufLen);
	if (!outBuf)
		return -1;

	memset(outBuf, 0xff, outBufLen);

	srcOffset = 0;
	for (row = 0; row < picData->inRows ; row++) {
		int col;
		for (col = 0 ; col < picData->inCols ; col++) {
			int j;

			outOffset = picData->bytes_pp *
				(col + ((picData->outCols - picData->inCols) / 2) +
				 ((row + ((picData->outRows - picData->inRows) / 2)) *
				  picData->outCols));

			for (j = 0 ; j < picData->bytes_pp ; j++) {
			        outBuf[outOffset] = srcPtr[srcOffset];
				outOffset++;
				srcOffset++;
			}
		}

	}

	memcpy(destPtr, outBuf, outBufLen);
	free(outBuf);

	(void)this; /* Shut up, compiler */

	return 0;
}

static bool CImageProc_AddBorder(struct CImageProc *this, uint8_t *destPtr, uint8_t *srcRGBPtr)

{
	/* If this is a 5x7 print.. */
	if (this->conf.borderCapable == 0x02 &&
	    this->conf.width == 1548 &&
	    this->conf.height == 2140) {
		struct pic_data borderPicData;
		struct crop_conf cropConf;
		struct pic_data cropPicData;

		cropPicData.inCols = 0x60c;
		cropPicData.inRows = 0x85c;
		cropPicData.outCols = 0x60c;
		cropPicData.outRows = 0x85c;
		cropPicData.bytes_pp = 3;
		cropPicData.srcPtr = srcRGBPtr;

		cropConf.startCol = 0x0018;
		cropConf.startRow = 0x0012;
		cropConf.numCols = 0x05e0;
		cropConf.numRows = 0x0838;

		if (CImageProc_CropPicDotSeq(this, destPtr, &cropPicData, &cropConf))
			return 0;

		borderPicData.inCols = 0x5e0;
		borderPicData.inRows = 0x838;
		borderPicData.outCols = 0x734;
		borderPicData.outRows = 0x982;
		borderPicData.bytes_pp = 3;
		borderPicData.srcPtr = destPtr;
		if (CImageProc_AddBorderEx(this, destPtr, destPtr, &borderPicData))
			return 0;

	} else {
		memcpy(destPtr, srcRGBPtr, this->pixelsOCG);
	}

	return 1;
}

static bool CImageProc_TransDotToPlane(struct CImageProc *this, uint8_t *destPtr, uint8_t *srcPtr)
{
	uint8_t *workBuf;
	uint32_t offset;

	workBuf = malloc(this->pixelsOCG);
	if (!workBuf)
		return 0;

	for (offset = 0 ; offset < this->pixelsOCG ; offset++) {
		workBuf[((this->planesOCG - 1) - offset % this->planesOCG) *
			(this->pixelsOCG / this->planesOCG) + offset / this->planesOCG] = srcPtr[offset] ^ 0xff;
	}

	memcpy(destPtr, workBuf, this->pixelsOCG);
	free(workBuf);

	return 1;
}

static bool CImageProc_GetIppData(struct CImageProc *this, uint8_t plane)

{
	return CIppMng_GetIppData(this->ippMng, &this->planeIPPdata, plane);
}

static bool CImageProc_SetProcConfig(struct CImageProc *this, uint8_t plane)

{
	bool rval;
	uint8_t planeOffset;

	this->currentPlane = plane;
	rval = CIppMng_GetIppNo(this->ippMng, &planeOffset, this->currentPlane);
	if (rval)
		rval = CImageProc_GetIppData(this, planeOffset);

	return rval;
}


static bool CImageProc_MiddleDataGen(struct CImageProc *this, uint8_t plane)

{
	struct CMidDataGen midData;

	CMidDataGen_Init(&midData);

	return CMidDataGen_GenData(&midData, (uint8_t *)this->imageScratchDataPtr, plane,
				   this->conf.width, this->conf.height, &this->planeIPPdata);
}


static void CImageProc_MtfPreCalcTableGen(struct CImageProc *this)

{
	uint16_t uVar1;
	int16_t i;

	uVar1 = (this->planeIPPdata).mtfPreCalcCutoff;
	for (i = -0x100 ; i < 0x100 ; i++) { // XXX this is a 512 element array!
		if (i * i < uVar1 * uVar1) {
			this->mtfCorrPlaneTable[i + 0x100] = -i;
		} else {
			this->mtfCorrPlaneTable[i + 0x100] = i;
		}
	}
	return;
}

static bool CImageProc_MtfCorrPlaneEx(struct CImageProc *this, uint8_t *outData, uint8_t *inData)

{
	uint32_t planeSize;
	uint8_t *workBuf;
	int16_t *mtfCorrPlaneTable;
	uint16_t row;

	CImageProc_MtfPreCalcTableGen(this);
	planeSize = this->conf.width * this->conf.height;

	workBuf = malloc(planeSize);
	if (!workBuf)
		return 0;

	mtfCorrPlaneTable = &this->mtfCorrPlaneTable[256]; /* Mid-point of array */
	memcpy(workBuf, inData, planeSize);

	for (row = 1 ; row < (this->conf.height - 1) ; row++) {
		uint16_t col;
		for (col = 1 ; col < (this->conf.width - 1) ; col++) {
			uint32_t rowOffset = col + row * this->conf.width;
			uint8_t *inPixel = inData + rowOffset;

			int outVal;
			uint8_t outPixel;

			// (curPixel + ((diff_prevRowPixel + diff_prevColPixel) * prevWeight) + ((diff_nextRowPixel + diff_nextColPixel) * nextWeight) / 128

			outVal = *inPixel +
				(((mtfCorrPlaneTable[*inPixel - inPixel[-this->conf.width]]
				   + mtfCorrPlaneTable[*inPixel - inPixel[this->conf.width]]) * (this->planeIPPdata).prevWeight
				  + (mtfCorrPlaneTable[(*inPixel - inPixel[-1])]
				     + mtfCorrPlaneTable[(*inPixel - inPixel[1])]) * this->planeIPPdata.nextWeight) >> 7);

			outPixel = outVal;
			if (outVal < 0x100) {
				if (outVal < 1) {
					outPixel = 1;
				}
			} else {
				outPixel = 0xff;
			}
			workBuf[rowOffset] = outPixel;
		}
	}


	memcpy(outData, workBuf, planeSize);
	free(workBuf);

	return 1;
}


static bool CImageProc_MtfCorrPlane(struct CImageProc *this)
{
	uint32_t offset;

	if (!this->correctMtf)
		return 1;

	offset = this->pixels * this->currentPlane;

	return CImageProc_MtfCorrPlaneEx(this, ((uint8_t *)this->imageScratchDataPtr) + offset,
					 ((uint8_t *)this->imageScratchDataPtr) + offset);
}

static int32_t CImageProc_TransPlaneToPulseEx(struct CImageProc *this, uint16_t *outPtr, uint8_t *inPtr)

{
	int32_t offset;

	for (offset = 0 ; offset < this->conf.width * this->conf.height ; offset++) {
		outPtr[offset] = this->planeIPPdata.pulseExMap[inPtr[offset]];
	}
	return offset;
}

static void CImageProc_TransPlaneToPulse(struct CImageProc *this)

{
	uint32_t offset = this->pixels * this->currentPlane;

	CImageProc_TransPlaneToPulseEx(this,this->imageDataPtr + offset,
				       ((uint8_t *)this->imageScratchDataPtr) + offset);
	return;

}

static uint16_t CImageProc_GetMaxPulse(struct CImageProc *this)

{
	return (this->planeIPPdata.field_0x0 * (this->planeIPPdata.field_0x2 + 1)) + this->planeIPPdata.field_0x2;
}

static bool CImageProc_HeatCorrection(struct CImageProc *this)

{
	struct CHeatCorrProc heatProc;
	uint32_t offset;
	uint16_t maxPulse;
	bool rval;

	/* Don't correct heat transfer if so requested */
	if (!this->correctHeat)
		return 1;

	CHeatCorrProc_Init(&heatProc);

	offset = this->pixels * this->currentPlane;
	maxPulse = CImageProc_GetMaxPulse(this);

	rval = CHeatCorrProc_SetIppData(&heatProc, &this->planeIPPdata,
					this->conf.width, this->conf.height,
					this->conf.headWidth, maxPulse);
	if (!rval)
		return 0;


	CHeatCorrProc_Correction(&heatProc,
				 this->imageDataPtr + offset,
				 this->imageDataPtr + offset,
				 ((uint8_t *)this->imageScratchDataPtr) + offset);

	return 1;
}

static bool CImageProc_LineCorrEx(struct CImageProc *this, uint16_t *outDataPtr, uint16_t *inData, uint8_t *scratchData)

{
	uint32_t pixels;
	uint16_t *workBuf;
	uint16_t minRowSum;
	uint16_t maxPulse;
	int32_t  pageSum;
	uint32_t lineCorrectionFactor;
	uint16_t *workPtr;
	uint8_t  *scratchPtr;
	uint16_t *inPtr;
	int32_t  maxLineSum;
	int32_t  rowSum;

	uint16_t row;

	pixels = this->conf.width * this->conf.height;

	workBuf = malloc(pixels * 2);
	if (!workBuf)
		return 0;

	pageSum = 0;
	lineCorrectionFactor = 0;
	maxLineSum = this->conf.width * this->planeIPPdata.maxLineSum;
	minRowSum = this->planeIPPdata.minRowSum;
	maxPulse = CImageProc_GetMaxPulse(this);
	workPtr = workBuf;
	scratchPtr = scratchData;
	inPtr = inData;

	for (row = 0 ; row < this->conf.height ; row ++) {
		uint16_t col;
		uint16_t local_54;
		int32_t iVar3;

		rowSum = 0;
		for (col = 0 ; col < this->conf.width ; col++) {
			rowSum = *inPtr + rowSum;
			iVar3 = *inPtr - (((this->planeIPPdata).lineCorrection[*scratchPtr] * lineCorrectionFactor) >> 10);
			if (iVar3 < 0) {
				local_54 = 0;
			} else {
				local_54 = iVar3;
				if (maxPulse < iVar3) {
					local_54 = maxPulse;
				}
			}
			*workPtr = local_54;
			workPtr++;
			inPtr++;
			scratchPtr++;
		}

		if ((this->conf.width * minRowSum) < rowSum) {
			pageSum = rowSum + pageSum;
			if (maxLineSum < pageSum) {
				pageSum -= maxLineSum;
				lineCorrectionFactor++;
				if (this->planeIPPdata.maxLineCorrectionFactor < lineCorrectionFactor) {
					lineCorrectionFactor = this->planeIPPdata.maxLineCorrectionFactor;
				}
			}
		}
	}

	memcpy(outDataPtr, workBuf, pixels * 2);
	free(workBuf);

	return 1;
}

static bool CImageProc_LineCorrection(struct CImageProc *this)
{
	uint32_t offset;

	if (!this->correctLine)
		return 1;

	offset = this->pixels * this->currentPlane;

	return CImageProc_LineCorrEx(this, this->imageDataPtr + offset, this->imageDataPtr + offset,
				     (uint8_t*)this->imageScratchDataPtr + offset);
}

static bool CImageProc_SideEdgeCorrEx(struct CImageProc *this, uint16_t *outDataPtr, uint16_t *inDataPtr, uint8_t *scratchDataPtr)

{
	uint32_t pixels = this->conf.width * this->conf.height;
	uint16_t *workBuf, *workPtr;
	uint16_t maxPulse;
	uint8_t *scratchPtr;
	uint16_t *inPtr;

	int col;
	int row;

	workBuf = malloc(pixels * 2);
	if (!workBuf)
		return 0;

	maxPulse = CImageProc_GetMaxPulse(this);

	workPtr = workBuf;
	scratchPtr = scratchDataPtr;
	inPtr = inDataPtr;

	for (row = 0 ; row < this->conf.height ; row++) {
		for (col = 0 ; col < this->conf.width ; col++) {
			uint32_t corrVal;
			uint16_t inPixel;
			int32_t iVar3;

			if (col < 0x40) {
				corrVal = this->planeIPPdata.sideEdgeCorrectionComp[col];
			} else {
				if ((this->conf.width - 0x40) < col) {
					corrVal = this->planeIPPdata.sideEdgeCorrectionComp[(this->conf.width - col) -1];
				} else {
					corrVal = 0;
				}
			}
			inPixel = *inPtr;
			if (corrVal != 0) {
				iVar3 = *inPtr - (((this->planeIPPdata).sideEdgeCorrectionTable[*scratchPtr] * corrVal) >> 7);
				if (iVar3 < 0) {
					inPixel = 0;
				} else {
					inPixel = iVar3;
					if (maxPulse < iVar3) {
						inPixel = maxPulse;
					}
				}
			}
			*workPtr = inPixel;
			workPtr++;
			inPtr++;
			scratchPtr++;
		}
	}

	memcpy(outDataPtr, workBuf, pixels * 2);
	free(workBuf);
	return 1;
}

static bool CImageProc_SideEdgeCorrection(struct CImageProc *this)

{
	uint32_t offset;

	if (!this->correctSideEdge)
		return 1;

	offset = this->pixels * this->currentPlane;

	return CImageProc_SideEdgeCorrEx(this, this->imageDataPtr + offset, this->imageDataPtr + offset,
					 ((uint8_t*)this->imageScratchDataPtr + offset));
}

static bool CImageProc_PulseGenEx(struct CImageProc *this)

{
	bool rval;

	rval = CImageProc_MtfCorrPlane(this);
	if (rval) {
		CImageProc_TransPlaneToPulse(this);
		rval = CImageProc_HeatCorrection(this);
	}
	if (rval)
		rval = CImageProc_LineCorrection(this);
	if (rval)
		rval = CImageProc_SideEdgeCorrection(this);

	return rval;
}


static bool CImageProc_PulseGen(struct CImageProc *this, uint16_t *outImgPtr, uint8_t *srcRGB)
{
	bool rval;
	uint32_t i;

	rval = CImageProc_Initialize(this);
	if (rval)
		rval = CImageProc_AddBorder(this, (uint8_t*)this->imageScratchDataPtr, srcRGB);
	if (rval)
		rval = CImageProc_TransDotToPlane(this, (uint8_t*)this->imageScratchDataPtr,
						  (uint8_t*)this->imageScratchDataPtr);
	if (!rval)
		goto done;

	for (i = 0 ; i < this->planesOCM ; i++) {
		rval = CImageProc_SetProcConfig(this, i);
		if (!rval)
			goto done;

		/* Generate the lamination plane as needed */
		if ((this->planesOCG <= i) &&
		     !(rval = CImageProc_MiddleDataGen(this, i)))
			goto done;
		if (!(rval = CImageProc_PulseGenEx(this)))
			goto done;
	}

#if (__BYTE_ORDER != __LITTLE_ENDIAN)
	for (i = 0 ; i < this->imageDataLen / 2 ; i++) {
		outImgPtr[i] = cpu_to_le16(this->imageDataPtr[i]);
	}
#else
	memcpy(outImgPtr, this->imageDataPtr, this->imageDataLen);
#endif

done:
	if (this->imageScratchDataPtr) {
		free(this->imageScratchDataPtr);
		this->imageScratchDataPtr = NULL;
	}
	if (this->imageDataPtr) {
		free(this->imageDataPtr);
		this->imageDataPtr = NULL;
	}
	return rval;
}

//-------------------------------------------------------------------------
// Exported Functions

bool ip_imageProc(uint16_t *destData, uint8_t *srcInRgb,
		  uint16_t width, uint16_t height, void *srcIpp)
{
	bool rval = 0;
	struct CIppMng ippMng;
	struct CImageProc imageProc;

	fprintf(stderr, "INFO: libS2245ImageReProcess version '%s'\n", LIB_VERSION);
	fprintf(stderr, "INFO: Copyright (c) 2020 Solomon Peachy\n");
	fprintf(stderr, "INFO: This free software comes with ABSOLUTELY NO WARRANTY!\n");
	fprintf(stderr, "INFO: Licensed under the GNU GPLv3.\n");
	fprintf(stderr, "INFO: *** This code is NOT supported or endorsed by Sinfonia! ***\n");

	CIppMng_Init(&ippMng);
	CImageProc_Init(&imageProc, &ippMng);

	if (!width || !height || !srcIpp || !srcInRgb || !destData)
		goto done;

	rval = CIppMng_SetIPP(&ippMng, srcIpp, width, height);
	if (rval)
		rval = CImageProc_PulseGen(&imageProc, destData, srcInRgb);

done:
	CIppMng_Cleanup(&ippMng);

	return rval;
}

bool ip_checkIpp(uint16_t width, uint16_t height, void *srcIpp)
{
	bool rval = 0;
	struct CIppMng ippMng;

	if (!width || !height || !srcIpp)
		goto done;

	CIppMng_Init(&ippMng);
#ifdef USE_EXTRA_STUFF
	rval = CIppMng_SetConf(&ippMng, srcIpp, width, height);
	if (!rval)
		goto done;
#endif
	rval = CIppMng_CheckIpp(&ippMng, srcIpp);

	CIppMng_Cleanup(&ippMng);

done:
	return rval;
}

bool ip_getMemorySize(uint32_t *szMemory,
		      uint16_t width, uint16_t height,
		      void *srcIpp)
{
	struct ippData *ippData;
	uint32_t  pixels;

	if (!width || !height || !szMemory || !srcIpp)
		return 0;

	ippData = srcIpp;

	if ((ippData->conf.borderCapable == 0x02) &&
	    (width == 1548) && (height == 2140)) {
		pixels = 2434*1844;  /* ie 8x6 */
	} else {
		pixels = height * width;
	}

	*szMemory = pixels * 2 * ippData->conf.planes;

	if (!*szMemory)
		return 0;

	return 1;
}
