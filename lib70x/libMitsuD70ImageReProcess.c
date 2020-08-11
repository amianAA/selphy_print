/* LibMitsuD70ImageReProcess -- Re-implemented image processing library for
                                the Mitsubishi CP-D70 family of printers

   Copyright (c) 2016-2020 Solomon Peachy <pizza@shaftnet.org>

   ** ** ** ** Do NOT contact Mitsubishi about this library! ** ** ** **

   This library is a platform-independent reimplementation of the image
   processing algorithms that are necessary to utilize most newer
   Mitsubishi photo printers.

   Mitsubishi was *NOT* involved in the creation of this library, and is
   not responsible in any way for the library or any deficiencies in its
   output.  They will provide no support if it is used.

   However, without this library, it is nearly impossible to utilize
   their these printers under Linux and similar operating systems.

   The following printers are known to function with this library:

     * Mitsubishi CP-D70DW
     * Mitsubishi CP-D707DW
     * Mitsubishi CP-K60DW-S
     * Mitsubishi CP-D80DW
     * Kodak 305
     * Fujifilm ASK-300
     * Mitsubishi CP-M1
     * Mitsubishi CP-M15

   More recently, the CP98xx family now uses this library.  These
   models are expected to function:

     * Mitsubishi CP9800DW
     * Mitsubishi CP9810DW
     * Mitsubishi CP9820DW-S

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

#define LIB_VERSION "0.9.4"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libMitsuD70ImageReProcess.h"

#define UNUSED(expr) do { (void)(expr); } while (0)
#define STATIC_ASSERT(test_for_true) _Static_assert((test_for_true), "(" #test_for_true ") failed")

//-------------------------------------------------------------------------
// Endian Manipulation macros
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
#define le16_to_cpu(__x) __x
#define le32_to_cpu(__x) __x
#define be16_to_cpu(__x) __builtin_bswap16(__x)
#define be32_to_cpu(__x) __builtin_bswap32(__x)

#define le64d_to_cpu(__x) __x
#define be64d_to_cpu(__x) swab_double(__x)
#else
#define le16_to_cpu(__x) __builtin_bswap16(__x)
#define le32_to_cpu(__x) __builtin_bswap32(__x)
#define be16_to_cpu(__x) __x
#define be32_to_cpu(__x) __x

#define le64d_to_cpu(__x) swab_double(__x)
#define be64d_to_cpu(__x) __x
#endif

#define cpu_to_le16 le16_to_cpu
#define cpu_to_le32 le32_to_cpu
#define cpu_to_be16 be16_to_cpu
#define cpu_to_be32 be32_to_cpu
#define cpu_to_le64d le64d_to_cpu
#define cpu_to_be64d be64d_to_cpu

//-------------------------------------------------------------------------
// Data declarations

#define CPC_DATA_ROWS 2730

#define CHUNK_LEN (256*1024)

struct CColorConv3D {
	uint8_t lut[17][17][17][3];
};

/* State for image processing algorithm */
/* Note: pixel data is always ordered YMC! */
struct CImageEffect70 {
	uint32_t pad;            // @0
	  double *ttd_htd_scratch;  // @4/1      // array [(cols+6) * 3], single row, plus padding.  processing buffer from TTD->HTD
	  double *ttd_htd_first; // @8/2         // first pixel of ttd_htd_scratch
	  double *ttd_htd_last;  // @12/3        // last pixel of ttd_htd_scratch
	  double *htd_ttd_next;  // @16/4        // array [band_pixels], state from HTD->TTDnext.
	  double fcc_ymc_scale[3];  // @20/5     // FCC generates, YMC consumes. per-color scaling factor for thermal compensation.
	uint32_t htd_fcc_scratch[3][128];  // @44/11    // per-color state from HTD->FCC
	  double fcc_ymc_scratch[3][128];  // @1580/395 // per-color state from FCC->YMC6
	  double *fcc_rowcomps;  // @4652/1163   // array of [row_count][3], Per-row/color correction factor?  Used internally by FCC code.
	uint16_t *linebuf;       // @4656/1164   // array of [11 * sizeof(uint16_t) * linebuf_stride], Historical line buffer
	uint16_t *linebuf_row[11];  // @4660/1165   // Pointers into rows in line buffer, minus padding!
	uint16_t *linebuf_line[11]; // @4704/1176   // Pointers to raw rows in line buffer (w/ padding on either side)
	uint16_t *linebuf_shrp[8];  // @4748/1187   // Pointers into line buffer, for pixels used for sharpening
	struct CPCData *cpc;     // @4780/1195   // Loaded from disk..
	 int32_t sharpen;        // @4784/1196   // -1 off, 0-8, "normal" is 4.
	uint32_t columns;        // @4788/1197   // columns in image
	uint32_t rows;           // @4792/1198   // rows in image
	uint32_t pixel_count;    // @4796/1199   // pixels per input band (ie input stride / 2)
	uint32_t cur_row;        // @4800/1200   // row index.
	uint32_t band_pixels;    // @4804/1201   // pixels per output band (always columns * 3)
	uint32_t linebuf_stride; // @4808/1202   // band_pixels + 6 -- line buffer row stride
	double   fhdiv_up;       // @4812/1203   // FH[0]  // division factor for positive comp
	double   fhdiv_dn;       // @4820/1205   // FH[1]  // divison factor for negative comp
	double   fh_cur;         // @4828/1207   // FH[2]
	double   fh_prev1;       // @4836/1209   // FH[3] - FH[2]
	double   fh_prev2;       // @4844/1211   // FH[4] - FH[3]
	double   fh_prev3;       // @4852/1213   // FH[4]
	                         // @4860/1215
};

/* The parsed data out of the CPC files */
struct CPCData {
	/* One per output row, Used for HTD. */
	uint32_t LINEy[2730];    // @0      // can be uint16?
	uint32_t LINEm[2730];    // @10920  // can be uint16?
	uint32_t LINEc[2730];    // @21840  // can be uint16?
	/* Maps input color to gamma-corrected 16bpp inverse */
	uint16_t GNMby[256];     // @32760  // Gamma map Blue->Yellow
	uint16_t GNMgm[256];     // @33272  // Gamma map Green->Magenta
	uint16_t GNMrc[256];     // @33784  // Gamma map Red->Cyan
	/* Used for FCC */
	double   FM[256];        // @34296
	/* Used for TTD */
	double   KSP[128];       // @36344
	double   KSM[128];       // @37368
	double   OSP[128];       // @38392
	double   OSM[128];       // @39416
	double   KP[11];         // @40440 // weights for line buffer!
	double   KM[11];         // @40528 // weights for line buffer!
	/* Used for HTD */
	double   HK[4];          // @40616

	uint32_t Speed[3];       // @40648 -- Unused!
	double   FH[5];          // @40660
	/* Used for sharpening */
	double   SHK[72];        // @40700 // sharpening coefficients, actually double[9][8]
	/* Used for YMC6 */
	double   UH[101];        // @41276

	/* Used by roller mark correction (K60/D80/EK305) -- Unused! */
	uint32_t ROLK[13];       // @42084
	/* Used by reverse/skip logic (K60/D80/EK305) */
	 int32_t REV[76];        // @42136 // Actually int32_t[4][19]
	                         // @42440
};

/*** Version ***/
int lib70x_getapiversion(void)
{
	return LIB_APIVERSION;
}

/*** 3D color Lookup table ****/

/* Load the Lookup table off of disk into *PRE-ALLOCATED* buffer */
int CColorConv3D_Get3DColorTable(uint8_t *buf, const char *filename)
{
	FILE *stream;
	int rval;

	if (!filename)
		return 1;
	if (!*filename)
		return 2;
	if (!buf)
		return 3;

	stream = fopen(filename, "rb");
	if (!stream)
		return 4;

	fseek(stream, 0, SEEK_END);
	if (ftell(stream) < LUT_LEN) {
		fclose(stream);
		return 5;
	}
	fseek(stream, 0, SEEK_SET);
	rval = fread(buf, LUT_LEN, 1, stream);
	fclose(stream);

	return (rval != 1);
}

/* Parse the on-disk LUT data into the structure.... */
struct CColorConv3D *CColorConv3D_Load3DColorTable(const uint8_t *ptr)
{
	struct CColorConv3D *this;
	this = malloc(sizeof(*this));
	if (!this)
		return NULL;

	int i, j, k;

	for (i = 0 ; i <= 16 ; i++) {
		for (j = 0 ; j <= 16 ; j++) {
			for (k = 0; k <= 16; k++) {
				this->lut[k][j][i][2] = *ptr++;
				this->lut[k][j][i][1] = *ptr++;
				this->lut[k][j][i][0] = *ptr++;
			}
		}
	}
	return this;
}
void CColorConv3D_Destroy3DColorTable(struct CColorConv3D *this)
{
	free(this);
}

/* Transform a single pixel. */
static void CColorConv3D_DoColorConvPixel(struct CColorConv3D *this, uint8_t *redp, uint8_t *grnp, uint8_t *blup)
{
	int red_h;
	int grn_h;
	int blu_h;
	int grn_li;
	int red_li;
	int blu_li;
	int red_l;
	int grn_l;
	int blu_l;

	uint8_t *tab0;       // @ 14743
	uint8_t *tab1;       // @ 14746
	uint8_t *tab2;       // @ 14749
	uint8_t *tab3;       // @ 14752
	uint8_t *tab4;       // @ 14755
	uint8_t *tab5;       // @ 14758
	uint8_t *tab6;       // @ 14761
	uint8_t *tab7;       // @ 14764

	red_h = *redp >> 4;
	red_l = *redp & 0xF;
	red_li = 16 - red_l;

	grn_h = *grnp >> 4;
	grn_l = *grnp & 0xF;
	grn_li = 16 - grn_l;

	blu_h = *blup >> 4;
	blu_l = *blup & 0xF;
	blu_li = 16 - blu_l;

	tab0 = this->lut[red_h+0][grn_h+0][blu_h+0];
	tab1 = this->lut[red_h+1][grn_h+0][blu_h+0];
	tab2 = this->lut[red_h+0][grn_h+1][blu_h+0];
	tab3 = this->lut[red_h+1][grn_h+1][blu_h+0];
	tab4 = this->lut[red_h+0][grn_h+0][blu_h+1];
	tab5 = this->lut[red_h+1][grn_h+0][blu_h+1];
	tab6 = this->lut[red_h+0][grn_h+1][blu_h+1];
	tab7 = this->lut[red_h+1][grn_h+1][blu_h+1];

	*redp = (blu_li
		 * (grn_li * (red_li * tab0[0] + red_l * tab1[0])
		    + grn_l * (red_li * tab2[0] + red_l * tab3[0]))
		 + blu_l
		 * (grn_li * (red_li * tab4[0] + red_l * tab5[0])
		    + grn_l * (red_li * tab6[0] + red_l * tab7[0]))
		 + 2048) >> 12;
	*grnp = (blu_li
		 * (grn_li * (red_li * tab0[1] + red_l * tab1[1])
		    + grn_l * (red_li * tab2[1] + red_l * tab3[1]))
		 + blu_l
		 * (grn_li * (red_li * tab4[1] + red_l * tab5[1])
		    + grn_l * (red_li * tab6[1] + red_l * tab7[1]))
		 + 2048) >> 12;
	*blup = (blu_li
		 * (grn_li * (red_li * tab0[2] + red_l * tab1[2])
		    + grn_l * (red_li * tab2[2] + red_l * tab3[2]))
		 + blu_l
		 * (grn_li * (red_li * tab4[2] + red_l * tab5[2])
		    + grn_l * (red_li * tab6[2] + red_l * tab7[2]))
		 + 2048) >> 12;
}

/* Perform a total conversion on an entire image */
void CColorConv3D_DoColorConv(struct CColorConv3D *this, uint8_t *data, uint16_t cols, uint16_t rows, uint32_t stride, int rgb_bgr)
{
	uint16_t i, j;

	uint8_t *ptr;

	for ( i = 0; i < rows ; i++ )
	{
		ptr = data;
		for ( j = 0; cols > j; j++ )
		{
			if (rgb_bgr) {
				CColorConv3D_DoColorConvPixel(this, ptr + 2, ptr + 1, ptr);
			} else {
				CColorConv3D_DoColorConvPixel(this, ptr, ptr + 1, ptr + 2);
			}
			ptr += 3;
		}
		data += stride;
	}
}

/*** CPC Data ***/

/* Load and parse the CPC data */
struct CPCData *get_CPCData(const char *filename)
{
	struct CPCData *data;
	FILE *f;
	char buf[4096];
	int line;
	char *ptr;

	const char *delim = " ,\t\n";

	if (!filename)
		return NULL;
	data = malloc(sizeof(struct CPCData));
	if (!data)
		return NULL;

	f = fopen(filename, "r");
	if (!f)
		goto done_free;

	/* Skip the first two rows */
	for (line = 0 ; line < 2 ; line++) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			goto abort;
	}

	/* Init the REV and ROLK first rows */
	data->REV[0] = 0;
	data->ROLK[0] = 0;

	/* Start reading in data */
	for (line = 0 ; line < CPC_DATA_ROWS ; line++) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			goto abort;
		ptr = strtok(buf, delim);  // Always skip first column
		if (!ptr)
			goto abort;
		if (line < CPC_DATA_ROWS) {
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->LINEy[line] = strtol(ptr, NULL, 10);
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->LINEm[line] = strtol(ptr, NULL, 10);
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->LINEc[line] = strtol(ptr, NULL, 10);
		}
		if (line < 256) {
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->GNMby[line] = strtol(ptr, NULL, 10);
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->GNMgm[line] = strtol(ptr, NULL, 10);
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->GNMrc[line] = strtol(ptr, NULL, 10);
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->FM[line] = strtod(ptr, NULL);
		}
		if (line < 128) {
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->KSP[line] = strtod(ptr, NULL);
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->KSM[line] = strtod(ptr, NULL);
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->OSP[line] = strtod(ptr, NULL);
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->OSM[line] = strtod(ptr, NULL);
		}
		if (line < 11) {
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->KP[line] = strtod(ptr, NULL);
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->KM[line] = strtod(ptr, NULL);
		}
		if (line < 4) {
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->HK[line] = strtod(ptr, NULL);
		}
		if (line < 3) {
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->Speed[line] = strtol(ptr, NULL, 10);
		}
		if (line < 5) {
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->FH[line] = strtod(ptr, NULL);
		}
		if (line < 72) {
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->SHK[line] = strtod(ptr, NULL);
		}
		if (line < 101) {
			ptr = strtok(NULL, delim);
			if (!ptr)
				goto abort;
			data->UH[line] = strtod(ptr, NULL);
		}
		if (line < 13) { // ROLK, not present on D70
			ptr = strtok(NULL, delim);
			if (!ptr)
				continue;
			data->ROLK[line] = strtol(ptr, NULL, 10);

		}
		if (line < 76) { // REV, not present on D70
			ptr = strtok(NULL, delim);
			if (!ptr)
				continue;
			data->REV[line] = strtol(ptr, NULL, 10);
		}
	}

	fclose(f);
	return data;

abort:
	fclose(f);
done_free:
	free(data);
	return NULL;
}

void destroy_CPCData(struct CPCData *data) {
	free(data);
}

/*** Image Processing ***/
static struct CImageEffect70 *CImageEffect70_Create(struct CPCData *cpc)
{
	struct CImageEffect70 *data = malloc(sizeof (struct CImageEffect70));
	if (!data)
		return NULL;

	memset(data, 0, sizeof(*data));
	data->sharpen = -1;
	data->fhdiv_up = 1.0;
	data->fhdiv_dn = 1.0;
	data->cpc = cpc;
	return data;
}

static void CImageEffect70_Destroy(struct CImageEffect70 *data)
{
	free(data);
}

static void CImageEffect70_InitMidData(struct CImageEffect70 *data)
{
	data->ttd_htd_first = NULL;
	data->ttd_htd_last = NULL;
	data->ttd_htd_scratch = NULL;
	data->htd_ttd_next = NULL;
	data->fcc_rowcomps = NULL;
	data->linebuf = NULL;

	data->fcc_ymc_scale[0] = 1.0;
	data->fcc_ymc_scale[1] = 1.0;
	data->fcc_ymc_scale[2] = 1.0;

	memset(data->linebuf_row, 0, sizeof(data->linebuf_row));
	memset(data->linebuf_line, 0, sizeof(data->linebuf_line));
	memset(data->htd_fcc_scratch, 0, sizeof(data->htd_fcc_scratch)); // redundant
	memset(data->fcc_ymc_scratch, 0, sizeof(data->fcc_ymc_scratch)); // redundant
}

static void CImageEffect70_CreateMidData(struct CImageEffect70 *data)
{
	int i;

	data->ttd_htd_scratch = malloc(sizeof(double) * 3 * (data->columns + 6));
	memset(data->ttd_htd_scratch, 0, (sizeof(double) * 3 * (data->columns + 6)));
	data->ttd_htd_first = data->ttd_htd_scratch + 9;
	data->ttd_htd_last = data->ttd_htd_first + 3 * (data->columns - 1);
	data->htd_ttd_next = malloc(sizeof(double) * data->band_pixels);
	memset(data->htd_ttd_next, 0, (sizeof(double) * data->band_pixels));
	data->fcc_rowcomps = malloc(3 * sizeof(double) * data->rows);
	memset(data->fcc_rowcomps, 0, (3 * sizeof(double) * data->rows));
	data->linebuf_stride = data->band_pixels + 6;
	data->linebuf = malloc(11 * sizeof(uint16_t) * data->linebuf_stride);
	memset(data->linebuf, 0, (11 * sizeof(uint16_t) * data->linebuf_stride));
	data->linebuf_line[0] = data->linebuf;
	data->linebuf_row[0] = data->linebuf_line[0] + 3; // ie 6 bytes.

	for (i = 1 ; i < 11 ; i++ ) {
		data->linebuf_line[i] = data->linebuf_line[i-1] + data->linebuf_stride;
		data->linebuf_row[i] = data->linebuf_line[i] + 3; // ie 6 bytes
	}
	memset(data->htd_fcc_scratch, 0, sizeof(data->htd_fcc_scratch));
	memset(data->fcc_ymc_scratch, 0, sizeof(data->fcc_ymc_scratch));
}

static void CImageEffect70_DeleteMidData(struct CImageEffect70 *data)
{
	int i;

	if (data->ttd_htd_scratch) {
		free(data->ttd_htd_scratch);
		data->ttd_htd_scratch = NULL;
		data->ttd_htd_first = NULL;
		data->ttd_htd_last = NULL;
	}
	if (data->htd_ttd_next) {
		free(data->htd_ttd_next);
		data->htd_ttd_next = NULL;
	}
	if (data->fcc_rowcomps) {
		free(data->fcc_rowcomps);
		data->fcc_rowcomps = NULL;
	}
	if (data->linebuf) {
		free(data->linebuf);
		data->linebuf = NULL;
	}

	for (i = 0 ; i < 3 ; i++) {
		data->fcc_ymc_scale[i] = 0.0;
	}
	memset(data->linebuf_row, 0, sizeof(data->linebuf_row));
	memset(data->linebuf_line, 0, sizeof(data->linebuf_line));
	memset(data->htd_fcc_scratch, 0, sizeof(data->htd_fcc_scratch));
	memset(data->fcc_ymc_scratch, 0, sizeof(data->fcc_ymc_scratch));
}

static void CImageEffect70_Sharp_CopyLine(struct CImageEffect70 *data,
					  int offset, const uint16_t *row, int rownum)
{
	uint16_t *dst, *end;

	dst = data->linebuf_row[offset + 5]; /* Points at start of dst row */
	end = dst + 3 * data->columns; /* Point at end of dst row */

	memcpy(dst, row -(rownum * data->pixel_count), sizeof(uint16_t) * data->band_pixels);

	memcpy(dst - 3, dst, 6); /* Fill in dst row head */
	memcpy(end, end - 3, 6); /* Fill in dst row tail */
}

static void CImageEffect70_Sharp_PrepareLine(struct CImageEffect70 *data,
					     const uint16_t *row)
{
	int i;

	CImageEffect70_Sharp_CopyLine(data, 0, row, 0);
	for (i = 0 ; i < 5 ; i++) {
		memcpy(data->linebuf_line[i], data->linebuf_line[5], sizeof(uint16_t) * data->linebuf_stride);
	}
	for (i = 1 ; i <= 5 ; i++) {
		int rownum = data->rows -1;
		if (rownum < i)
			rownum = i;
		CImageEffect70_Sharp_CopyLine(data, i, row, rownum);
	}
}

static void CImageEffect70_Sharp_ShiftLine(struct CImageEffect70 *data)
{
	// XXX was memcpy, but src and dest definitely overlap!
	memmove(data->linebuf_line[0], data->linebuf_line[1], 10 * sizeof(uint16_t) * data->linebuf_stride);
}

/* Sets up reference pointers for the sharpening algorithm */
static void CImageEffect70_Sharp_SetRefPtr(struct CImageEffect70 *data)
{
	data->linebuf_shrp[0] = data->linebuf_row[4] - 3; // 6 bytes
	data->linebuf_shrp[1] = data->linebuf_row[4];
	data->linebuf_shrp[2] = data->linebuf_row[4] + 3; // 6 bytes
	data->linebuf_shrp[3] = data->linebuf_row[5] - 3; // 6 bytes
	data->linebuf_shrp[4] = data->linebuf_row[5] + 3; // 6 bytes
	data->linebuf_shrp[5] = data->linebuf_row[6] - 3; // 6 bytes
	data->linebuf_shrp[6] = data->linebuf_row[6];
	data->linebuf_shrp[7] = data->linebuf_row[6] + 3; // 6 bytes
}

/* Applies the final correction factor to a row. */
static void CImageEffect70_CalcYMC6(struct CImageEffect70 *data,
				    const double *in, uint16_t *imgdata)
{
	uint16_t i, j;
	uint32_t offset;
	double uh_val;

	/* Work out the UH value we need based on our row count */
	offset = data->rows - 1 - data->cur_row;
	if ( offset > 100 )
		offset = 100;
	uh_val = data->cpc->UH[offset];

	/* Now apply the final correction to each pixel in the row */
	offset = 0;
	for ( i = 0; i < data->columns; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			/* Processed input pixel * UH factor * per-color scaling * per-bucket scaling */
			double pixel = in[offset] * uh_val * data->fcc_ymc_scale[j] * data->fcc_ymc_scratch[j][((int)in[offset] >> 9)];
			if ( pixel > 65535.0)
				imgdata[offset] = 65535;
			else if ( pixel < 0.0)
				imgdata[offset] = 0;
			else
				imgdata[offset] = (int)pixel;
			++offset;
		}
	}
}

static void CImageEffect70_CalcFCC(struct CImageEffect70 *data)
{
	double s[3];
	double *row_comp;
	int i, j;
	double *prev1, *prev2, *prev3;

	/* Figure out where we need to be */
	row_comp = &data->fcc_rowcomps[3*data->cur_row];

	/* Initialize correction factors for this row based on the
	   buckets that CalcHTD handed us */
	for (j = 0 ; j < 3 ; j++) {
		row_comp[j] = 127 * data->htd_fcc_scratch[j][127];
	}
	for (i = 126 ; i >= 0 ; i--) {
		for (j = 0 ; j < 3 ; j++) {
			row_comp[j] += i * data->htd_fcc_scratch[j][i];
			data->htd_fcc_scratch[j][i] += data->htd_fcc_scratch[j][i+1];
		}
	}

	/* Set up pointers to previous rows. Or if we're on the first
	   three rows, take special action. */
	if (data->cur_row > 2) {
		prev1 = row_comp - 3;
		prev2 = row_comp - 6;
		prev3 = row_comp - 9;
	} else if (data->cur_row == 2) {
		prev1 = row_comp - 3;
		prev2 = row_comp - 6;
		prev3 = row_comp - 6;
	} else if (data->cur_row == 1) {
		prev1 = row_comp - 3;
		prev2 = row_comp - 3;
		prev3 = row_comp - 3;
	} else {
		prev1 = row_comp;
		prev2 = row_comp;
		prev3 = row_comp;
	}

	/* Work out the global color scaling factor for each color in the row */
	for (i = 0 ; i < 3 ; i++) {
		double val;
		/* Average it out over the number of columns */
		row_comp[i] /= data->columns;

		val = data->fh_cur * row_comp[i]
			+ data->fh_prev1 * prev1[i]
			+ data->fh_prev2 * prev2[i] // XXX this line is '-' in PPC versions, but + in x86.  Investigate WTF is going on.
			- data->fh_prev3 * prev3[i];
		/* Positive vs Negative values require different scaling factors */
		if (val >= 0.0) {
			data->fcc_ymc_scale[i] = val / data->fhdiv_up + 1.0;
		} else {
			data->fcc_ymc_scale[i] = val / data->fhdiv_dn + 1.0;
		}
	}

	/* Update the output buckets based on the input buckets plus
	   the FM correction factor */
	memset(s, 0, sizeof(s));
	for (i = 0 ; i < 128 ; i++) {
		for (j = 0 ; j < 3 ; j++) {
			int val = 255 * data->htd_fcc_scratch[j][i] / 1864;
			if (val > 255)
				val = 255;
			s[j] += data->cpc->FM[val];
			data->fcc_ymc_scratch[j][i] = s[j] / (i + 1);
		}
	}
}

/* Heat Transfer compensation (I think)

   Take the raw data
   add in the scaling factor from the adoining rows, using HK[]
   add in the fixed overhead from LINEy/m/c[]
   cap at 0-65535.

   Also populates htd_ttd_next, which informs the NEXT CalcTTD run what to do.
*/

static void CImageEffect70_CalcHTD(struct CImageEffect70 *data, const double *in, double *out)
{
	int32_t cur_row, offset;
	double *hk;
	double *last, *first;
	unsigned int i, k;
	uint32_t line_comp[3];

	hk = data->cpc->HK;
	first = data->ttd_htd_first;
	last = data->ttd_htd_last;

	/* Clean out correction buckets */
	memset(data->htd_fcc_scratch, 0, sizeof(data->htd_fcc_scratch));

	cur_row = data->cur_row;
	if (cur_row > 2729)
		cur_row = 2729;

	/* Fixed compensation per-line */
	line_comp[0] = data->cpc->LINEy[cur_row];
	line_comp[1] = data->cpc->LINEm[cur_row];
	line_comp[2] = data->cpc->LINEc[cur_row];

#if 0
	/* EK305 and K60 have only LINEy in their tables. Copy over to the others! */
	if (!line_comp[1]) line_comp[1] = line_comp[0];
	if (!line_comp[2]) line_comp[2] = line_comp[0];
#endif

	/* Fill in shoulders of the row */
	memcpy(first - 9, first, 0x18);   // Copy first pixel to pre-buffer
	memcpy(first - 6, first, 0x18);
	memcpy(first - 3, first, 0x18);
	memcpy(last + 3, last, 0x18);     // Copy last pixel to post-buffer
	memcpy(last + 6, last, 0x18);
	memcpy(last + 9, last, 0x18);

	/* Work out the compensation factors for each pixel in the row */
	offset = 0;
	for (i = 0; i < data->columns; i++) {
		for (k = 0; k < 3 ; k++) {
			int v11;

			/* Compute starting point for next TTD row, weighing the adjacent pixels based on the HK factor.. */
			data->htd_ttd_next[offset] = hk[0] * (first[offset] + first[offset]) +
				hk[1] * (first[offset - 3] + first[offset + 3]) +
				hk[2] * (first[offset - 6] + first[offset + 6]) +
				hk[3] * (first[offset - 9] + first[offset + 9]);

			/* Add in fixed per-line compensation */
			out[offset] = in[offset] + line_comp[k];

			/* Cap and Scale */
			v11 = out[offset];
			if ( out[offset] > 65535.0 ) {
				out[offset] = 65535.0;
				v11 = 127;
			} else if (out[offset] < 0.0) {
				out[offset] = 0.0;
				v11 = 0;
			} else {
				v11 >>= 9;
			}

			/* Increment buckets */
			data->htd_fcc_scratch[k][v11]++;
			offset++;
		}
	}
}

static void CImageEffect70_CalcTTD(struct CImageEffect70 *data,
				   const uint16_t *in, double *out)
{
	double *km, *kp, *osm, *osp, *ksm, *ksp;
	double *sharp = NULL;
	uint32_t i;
	ksp = data->cpc->KSP;  // KS Plus
	ksm = data->cpc->KSM;  // KS Minus
	osp = data->cpc->OSP;  // OS Plus
	osm = data->cpc->OSM;  // OS Minus
	kp = data->cpc->KP;    // K  Plus
	km = data->cpc->KM;    // K  Minus

	/* If we have sharpening turned on, set up the state */
	if (data->sharpen >= 0)
		sharp = &data->cpc->SHK[8 * data->sharpen];

	/* For each pixel in the row.. Note this is not color/plane dependent */
	for (i = 0 ; i < data->band_pixels ; i++) {
		double v4, v6, v7, input;
		int v29;
		double ks_comp_f, k_comp, ks_comp, os_comp, sharp_comp;
		int j, k;

		/* Starting point is the carry-over from the previous row minus
		   the new pixel */
		input = in[i];
		v7 = data->htd_ttd_next[i] - input;
		v29 = v7;
		if (v29 >= 0) {
			int v31 = 127;
			if (v29 <= 65535)
				v31 = v29 >> 9;
			ks_comp = ksp[v31];
		} else {
			int v30 = 127;
			if (-v29 <= 65535)
				v30 = -v29 >> 9;
			ks_comp = ksm[v30];
		}

		v6 = (v7 * ks_comp + input) - input;  /* This WTF add/subtract is present in every version I've looked at.  Leaving it here for completion's sake. */
		v29 = v6;
		if (v29 >= 0) {
			int v27 = 127;
			if (v29 <= 65535)
				v27 = v29 >> 9;
			os_comp = osp[v27];
		} else {
			int v26 = 127;
			if (-v29 <= 65535)
				v26 = -v29 >> 9;
			os_comp = osm[v26];
		}

		k_comp = 0.0;
		for ( j = 0 ; j < 11 ; j++) {
			int val;
			if (j == 5)
				continue;

			val = in[i] - data->linebuf_row[j][i];
			if (val >= 0)
				k_comp += kp[j] * val;
			else
				k_comp += km[j] * val;
		}

		sharp_comp = 0.0;
		if (sharp) {
			for (k = 0 ; k < 8 ; k++) {
				sharp_comp += sharp[k] * (in[i] - data->linebuf_shrp[k][i]);
			}
		}
		/* Update output state based on input plus the
		   various correction factors */
		out[i] = input - v6 * os_comp + k_comp + sharp_comp;

		/* Work out the state for HTD operation */
		v4 = data->htd_ttd_next[i] - out[i];
		v29 = v4;
		if ( v29 >= 0 ) {
			int v19 = 127;
			if ( v29 <= 65535 )
				v19 = v29 >> 9;
			ks_comp_f = ksp[v19];
		} else {
			int v18 = 127;
			if ( -v29 <= 65535 )
				v18 = -v29 >> 9;
			ks_comp_f = ksm[v18];
		}
		data->ttd_htd_first[i] = out[i] + v4 * ks_comp_f;
	}
}

/* Work out the number of times the density of a given color in
   a given area exceeds a threshold */
static void CImageEffect70_CalcSA(struct BandImage *img,
				  int invert, int32_t *in,
				  int32_t revX, int32_t *out)
{
	int cols, rows;
	int16_t *buf, *ptr;
	int stride;
	int start_row, row, start_col, col;

	cols = img->cols - img->origin_cols;
	rows = img->rows - img->origin_rows;

	if ( img->bytes_per_row >= 0 ) {
		if ( invert ) {
			stride = img->bytes_per_row >> 1;
			buf = (int16_t*)img->imgbuf + stride * (rows - 1);
		} else {
			stride = -img->bytes_per_row >> 1;
			buf = img->imgbuf;
		}
	} else {
		if ( invert ) {
			stride = img->bytes_per_row >> 1;
			buf = img->imgbuf;
		} else {
			stride = -img->bytes_per_row >> 1;
			buf = (int16_t*)img->imgbuf + stride * (rows - 1);
		}
	}

	start_col = in[0];
	start_row = in[1];
	if ( cols > in[2] )
		cols = in[2];
	if ( rows > in[3] )
		rows = in[3];
	if ( start_row < 0 )
		start_row = 0;
	if ( start_col < 0 )
		start_col = 0;

	out[2] = 0;
	out[1] = 0;
	out[0] = 0;

	ptr = buf - start_row * stride;
	for ( row = start_row ; row < rows ; row++ ) {
		int16_t *v18 = ptr + 3 * start_col;
		for ( col = start_col ; col < cols ; col++) {
			out[0] += (revX <= v18[0]);
			out[1] += (revX <= v18[1]);
			out[2] += (revX <= v18[2]);
			v18 += 3;
		}
		ptr -= stride;
	}
}

static int CImageEffect70_JudgeReverseSkipRibbon_int(struct BandImage *img,
						     int32_t *REV,
						     int invert)
{
	int32_t rows, cols;

	int j;

	rows = img->rows - img->origin_rows;
	cols = img->cols - img->origin_cols;

	/* Input rectangles: start_col, start_row, cols, rows */
	int32_t v16[4] = { REV[0], REV[2], REV[1], rows };
	int32_t v20[4] = { REV[1], 0, cols, rows };
	int32_t v24[4] = { 0, 0, REV[0], rows };
	int32_t v28[4] = { REV[0], 0, REV[1], REV[2] };

	/* Output buffers */
	int32_t v32[3] = { 0, 0, 0 };
	int32_t v35[3] = { 0, 0, 0 };
	int32_t v38[3] = { 0, 0, 0 };
	int32_t v41[3] = { 0, 0, 0 };

	/* Work out the density inherent in these areas */
	CImageEffect70_CalcSA(img, invert, v24, REV[3], v32);
	CImageEffect70_CalcSA(img, invert, v20, REV[7], v41);
	CImageEffect70_CalcSA(img, invert, v16, REV[11], v38);
	CImageEffect70_CalcSA(img, invert, v28, REV[15], v35);

	for (j = 0 ; j < 3 ; j++) {
		if ( v32[j] >= REV[4] &&
		     (v32[j] >= REV[5] || v38[j] >= REV[14] || v35[j] >= REV[18]) ) {
			return 0;
		}

		if ( v41[j] >= REV[8] &&
		     (v41[j] >= REV[9] || v38[j] >= REV[14] || v35[j] >= REV[18]) ) {
			return 0;
		}

		if ( v38[j] >= REV[12] &&
		     (v38[j] >= REV[13] || v32[j] >= REV[6] || v41[j] >= REV[10] || v35[j] >= REV[18]) ) {
			return 0;
		}

		if ( v35[j] >= REV[16] &&
		     (v35[j] >= REV[17] || v32[j] >= REV[6] || v41[j] >= REV[10] || v38[j] >= REV[14]) ) {
			return 0;
		}
	}
	return 1;
}

// called twice, once with param1 == 1, once with param1 == 2.
static int CImageEffect70_JudgeReverseSkipRibbon(struct CPCData *cpc,
						 struct BandImage *img,
						 int is_6inch,
						 int param1)
{
	int offset = -1;

	if (param1 == 1) {
		if (is_6inch) {
			offset = 0; // REV[0][0]
		} else {
			offset = 19; // REV[1][0]
		}
	} else if (param1 == 2) {
		if (is_6inch) {
			offset = 38; // REV[2][0]
		} else {
			offset = 57; // REV[3][0]
		}
	}
	if (offset != -1) {
		return CImageEffect70_JudgeReverseSkipRibbon_int(img, &cpc->REV[offset], 1);
	}

	return 0;
}

static void CImageEffect70_DoConv(struct CImageEffect70 *data,
				  struct CPCData *cpc,
				  struct BandImage *in,
				  struct BandImage *out,
				  int sharpen)
{
	double maxval[3];
	double *v9 = NULL;
	double *v10 = NULL;

	uint32_t i, j;
	int offset;
	int outstride;
	uint16_t *outptr;
	uint16_t *inptr;

	CImageEffect70_InitMidData(data);

	if (sharpen > 8)
		sharpen = 8;
	data->sharpen = sharpen;

	data->fhdiv_up = cpc->FH[0];
	data->fhdiv_dn = cpc->FH[1];
	data->fh_cur   = cpc->FH[2];
	data->fh_prev1 = cpc->FH[3] - cpc->FH[2];
	data->fh_prev2 = cpc->FH[4] - cpc->FH[3];
	data->fh_prev3 = cpc->FH[4];

	data->columns = in->cols - in->origin_cols;
	data->rows = in->rows - in->origin_rows;
	data->band_pixels = data->columns * 3;

	if (data->columns <= 0 || data->rows <= 0 ||
	    cpc->FH[0] < 1.0 || cpc->FH[1] < 1.0)
		return;

	if (in->bytes_per_row >= 0) {
		data->pixel_count = in->bytes_per_row / sizeof(uint16_t); // numbers of pixels per input band

		outstride = out->bytes_per_row / sizeof(uint16_t); // pixels per dest band
		inptr = (uint16_t*) in->imgbuf + data->pixel_count * (data->rows - 1); // ie last row of input buffer
		outptr = (uint16_t*) out->imgbuf + outstride * (data->rows - 1); // last row of output buffer
	} else {
		data->pixel_count = -in->bytes_per_row / sizeof(uint16_t);
		outstride = out->bytes_per_row / sizeof(uint16_t);
		inptr = in->imgbuf;
		outptr = out->imgbuf;
	}

	CImageEffect70_CreateMidData(data);

	v10 = malloc(data->band_pixels * sizeof(double));
	memset(v10, 0, (data->band_pixels * sizeof(double)));
	v9 = malloc(data->band_pixels * sizeof(double));
	memset(v9, 0, (data->band_pixels * sizeof(double)));
	maxval[0] = cpc->GNMby[255];
	maxval[1] = cpc->GNMgm[255];
	maxval[2] = cpc->GNMrc[255];

	/* Initialize ttd_next structures */
	offset = 0;
	for(j = 0; j < data->columns ; j++) {
		for (i = 0 ; i < 3 ; i++) {
			data->htd_ttd_next[offset++] = maxval[i];
		}
	}

	CImageEffect70_Sharp_PrepareLine(data, inptr);

	if (data->sharpen >= 0)
		CImageEffect70_Sharp_SetRefPtr(data);

	for (data->cur_row = 0 ; data->cur_row < data->rows ; data->cur_row++) {
		if (data->cur_row + 5 < data->rows)
			CImageEffect70_Sharp_CopyLine(data, 5, inptr, 5);
		CImageEffect70_CalcTTD(data, inptr, v10);
		CImageEffect70_CalcHTD(data, v10, v9);
		CImageEffect70_CalcFCC(data);
		CImageEffect70_CalcYMC6(data, v9, outptr);
		inptr -= data->pixel_count; // work backwards one input row
		outptr -= outstride;        // work backwards one output row
		CImageEffect70_Sharp_ShiftLine(data);
	}
	CImageEffect70_DeleteMidData(data);

	if (v10)
		free(v10);
	if (v9)
		free(v9);
}

static void CImageEffect70_DoGamma(struct CImageEffect70 *data, struct BandImage *input, struct BandImage *out, int reverse)
{
	int cols, rows;
	int i, j;

	uint8_t *outptr, *inptr;
	uint32_t in_stride, out_stride;

	struct CPCData *cpc = data->cpc;

	cols = input->cols - input->origin_cols;
	rows = input->rows - input->origin_rows;

	in_stride = abs(input->bytes_per_row);
	out_stride = abs(out->bytes_per_row);

	if (cols <= 0 || rows <= 0)
	    return;

	inptr = (uint8_t*) input->imgbuf;
	outptr = out->imgbuf;

	/* HACK:  Reverse the row data when we perform gamma correction,
	          because Old Gutenprint sends it in the wrong order. */
	for (i = 0; i < rows; i++) {
		uint8_t *v10 = inptr;
		uint16_t *v9 = (uint16_t*)outptr;
		if (reverse)
			v9 += (cols - 1) * 3;
		for (j = 0 ; j < cols ; j++) {
			v9[0] = cpc->GNMby[v10[0]];
			v9[1] = cpc->GNMgm[v10[1]];
			v9[2] = cpc->GNMrc[v10[2]];
			v10 += 3;
			if (reverse)
				v9 -= 3;
			else
				v9 += 3;
		}
		inptr += in_stride;
		outptr += out_stride;
	}
}

void dump_announce(FILE *fp)
{
	fprintf(fp, "INFO: libMitsuD70ImageReProcess version '%s' API %d\n", LIB_VERSION, LIB_APIVERSION);
	fprintf(fp, "INFO: Copyright (c) 2016-2020 Solomon Peachy\n");
	fprintf(fp, "INFO: This free software comes with ABSOLUTELY NO WARRANTY!\n");
	fprintf(fp, "INFO: Licensed under the GNU GPLv3.\n");
	fprintf(fp, "INFO: *** This code is NOT supported or endorsed by Mitsubishi! ***\n");
}

int do_image_effect80(struct CPCData *cpc, struct CPCData *ecpc, struct BandImage *input, struct BandImage *output, int sharpen, int reverse, uint8_t rew[2])
{
	struct CImageEffect70 *data;

	data = CImageEffect70_Create(cpc);
	if (!data)
		return -1;

	CImageEffect70_DoGamma(data, input, output, reverse);

	/* Figure out if we can get away with rewinding, or not... */
	if (cpc->REV[0]) {
		int is_6 = -1;

		/* Only allow rewinds for 4x6 and 5x3.5" prints */
		if (input->cols == 0x0620 && input->rows == 0x0434)
			is_6 = 0;
		else if (input->cols == 0x0748 && input->rows == 0x04c2)
			is_6 = 1;

		rew[1] = 1;
		if (ecpc == NULL)  /* IOW, only do the rewind check for SuperFine */
			rew[0] = 1;
		else if (is_6 != -1) {
			rew[0] = CImageEffect70_JudgeReverseSkipRibbon(cpc, output, is_6, 1);
		} else {
			rew[0] = 1;
		}
	}

	/* If we're rewinding, we have to switch to the other CPC file and restart the process */
	if (! rew[0] ) {
		CImageEffect70_Destroy(data);
		data = CImageEffect70_Create(ecpc);
		if (!data)
			return -1;

		CImageEffect70_DoGamma(data, input, output, reverse);
	}

	CImageEffect70_DoConv(data, cpc, output, output, sharpen);

	CImageEffect70_Destroy(data);

	return 0;
}

int do_image_effect60(struct CPCData *cpc, struct CPCData *ecpc, struct BandImage *input, struct BandImage *output, int sharpen, int reverse, uint8_t rew[2])
{
	struct CImageEffect70 *data;

	UNUSED(ecpc);

	data = CImageEffect70_Create(cpc);
	if (!data)
		return -1;

	CImageEffect70_DoGamma(data, input, output, reverse);
	CImageEffect70_DoConv(data, cpc, output, output, sharpen);

	/* Figure out if we can get away with rewinding, or not... */
	if (cpc->REV[0]) {
		int is_6 = -1;

		/* Only allow rewinds for 4x6 and 5x3.5" prints */
		if (input->cols == 0x0620 && input->rows == 0x0434)
			is_6 = 0;
		else if (input->cols == 0x0748 && input->rows == 0x04c2)
			is_6 = 1;

		if (is_6 != -1) {
			rew[0] = CImageEffect70_JudgeReverseSkipRibbon(cpc, output, is_6, 1);
			rew[1] = CImageEffect70_JudgeReverseSkipRibbon(cpc, output, is_6, 2);
		}
	}

	CImageEffect70_Destroy(data);

	return 0;
}

int do_image_effect70(struct CPCData *cpc, struct CPCData *ecpc, struct BandImage *input, struct BandImage *output, int sharpen, int reverse, uint8_t rew[2])
{
	struct CImageEffect70 *data;

	UNUSED(ecpc);
	UNUSED(rew);

	data = CImageEffect70_Create(cpc);
	if (!data)
		return -1;

	CImageEffect70_DoGamma(data, input, output, reverse);
	CImageEffect70_DoConv(data, cpc, output, output, sharpen);
	CImageEffect70_Destroy(data);

	return 0;
}

int send_image_data(struct BandImage *out, void *context,
		    int (*callback_fn)(void *context, void *buffer, uint32_t len))
{
	uint32_t rows, cols;
	uint16_t *buf;
	uint32_t i, j, k;
	int ret = 1;
	uint16_t *v15;
	size_t count;

	cols = out->cols - out->origin_cols;
	rows = out->rows - out->origin_rows;
	buf = malloc(CHUNK_LEN);
	if (!buf)
		goto done;
	if (!callback_fn)
		goto done;

	if (out->bytes_per_row > 0) {
		v15 = (uint16_t*)((uint8_t*)out->imgbuf + ((rows - 1) * out->bytes_per_row));
	} else {
		v15 = out->imgbuf;
	}

	for ( i = 0 ; i < 3 ; i++) {
		uint16_t *v13 = &v15[i];
		uint16_t *outptr = buf;

		count = 0;
		memset(buf, 0, CHUNK_LEN);
		for (j = 0 ; j < rows ; j++) {
			uint16_t *v9 = v13;
			for (k = 0 ; k < cols ; k++) {
				*outptr++ = cpu_to_be16(*v9);
				v9 += 3;
				count += 2;
				if ( count == CHUNK_LEN )
				{
					if (callback_fn(context, buf, count))
						goto done;
					count = 0;
					outptr = buf;
					memset(buf, 0, CHUNK_LEN);
				}
			}
			v13 -= out->bytes_per_row / sizeof(uint16_t);
		}
		if (count) {
			if (callback_fn(context, buf, (count + 511) / 512 * 512))
				goto done;
		}
	}

	ret = 0;
done:
	if (buf)
		free(buf);
	return ret;
}

#if 0

struct PrintSetting {
	uint32_t  pad;   // @0
	uint32_t  cols;  // @4
	uint32_t  rows;  // @8
	uint32_t  deck;  // @12 // Deck selection, 0, 1, 2.
	uint32_t  sharp; // @16 // sharpness, -1 is none, 0->8, 4 is normal
	uint32_t  matte; // @20 // 1 or 0.
	uint32_t  mode;  // @24 // 0/1/2 Fine/SuperFine/Ultrafine
	uint32_t  cconv; // @28 // -1/0 none/Table1
	uint32_t  unk6;  // @32 // multicut (0, 4x6*2 = 1, 2x6*2 = 5)
	uint32_t  unk7;  // @36 // image doubled up, (0 or 1)
	uint16_t  unk8;  // @40 // actual print cols in image doubled mode
	uint16_t  unk9;  // @42 // actual print rows in image doubled mode
	uint32_t  unk10; // @44 // rows to cut in doubled mode?  (38 for 4x6T2)
	uint16_t  unk11; // @48 // 4x6 doubled mode, type 3. (0 or 1) ??
	uint16_t  gammaR; // @50
	uint16_t  gammaG; // @52
	uint16_t  gammaB; // @54
	uint16_t  brighr; // @56
	uint16_t  brighg; // @58
	uint16_t  brighb; // @60
	uint16_t  contrr; // @62
	uint16_t  cpntrg; // @64
	uint16_t  contrb; // @66
};

// XXX replace 'args'. buf is >= 512 bytes.
void create_header(uint8_t *buf, uint8_t *args, uint16_t *dim, uint16_t *dim_lam)
{
	int laminate;
	int deck;

	memset((buf + 4), 0, 0x1FCu);
	*(buf + 4) = 0;
	*(buf + 5) = 0;
	*(buf + 16) = dim[0] >> 8;
	*(buf + 17) = dim[0];
	*(buf + 18) = dim[1] >> 8;
	*(buf + 19) = dim[1];
	if ( *(uint32_t *)(args + 12) ) { // laminate dimensions
		*(buf + 20) = dim_lam[0] >> 8;
		*(buf + 21) = dim_lam[0];
		*(buf + 22) = dim_lam[1] >> 8;
		*(buf + 23) = dim_lam[1];
	}
	laminate = *(uint32_t *)(args + 4); // speed/mode
	if ( laminate == 1 ) {
		*(buf + 24) = 3;
	} else if ( laminate == 2 ) {
		*(buf + 24) = 4;
	} else {
		*(buf + 24) = 0;
	}
	deck = *(uint32_t *)(args + 8); // deck
	if ( deck == 1 )
		*(buf + 32) = 2;
	else
		*(buf + 32) = deck == 2;
	*(buf + 40) = 0;
	if ( *(uint32_t *)(args + 12) ) // laminate type
		*(buf + 41) = 2;
	else
		*(buf + 41) = 0;

	*(buf + 48) = *(uint32_t *)(args + 16); // multicut mode
}

void CImageUtility_CreateBandImage16(struct BandImage *img, uint32_t *a2, int32_t a3)
{
	img->bytes_per_row = 3 * sizeof(uint16_t) * (a2[2] - a2[0]);
	if (a3 < 0)
		img->bytes_per_row = -img->bytes_per_row;
	img->origin_cols = a2[0];  // origin_cols
	img->origin_rows = a2[1];  // origin_rows
	img->cols = a2[2];  // cols
	img->rows = a2[3];  // rows

	img->imgbuf = malloc(3 * sizeof(uint16_t) * (a2[3] - a2[1]) * (a2[2] - a2[0]));
}

#endif

/* CP98xx family */

struct CP98xx_WMAM {
	/* @    0 */	double   unka[256];
	/* @ 2048 */	double   unkb[256];
	/* @ 4096 */	double   unkc[5];   /* Weight factors */
	/* @ 4136 */	double   unkd[256];
	/* @ 6184 */	double   unke[256]; // *= sharp->coef[X]
	/* @ 8232 */	double   unkf[5];   /* Weight factors */
	/* @ 8272 */	double   unkg[256];
	/* @10320 */
} __attribute__((packed));

/* CP98xx Tabular Data, as stored in data file! */
struct mitsu98xx_data {
	/* @    0 */	uint16_t GNMby[256];     /* BGR Order uncertain */
	/* @  512 */	uint16_t GNMgm[256];
	/* @ 1024 */    uint16_t GNMrc[256];
	/* @ 1536 */    int16_t  sharp[20];   /* Actual format is: u16, u16[9], u16, u16[9] */
	/* @ 1576 */    double   GammaAdj[3];
	/* @ 1600 */	struct CP98xx_WMAM WMAM;
	/* @11920 */	double   sharp_coef[11]; /* 0 is off, 1-10 are the levels.  Default is 5. [4 in settings] */
	/* @12008 */	int32_t KHStart;
	/* @12012 */	int32_t KHEnd;
	/* @12016 */	int32_t KHStep;
	/* @12020 */	double  KH[256];
	/* @14068 */
} __attribute__((packed));

/* Informational purposes only */
struct mitsu98xx_tables {
	struct mitsu98xx_data superfine;
	struct mitsu98xx_data fine_std;
	struct mitsu98xx_data fine_hg;
} __attribute__((packed));

#define M98XX_DATATABLE_SIZE  42204

STATIC_ASSERT(sizeof(struct mitsu98xx_data) *3 == M98XX_DATATABLE_SIZE);

/* Cooked versions for local state & manipulation */
struct CP98xx_KHParams {
    double KH[256];
    int32_t Start;
    int32_t End;
    int32_t Step;
};

struct CP98xx_GammaParams {
    uint16_t GNMby[256];
    uint16_t GNMgm[256];
    uint16_t GNMrc[256];
    double GammaAdj[3];
};

struct CP98xx_AptParams {
    int16_t mask[8][6];
    int unsharp;
    int mpx10;
};

static double swab_double(double d)
{
	union {
		double d;
		uint64_t n;
		uint8_t c[sizeof(double)];
	} u;

	u.d = d;
	u.n = __builtin_bswap64(u.n);

	return u.d;
}

struct mitsu98xx_data *CP98xx_GetData(const char *filename)
{
	struct mitsu98xx_data *data = NULL;
	FILE *stream;
	int rval;

	if (!filename || !*filename)
		return NULL;

	data = malloc(M98XX_DATATABLE_SIZE);
	if (!data)
		return NULL;

	stream = fopen(filename, "rb");
	if (!stream) {
		free(data);
		return NULL;
	}

	fseek(stream, 0, SEEK_END);
	if (ftell(stream) < M98XX_DATATABLE_SIZE) {
		fclose(stream);
		free(data);
		return NULL;
	}
	fseek(stream, 0, SEEK_SET);
	rval = fread(data, M98XX_DATATABLE_SIZE, 1, stream);
	fclose(stream);

	if (rval != 1) {
		free(data);
		return NULL;
	}

	/* Byteswap data table to native endianness, if necessary */
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
	int i, j;
	for (j = 0 ; j < 3 ; j++) {
		data[j].KHStart = be32_to_cpu(data[j].KHStart);
		data[j].KHEnd = be32_to_cpu(data[j].KHEnd);
		data[j].KHStep = be32_to_cpu(data[j].KHStep);
		for (i = 3 ; i < 3 ; i++) {
			data[j].GammaAdj[i] = be64d_to_cpu(data[j].GammaAdj[i]);
		}
		for (i = 0 ; i < 5 ; i++) {
			data[j].WMAM.unkc[i] = be64d_to_cpu(data[j].WMAM.unkc[i]);
			data[j].WMAM.unkf[i] = be64d_to_cpu(data[j].WMAM.unkf[i]);
		}
		for (i = 0 ; i < 11 ; i++) {
			data[j].sharp_coef[i] = be64d_to_cpu(data[j].sharp_coef[i]);
		}
		for (i = 0 ; i < 20 ; i++) {
			data[j].sharp[i] = be16_to_cpu(data[j].sharp[i]);
		}
		for (i = 0 ; i < 256 ; i++) {
			data[j].WMAM.unka[i] = be64d_to_cpu(data[j].WMAM.unka[i]);
			data[j].WMAM.unkb[i] = be64d_to_cpu(data[j].WMAM.unkb[i]);
			data[j].WMAM.unkd[i] = be64d_to_cpu(data[j].WMAM.unkd[i]);
			data[j].WMAM.unke[i] = be64d_to_cpu(data[j].WMAM.unke[i]);
			data[j].WMAM.unkg[i] = be64d_to_cpu(data[j].WMAM.unkg[i]);

			data[j].GNMby[i] = be16_to_cpu(data[j].GNMby[i]);
			data[j].GNMgm[i] = be16_to_cpu(data[j].GNMgm[i]);
			data[j].GNMrc[i] = be16_to_cpu(data[j].GNMrc[i]);

			data[j].KH[i] = be64d_to_cpu(data[j].KH[i]);
		}
	}
#endif
	return data;
}

void CP98xx_DestroyData(const struct mitsu98xx_data *data)
{
	free((void*)data);
}

/* return 1 if it's ok, 0 if bad */
static int CP98xx_DoCorrectGammaTbl(struct CP98xx_GammaParams *Gamma,
				    const struct CP98xx_KHParams *KH,
				    const struct BandImage *img)
{
	int end, start, cols, rows, step, bytesPerRow;
	int i, j;
	int64_t elements, max;
	uint8_t *rowPtr;
	if (KH->Step < 1 || KH->End < KH->Start)
		return 1;

	if (KH->Start < 0 ||
	    img->cols <= KH->End ||
	    img->cols <= KH->Start ||
	    img->origin_cols ||
	    img->origin_rows)
		return 0;

	cols = img->cols - img->origin_cols;
	rows = img->rows - img->origin_rows;
	bytesPerRow = img->bytes_per_row;

	if (bytesPerRow < 0) {
		rowPtr = img->imgbuf;
	} else {
		rowPtr = (uint8_t*)img->imgbuf + (bytesPerRow * (rows - 1));
	}

	step = KH->Step;
	end = KH->End;
	start = KH->Start;
	elements = step * ((end - start) + 1);
	max = elements * 0xff;

	for (j = 0 ; j < rows / step ; j++) {
		int k;
		int64_t sum1, sum2, sum3, sum4, sum5, sum6;
		int startcol = (cols - end) -1;

		sum6 = sum5 = sum4 = sum3 = sum2 = sum1 = 0;

		for (k = 0 ; k < step ; k++) {
			int curCol, curRowBufOffset;
			curRowBufOffset = start * 3;
			for (curCol = start ; curCol < (end + 1) ; curCol++) {
				sum3 += rowPtr[curRowBufOffset];
				sum2 += rowPtr[curRowBufOffset + 1];
				sum1 += rowPtr[curRowBufOffset + 2];
				curRowBufOffset += 3;
			}
			curRowBufOffset = startcol * 3;
			for (curCol = startcol ; curCol < (cols - start); curCol++) {
				sum6 += rowPtr[curRowBufOffset];
				sum5 += rowPtr[curRowBufOffset + 1];
				sum4 += rowPtr[curRowBufOffset + 2];
				curRowBufOffset += 3;
			}
			rowPtr -= bytesPerRow;
		}
		if (sum6 < max) {
			max = sum6;
		}
		if (sum5 < max) {
			max = sum5;
		}
		if (sum4 < max) {
			max = sum4;
		}
		if (sum3 < max) {
			max = sum3;
		}
		if (sum2 < max) {
			max = sum2;
		}
		if (sum1 < max) {
			max = sum1;
		}
	}

	int gammaMaxCalc;
	uint16_t baseRC, baseGM, baseBY;
	double baseKH;

	gammaMaxCalc = ((double)max / (double)elements) + 0.5;

	baseRC = Gamma->GNMrc[255];
	baseGM = Gamma->GNMgm[255];
	baseBY = Gamma->GNMby[255];
	baseKH = KH->KH[gammaMaxCalc];

	for (i = 0; i < 256 ; i++) {
		Gamma->GNMrc[i] = baseRC +
			(baseKH * (Gamma->GNMrc[i] - baseRC)) + 0.5;
		Gamma->GNMgm[i] = baseGM +
			(baseKH * (Gamma->GNMgm[i] - baseGM)) + 0.5;
		Gamma->GNMby[i] = baseBY +
			(baseKH * (Gamma->GNMby[i] - baseBY)) + 0.5;
	}

	return 1;
}

static int CP98xx_DoGammaConv(struct CP98xx_GammaParams *Gamma,
			      const struct BandImage *inImage,
			      struct BandImage *outImage,
			      int reverse)
{
	int cols, rows, inBytesPerRow, maxTank;
	uint8_t *inRowPtr;
	uint16_t *outRowPtr;
	int pixelsPerRow;

	cols = inImage->cols - inImage->origin_cols;
	rows = inImage->rows - inImage->origin_rows;
	/* Output always starts at end and works back */
	pixelsPerRow = outImage->bytes_per_row >> 1;
	outRowPtr = (uint16_t*)((uint8_t*)outImage->imgbuf + (pixelsPerRow * (rows-1) * sizeof(uint16_t)));

	/* Input is another matter.. */
	inBytesPerRow = inImage->bytes_per_row;

	if ((cols < 1) || (rows < 1) || (inBytesPerRow == 0))
		return 0;

	if (inBytesPerRow < 0) { /* First row of input is at the beginning */
		if (reverse) {
			/* count backwards from end of buffer */
			inBytesPerRow = -inBytesPerRow;
			inRowPtr = (uint8_t*)inImage->imgbuf + (inBytesPerRow * (rows-1));
		} else {
			/* Count forward from start of buffer */
			inRowPtr = inImage->imgbuf;
		}
	} else { /* First row of input is at the end */
		if (reverse) {
			/* Count forwards from start of buffer */
			inBytesPerRow = -inBytesPerRow;
			inRowPtr = (uint8_t*)inImage->imgbuf;
		} else {
			/* Count backwards from end of buffer */
			inRowPtr = (uint8_t*)inImage->imgbuf + (inBytesPerRow * (rows-1));
		}
	}

	maxTank = cols * 255;

	int outVal;
	int row, col;
	int curRowBufOffset;

	double gammaAdj2 = Gamma->GammaAdj[2];
	double gammaAdj1 = Gamma->GammaAdj[1];
	double gammaAdj0 = Gamma->GammaAdj[0];

	/* Do gamma mapping with correction/adjustments... */
	for (row = 0 ; row < rows && gammaAdj0 >= 0.5 ; row++) {
		double calc3, calc2, calc1, calc0;
		double gammaAdjX;

		calc0 = calc1 = calc2 = 0.0;

		for (col = 0, curRowBufOffset = 0 ; col < cols ; col++) {
			calc2 += inRowPtr[2 + curRowBufOffset];
			calc1 += inRowPtr[1 + curRowBufOffset];
			calc0 += inRowPtr[0 + curRowBufOffset];
			curRowBufOffset += 3;
		}

		calc3 = ((maxTank - calc0) + (maxTank - calc1) + (maxTank - calc2)) / (cols * 3);

		gammaAdjX = ((gammaAdj0 + (((calc3 * gammaAdj0) / 255.0) * gammaAdj1) / -4095.0) * gammaAdj2) / 4095.0;

		/* Input and output order are BGR and YMC! */
		for (col = 0, curRowBufOffset = 0; col < cols ; col++) {
			outVal = Gamma->GNMby[inRowPtr[curRowBufOffset]] + gammaAdjX + 0.5;
			if (outVal < 0x1000) {
				if (outVal < 0) {
					outRowPtr[curRowBufOffset] = 0;
				} else {
					outRowPtr[curRowBufOffset] = outVal;
				}
			} else {
				outRowPtr[curRowBufOffset] = 0xfff;
			}

			outVal = Gamma->GNMgm[inRowPtr[curRowBufOffset + 1]] + gammaAdjX + 0.5;
			if (outVal < 0x1000) {
				if (outVal < 0) {
					outRowPtr[curRowBufOffset + 1] = 0;
				} else {
					outRowPtr[curRowBufOffset + 1] = outVal;
				}
			} else {
				outRowPtr[curRowBufOffset + 1] = 0xfff;
			}

			outVal = Gamma->GNMrc[inRowPtr[curRowBufOffset + 2]] + gammaAdjX + 0.5;
			if (outVal < 0x1000) {
				if (outVal < 0) {
					outRowPtr[curRowBufOffset + 2] = 0;
				} else {
					outRowPtr[curRowBufOffset + 2] = outVal;
				}
			} else {
				outRowPtr[curRowBufOffset + 2] = 0xfff;
			}
			curRowBufOffset += 3;
		}

		inRowPtr -= inBytesPerRow;
		outRowPtr -= pixelsPerRow;
	}

	/* ...and pick up where the first loop left off, if we don't need adjustments */
	for ( ; row < rows ; row++) {
		for (col = 0, curRowBufOffset = 0 ; col < cols ; col ++, curRowBufOffset += 3) {
			/* Mitsu code treats input as RGB, we always use BGR. */
			outRowPtr[curRowBufOffset] = Gamma->GNMby[inRowPtr[curRowBufOffset]];
			outRowPtr[curRowBufOffset + 1] = Gamma->GNMgm[inRowPtr[curRowBufOffset + 1]];
			outRowPtr[curRowBufOffset + 2] = Gamma->GNMrc[inRowPtr[curRowBufOffset + 2]];
		}
		inRowPtr -= inBytesPerRow;
		outRowPtr -= pixelsPerRow;
	}

  return 1;
}

static void CP98xx_InitAptParams(const struct mitsu98xx_data *table, struct CP98xx_AptParams *APT, int sharpness)
{
	int i, j;
	double sharpCoef;

	APT->unsharp = 0;
	APT->mpx10 = 1;

	sharpCoef = table->sharp_coef[sharpness];
	for (i = 2, j = 0 ; j < 8 ; i++, j++) {
		APT->mask[j][5] = table->sharp[1];
		APT->mask[j][4] = sharpCoef * table->sharp[i] + 0.5;
		APT->mask[j][3] = table->sharp[11];
		APT->mask[j][2] = sharpCoef * table->sharp[i+10] + 0.5;
	}

	APT->mask[0][0] = -table->sharp[10];
	APT->mask[0][1] = -table->sharp[0];
	APT->mask[1][0] = 0;
	APT->mask[1][1] = -table->sharp[0];
	APT->mask[2][0] = table->sharp[10];
	APT->mask[2][1] = -table->sharp[0];
	APT->mask[3][0] = -table->sharp[10];
	APT->mask[3][1] = 0;
	APT->mask[4][0] = table->sharp[10];
	APT->mask[4][1] = 0;
	APT->mask[5][0] = -table->sharp[10];
	APT->mask[5][1] = table->sharp[0];
	APT->mask[6][0] = table->sharp[10];
	APT->mask[6][1] = table->sharp[0];
	APT->mask[7][0] = 0;
	APT->mask[7][1] = table->sharp[0];
}

static void CP98xx_InitWMAM(struct CP98xx_WMAM *wmam, const struct CP98xx_WMAM *src)
{
	int i;

	for (i = 0 ; i < 256 ; i++) {
		wmam->unka[i] = src->unka[i] / 255.0;
		wmam->unkb[i] = src->unkb[i] / 255.0;
		wmam->unkd[i] = src->unkd[i] / 255.0;
		wmam->unke[i] = src->unke[i] / 64.0;
		wmam->unkg[i] = src->unkg[i] / 64.0;
	}
	memcpy(wmam->unkc, src->unkc, sizeof(wmam->unkc));
	memcpy(wmam->unkf, src->unkd, sizeof(wmam->unkc));
}

static int CP98xx_DoWMAM(struct CP98xx_WMAM *wmam, struct BandImage *img, int reverse)
{
	uint16_t *imgBuf, *rowPtr;
	int rows, col, cols, pixelsPerRow, pixelCnt;
	double *rowCalcBuf1, *rowCalcBuf2, *rowCalcBuf3, *rowCalcBuf4, *rowCalcBuf5;
	double *pdVar3, *pdVar5, *pdVar6, *pdVar7, *pdVar8, *pdVar9, *pdVar11, *pdVar12;

	int row;
	int rowCalcBufLenB, rowCalcBufLen;
	int doubleBufOffset, imgBufOffset;

	cols = img->cols - img->origin_cols;
	rows = img->rows - img->origin_rows;
	pixelsPerRow = img->bytes_per_row;
	rowPtr = imgBuf = img->imgbuf;

	if ((cols < 6) || (rows < 1) || (pixelsPerRow == 0))
		return 0;

	if (pixelsPerRow < 0) {
		if (reverse) {
			pixelsPerRow = pixelsPerRow >> 1;
		} else {
			pixelsPerRow = (-pixelsPerRow) >> 1;
			rowPtr += pixelsPerRow * (rows -1);
			imgBuf = (uint16_t *)rowPtr;
		}
	} else {
		if (reverse) {
			pixelsPerRow = pixelsPerRow >> 1;
			rowPtr += pixelsPerRow * (rows -1);
			imgBuf = (uint16_t *)rowPtr;
		} else {
			pixelsPerRow = (-pixelsPerRow) >> 1;
		}
	}

	pixelCnt = cols * 3;
	rowCalcBufLen = cols * 3 * sizeof(double);

	rowCalcBuf1 = malloc(rowCalcBufLen);
	if (!rowCalcBuf1) {
		return 0;
	}

	rowCalcBuf2 = malloc(rowCalcBufLen);
	if (!rowCalcBuf2) {
		free(rowCalcBuf1);
		return 0;
	}

	rowCalcBufLenB = (pixelCnt + 24) * sizeof(double);
	rowCalcBuf3 = malloc(rowCalcBufLenB);
	if (!rowCalcBuf3) {
		free(rowCalcBuf2);
		free(rowCalcBuf1);
		return 0;
	}
	pdVar8 = rowCalcBuf3 + 12;

	rowCalcBuf4 = malloc(rowCalcBufLenB);
	if (!rowCalcBuf4) {
		free(rowCalcBuf3);
		free(rowCalcBuf2);
		free(rowCalcBuf1);
		return 0;
	}
	pdVar7 = rowCalcBuf4 + 12;

	rowCalcBuf5 = malloc(rowCalcBufLen);
	if (!rowCalcBuf5) {
		free(rowCalcBuf4);
		free(rowCalcBuf3);
		free(rowCalcBuf2);
		free(rowCalcBuf1);
		return 0;
	}

	memset(rowCalcBuf1, 0, cols * 3 * sizeof(double));
	memset(rowCalcBuf2, 0, cols * 3 * sizeof(double));
	pdVar5 = pdVar7 + (cols -1) * 3;
	pdVar3 = pdVar8 + (cols -1) * 3;

	for (row = 0 ; row < rows ; row++) {
		doubleBufOffset = 0;
		imgBufOffset = 0;
		if (pixelCnt < 0) {
			col = 0;
		} else {
			col = pixelCnt;
		}

		for ( ; col > 0 ; col --) {
			double dVar16, dVar17, dVar18, dVar19;
			int iVar1;
			int pixelVal;

			dVar16 = rowPtr[imgBufOffset];
			dVar17 = rowCalcBuf1[doubleBufOffset] - dVar16;
			pixelVal = dVar17;
			if (pixelVal < 0) {
				iVar1 = -0x80;
				if (-0xff0 < pixelVal) {
					iVar1 = -((0x10 - pixelVal) >> 5);
				}
			} else {
				iVar1 = 0x7f;
				if (pixelVal < 0xfd0) {
					iVar1 = (pixelVal + 0x10) >> 5;
				}
			}

			dVar17 *= wmam->unka[128+iVar1];
			pixelVal = dVar17;
			pdVar8[doubleBufOffset] = dVar16 + dVar17;
			if (pixelVal < 0) {
				iVar1 = -0x80;
				if (-0xff0 < pixelVal) {
					iVar1 = -((0x10 - pixelVal) >> 5);
				}
			} else {
				iVar1 = 0x7f;
				if (pixelVal < 0xfd0) {
					iVar1 = (pixelVal + 0x10) >> 5;
				}
			}
			dVar19 = wmam->unkb[128+iVar1];
			dVar18 = rowCalcBuf2[doubleBufOffset] - dVar16;

			pixelVal = dVar18;
			if (pixelVal < 0) {
				iVar1 = -0x80;
				if (-0xff0 < pixelVal) {
					iVar1 = -((0x10 - pixelVal) >> 5);
				}
			} else {
				iVar1 = 0x7f;
				if (pixelVal < 0xfd0) {
					iVar1 = (pixelVal + 0x10) >> 5;
				}
			}
			dVar18 *= wmam->unkd[128+iVar1];
			pdVar7[doubleBufOffset] = dVar16 + dVar18;

			pixelVal = dVar18;
			if (pixelVal < 0) {
				iVar1 = -0x80;
				if (-0xff0 < pixelVal) {
					iVar1 = -((0x10 - pixelVal) >> 5);
				}
			} else {
				iVar1 = 0x7f;
				if (pixelVal < 0xfd0) {
					iVar1 = (pixelVal + 0x10) >> 5;
				}
			}

			dVar16 = (-(dVar17 * dVar19 - dVar16) +
				  -(dVar18 * (wmam->unke[iVar1 + 128]) - dVar16)) * 0.50000000;

			if (row != 0) {
				if (0.00000000 <= dVar16) {
					if (dVar16 <= 4095.00000000) {
						dVar17 = rowCalcBuf5[doubleBufOffset];
					} else {
						iVar1 = 0;
						pixelVal = (dVar16 - 4095.00000000);
						if ((-1 < pixelVal) && (iVar1 = 0x7f, pixelVal < 0xff0)) {
							iVar1 = (pixelVal + 0x10) >> 5;
						}
						dVar17 = (dVar16 - 4095.00000000) * wmam->unkg[127+iVar1] +
							rowCalcBuf5[doubleBufOffset];
					}
				} else {
					pixelVal = dVar16;
					iVar1 = 0x80; // XXX seems redundant, double-check iVar1 here.
					if ((-0xff0 < pixelVal) && (iVar1 = 0xff, pixelVal < 1)) {
						iVar1 = 0xff - ((0x10 - pixelVal) >> 5);
					}
					dVar17 = dVar16 * wmam->unkg[127+iVar1] +
						rowCalcBuf5[doubleBufOffset];
				}
				pixelVal = dVar17 + 0.50000000;
				if (pixelVal < 0x1000) {
					if (pixelVal < 0) {
						imgBuf[imgBufOffset] = 0;
					} else {
						imgBuf[imgBufOffset] = pixelVal;
					}
				} else {
					imgBuf[imgBufOffset] = 0xfff;
				}
			}

			rowCalcBuf5[doubleBufOffset] = dVar16;
			imgBufOffset++;
			doubleBufOffset++;
		}

		col = 0;

		rowCalcBuf3[9]  = rowCalcBuf3[15];
		rowCalcBuf3[10] = rowCalcBuf3[16];
		rowCalcBuf3[11] = rowCalcBuf3[17];
		rowCalcBuf3[6]  = rowCalcBuf3[18];
		rowCalcBuf3[7]  = rowCalcBuf3[19];
		rowCalcBuf3[8]  = rowCalcBuf3[20];
		rowCalcBuf3[3]  = rowCalcBuf3[21];
		rowCalcBuf3[4]  = rowCalcBuf3[22];
		rowCalcBuf3[5]  = rowCalcBuf3[23];
		rowCalcBuf3[0]  = rowCalcBuf3[24];
		rowCalcBuf3[1]  = rowCalcBuf3[25];
		rowCalcBuf3[2]  = rowCalcBuf3[26];

		rowCalcBuf4[9]  = rowCalcBuf4[15];
		rowCalcBuf4[10] = rowCalcBuf4[16];
		rowCalcBuf4[11] = rowCalcBuf4[17];
		rowCalcBuf4[6]  = rowCalcBuf4[18];
		rowCalcBuf4[7]  = rowCalcBuf4[19];
		rowCalcBuf4[8]  = rowCalcBuf4[20];
		rowCalcBuf4[3]  = rowCalcBuf4[21];
		rowCalcBuf4[4]  = rowCalcBuf4[22];
		rowCalcBuf4[5]  = rowCalcBuf4[23];
		rowCalcBuf4[0]  = rowCalcBuf4[24];
		rowCalcBuf4[1]  = rowCalcBuf4[25];
		rowCalcBuf4[2]  = rowCalcBuf4[26];

		pdVar3[3]  = pdVar3[-3];
		pdVar3[4]  = pdVar3[-2];
		pdVar3[5]  = pdVar3[-1];
		pdVar3[6]  = pdVar3[-6];
		pdVar3[7]  = pdVar3[-5];
		pdVar3[8]  = pdVar3[-4];
		pdVar3[9]  = pdVar3[-9];
		pdVar3[10] = pdVar3[-8];
		pdVar3[11] = pdVar3[-7];
		pdVar3[12] = pdVar3[-12];
		pdVar3[13] = pdVar3[-11];
		pdVar3[14] = pdVar3[-10];

		pdVar5[3]  = pdVar5[-3];
		pdVar5[4]  = pdVar5[-2];
		pdVar5[5]  = pdVar5[-1];
		pdVar5[6]  = pdVar5[-6];
		pdVar5[7]  = pdVar5[-5];
		pdVar5[8]  = pdVar5[-4];
		pdVar5[9]  = pdVar5[-9];
		pdVar5[10] = pdVar5[-8];
		pdVar5[11] = pdVar5[-7];
		pdVar5[12] = pdVar5[-12];
		pdVar5[13] = pdVar5[-11];
		pdVar5[14] = pdVar5[-10];

		pdVar11 = rowCalcBuf2;
		pdVar12 = pdVar8;
		pdVar6 = pdVar7;
		pdVar9 = rowCalcBuf1;

		while( 1 ) {
			double dVar16, dVar17, dVar18, dVar19;
			double dVar20, dVar21, dVar22, dVar23, dVar24, dVar25;
			double dVar26, dVar27, dVar28, dVar29, dVar30, dVar31;
			double dVar32, dVar33, dVar34, dVar35, dVar36, dVar37;
			double dVar38, dVar39, dVar40, dVar41, dVar42, dVar43;
			double dVar44, dVar45, dVar46, dVar47, dVar48, dVar49;
			double dVar50, dVar51, dVar52, dVar53, dVar54, dVar55;
			double dVar56, dVar57, dVar58, dVar59, dVar60, dVar61;
			double dVar62, dVar63, dVar64, dVar65, dVar66, dVar67;
			double dVar68, dVar69, dVar70;

			if (pixelCnt <= col) break;
			col += 3;

			dVar38 = pdVar12[-11];
			dVar26 = pdVar12[-10];
			dVar60 = pdVar12[-8];
			dVar70 = pdVar12[-7];
			dVar61 = pdVar12[-5];
			dVar58 = pdVar12[-4];
			dVar36 = pdVar12[-2];
			dVar27 = pdVar12[-1];
			dVar43 = pdVar12[1];
			dVar25 = pdVar12[2];
			dVar30 = pdVar12[4];
			dVar42 = pdVar12[5];
			dVar54 = pdVar12[7];
			dVar59 = pdVar12[8];
			dVar55 = pdVar12[10];
			dVar28 = pdVar12[11];
			dVar53 = pdVar12[13];
			dVar56 = pdVar12[14];

			dVar41 = pdVar6[-12];
			dVar29 = pdVar6[-11];
			dVar45 = pdVar6[-10];
			dVar21 = pdVar6[-9];
			dVar66 = pdVar6[-8];
			dVar24 = pdVar6[-7];
			dVar57 = pdVar6[-6];
			dVar67 = pdVar6[-5];
			dVar64 = pdVar6[-4];
			dVar23 = pdVar6[-3];
			dVar33 = pdVar6[-2];
			dVar39 = pdVar6[-1];
			dVar22 = pdVar6[0];
			dVar47 = pdVar6[1];
			dVar31 = pdVar6[2];
			dVar46 = pdVar6[3];
			dVar49 = pdVar6[4];
			dVar16 = pdVar6[5];
			dVar63 = pdVar6[6];
			dVar65 = pdVar6[7];
			dVar69 = pdVar6[8];
			dVar40 = pdVar6[9];
			dVar44 = pdVar6[10];
			dVar48 = pdVar6[11];
			dVar62 = pdVar6[12];
			dVar68 = pdVar6[13];
			dVar20 = pdVar6[14];

			dVar37 = wmam->unkc[0];
			dVar50 = wmam->unkc[1];
			dVar18 = wmam->unkc[2];
			dVar35 = wmam->unkc[3];
			dVar19 = wmam->unkc[4];

			dVar34 = wmam->unkf[0];
			dVar17 = wmam->unkf[1];
			dVar51 = wmam->unkf[2];
			dVar32 = wmam->unkf[3];
			dVar52 = wmam->unkf[4];

			pdVar9[0] = (dVar19 * (pdVar12[-0xc] + pdVar12[0xc]) +
				     dVar35 * (pdVar12[-9] + pdVar12[9]) +
				     dVar18 * (pdVar12[-6] + pdVar12[6]) +
				     dVar37 * (*pdVar12 + *pdVar12) + dVar50 * (pdVar12[-3] + pdVar12[3])) / 1000.00000000;
			pdVar9[1] = (dVar19 * (dVar38 + dVar53) +
				     dVar35 * (dVar60 + dVar55) +
				     dVar18 * (dVar61 + dVar54) +
				     dVar37 * (dVar43 + dVar43) + dVar50 * (dVar36 + dVar30)) / 1000.00000000;
			pdVar9[2] = (dVar19 * (dVar26 + dVar56) +
				     dVar35 * (dVar70 + dVar28) +
				     dVar18 * (dVar58 + dVar59) +
				     dVar37 * (dVar25 + dVar25) + dVar50 * (dVar27 + dVar42)) / 1000.00000000;
			pdVar11[0] = (dVar52 * (dVar41 + dVar62) +
				    dVar32 * (dVar21 + dVar40) +
				    dVar51 * (dVar57 + dVar63) +
				    dVar34 * (dVar22 + dVar22) + dVar17 * (dVar23 + dVar46)) / 1000.00000000;
			pdVar11[1] = (dVar52 * (dVar29 + dVar68) +
				      dVar32 * (dVar66 + dVar44) +
				      dVar51 * (dVar67 + dVar65) +
				      dVar34 * (dVar47 + dVar47) + dVar17 * (dVar33 + dVar49)) / 1000.00000000;
			pdVar11[2] = (dVar52 * (dVar45 + dVar20) +
				      dVar32 * (dVar24 + dVar48) +
				      dVar51 * (dVar64 + dVar69) +
				      dVar34 * (dVar31 + dVar31) + dVar17 * (dVar39 + dVar16)) / 1000.00000000;

			pdVar11 += 3;
			pdVar12 += 3;
			pdVar6 += 3;
			pdVar9 += 3;
		}

		rowPtr -= pixelsPerRow;
		if (row != 0) {
			imgBuf -= pixelsPerRow;
		}
	}

	int iVar4;
	pdVar3 = rowCalcBuf5;
	for (iVar4 = pixelCnt ; iVar4 > 0 ; iVar4--) {
		int16_t val = (*pdVar3 + 0.50000000);
		if (val < 0) {
			*imgBuf = 0;
		} else {
			if (val < 0x1000) {
				*imgBuf = val;
			} else {
				*imgBuf = 0xfff;
			}
		}
		pdVar3 ++;
		imgBuf ++;
	}

	/* Clean up, we're done! */
	free(rowCalcBuf5);
	free(rowCalcBuf4);
	free(rowCalcBuf3);
	free(rowCalcBuf2);
	free(rowCalcBuf1);

	return 1;
}

int CP98xx_DoConvert(const struct mitsu98xx_data *table,
		     const struct BandImage *input,
		     struct BandImage *output,
		     uint8_t type, int sharpness, int already_reversed)
{
	uint32_t i;

	/* Figure out which table to use */
	switch (type) {
	case 0x80:
		table = &table[0];  /* Superfine */
		break;
	case 0x11:
		table = &table[2];  /* Fine HG */
		break;
	case 0x10:
	default:
		table = &table[1];  /* Fine STD */
		break;
	}

	/* We've already gone through 3D LUT */

	/* Sharpen, as needed */
	if (sharpness > 0) {
		struct CP98xx_AptParams APT;
		CP98xx_InitAptParams(table, &APT, sharpness);
		// XXX DoAptMWithParams();
	}

	/* Set up gamma tables */
	struct CP98xx_GammaParams gamma;
	struct CP98xx_KHParams kh;

	memcpy(gamma.GNMgm, table->GNMgm, sizeof(gamma.GNMgm));
	memcpy(gamma.GNMby, table->GNMby, sizeof(gamma.GNMby));
	memcpy(gamma.GNMrc, table->GNMrc, sizeof(gamma.GNMrc));
	memcpy(gamma.GammaAdj, table->GammaAdj, sizeof(gamma.GammaAdj));
	memcpy(kh.KH, table->KH, sizeof(kh.KH));
	kh.Start = table->KHStart;
	kh.End = table->KHEnd;
	kh.Step = table->KHStep;

	if (CP98xx_DoCorrectGammaTbl(&gamma, &kh, input) != 1) {
		return 0;
	}

	/* Run through gamma conversion */
	if (CP98xx_DoGammaConv(&gamma, input, output, already_reversed) != 1) {
		return 0;
	}

	/* Set up and run through the WMAM flow */
	struct CP98xx_WMAM wmam;
	CP98xx_InitWMAM(&wmam, &table->WMAM);
	if (CP98xx_DoWMAM(&wmam, output, 1) != 1) {
		return 0;
	}

	/* Convert to printer's native BE16 */
	for (i = 0; i < (output->rows * output->cols * 3 / sizeof(uint16_t)) ; i++) {
		((uint16_t*)output->imgbuf)[i] = cpu_to_be16(((uint16_t*)output->imgbuf)[i]);
	}

	return 1;
}


/* Mitsubishi CP-M1 family */
#define M1CPCDATA_GAMMA_ROWS 256
#define M1CPCDATA_ROWS 7 /* Correlates to sharpening levels */

struct M1CPCData {
	uint16_t GNMaB[M1CPCDATA_GAMMA_ROWS];
	uint16_t GNMaG[M1CPCDATA_GAMMA_ROWS];
	uint16_t GNMaR[M1CPCDATA_GAMMA_ROWS];

	uint16_t EnHTH[M1CPCDATA_ROWS];        // fixed @96
	uint16_t NoISetH[M1CPCDATA_ROWS];      // fixed @8
	uint16_t NRGain[M1CPCDATA_ROWS];       // fixed @40
	uint16_t NRTH[M1CPCDATA_ROWS];         // fixed @32
	uint8_t  NRK[M1CPCDATA_ROWS];          // fixed @1
	uint16_t HDEnhGain[M1CPCDATA_ROWS];    // Varies!
	uint16_t EnhDarkGain[M1CPCDATA_ROWS];  // Fixed @0
	uint8_t  DtctArea[M1CPCDATA_ROWS];     // Fixed @1
	uint8_t  CorCol[M1CPCDATA_ROWS];       // Fixed @2
	uint8_t  HighDownMode[M1CPCDATA_ROWS]; // Fixed @1
	uint16_t HighTH[M1CPCDATA_ROWS];       // Fixed @800
	double   HighG[M1CPCDATA_ROWS];        // Fixed @0.1
};

struct SIZE {
    int32_t cx;
    int32_t cy;
};

struct POINT {
    uint32_t x;
    uint32_t y;
};

/* Sharpening stuff */
static void M1_GetAroundBrightness(const uint16_t *pSrcBrightness,
				   const struct SIZE *pSrcSize,
				   const struct POINT *pPtCenter,
				   uint16_t *pDstBrightness,
				   struct SIZE *pDstSize)

{
	uint16_t center;
	int32_t vert, horiz;
	int32_t UVar1;
	int32_t UVar2;
	int bottom, right, top, left;
	int i;
	uint16_t *pBottomRight;
	const uint16_t *pTopLeft;
	int32_t col;
	int32_t row;
	uint16_t *pDstRow;

	center = pSrcBrightness[pSrcSize->cx * pPtCenter->y + pPtCenter->x];
	pDstRow = pDstBrightness + pDstSize->cx;

	for (row = 0 ; row < pDstSize->cx ; row++) {
		pDstBrightness[row] = center;
	}

	for (col = 1; col < pDstSize->cy; col++) {
		memcpy(pDstRow, pDstBrightness, pDstSize->cx * sizeof(uint16_t));
		pDstRow += pDstSize->cx;
	}

	vert = pPtCenter->x + (pDstSize->cx >> 1);
	horiz = pPtCenter->y + (pDstSize->cy >> 1);

	top = pPtCenter->x - vert;
	bottom = 0;
	if (top < 0) {
		bottom = -top;
		top = 0;
	}

	left = pPtCenter->y - horiz;
	right = 0;
	if (left < 0) {
		right = -left;
	  left = 0;
	}

	if (pSrcSize->cx - 1 < vert) {
		UVar1 = pDstSize->cx - ((vert - pSrcSize->cx) + 1);
	} else {
		UVar1 = pDstSize->cx;
	}

	if (pSrcSize->cy - 1 < horiz) {
		UVar2 = pDstSize->cy - ((horiz - pSrcSize->cy) + 1);
	} else {
		UVar2 = pDstSize->cy;
	}

	pTopLeft = pSrcBrightness + pSrcSize->cx * left + top;
	pBottomRight = pDstBrightness + pDstSize->cx * right + bottom;
	for (i = right ; i < UVar2 ; i++) {
		memcpy(pBottomRight, pTopLeft, (UVar1 - bottom) * sizeof(uint16_t));
		pTopLeft = pTopLeft + pSrcSize->cx;
		pBottomRight += pDstSize->cx;
	}
	return;
}

static const int16_t aroundMap08[9] = { 1, 1, 1,
					1, 0, 1,
					1, 1, 1};
static const int16_t aroundMap16[25] = { 0, 0, 1, 1, 0,
					 1, 1, 1, 1, 0,
					 1, 1, 0, 1, 1,
					 0, 1, 1, 1, 1,
					 0, 1, 1, 0, 0 };
static const int16_t aroundMap64[81] = { 0, 0, 0, 1, 1, 1, 1, 0, 0,
					 0, 1, 1, 1, 1, 1, 1, 1, 0,
					 0, 1, 1, 1, 5, 5, 1, 1, 0,
					 1, 1, 5, 5, 5, 5, 1, 1, 1,
					 1, 1, 5, 5, 1, 5, 5, 1, 1,
					 1, 1, 1, 5, 5, 5, 5, 1, 0,
					 0, 1, 1, 5, 5, 1, 1, 1, 0,
					 0, 1, 1, 1, 1, 1, 1, 1, 0,
					 0, 0, 1, 1, 1, 1, 1, 0, 0 };

static double M1_GetBrightnessAverage(const uint16_t *pBitBrightness,
				      const struct SIZE *pSize,
				      const struct POINT *pPtCenter,
				      uint8_t dtctArea,
				      int32_t enhTh, int32_t noiseTh)

{
	uint16_t srcPixel;
	struct SIZE dtct;
	uint16_t pDestBrightness [85];
	uint16_t intPixel;
	int32_t col, row;
	uint16_t *pDestPtr;
	uint16_t local_12 = 0;
	uint32_t local_10 = 0;

	const int16_t *aroundMapPtr;
	const int16_t *aroundMap;

	if (dtctArea == 0) {
		aroundMap = aroundMap64;
		dtct.cx = 9;
		dtct.cy = 9;
	} else if (dtctArea == 1) {
		aroundMap = aroundMap16;
		dtct.cx = 5;
		dtct.cy = 5;
	} else {
		aroundMap = aroundMap08;
		dtct.cx = 3;
		dtct.cy = 3;
	}
	M1_GetAroundBrightness(pBitBrightness,pSize,pPtCenter,pDestBrightness,&dtct);
	srcPixel = pBitBrightness[pSize->cx * pPtCenter->y + pPtCenter->x];
	pDestPtr = pDestBrightness;
	aroundMapPtr = aroundMap;
	for (row = 0 ; row < dtct.cy ; row++) {
		for (col = 0 ; col < dtct.cx ; col++) {
			int32_t tmp = *pDestPtr - srcPixel;
			if ((noiseTh + enhTh) < tmp) {
				intPixel = enhTh + srcPixel;
			} else {
				if (noiseTh < tmp) {
					intPixel = *pDestPtr - noiseTh;
				} else if (-(noiseTh + enhTh) == tmp || -tmp < (noiseTh + enhTh)) {
					intPixel = srcPixel;
					if (-noiseTh != tmp && noiseTh <= -tmp) {
						intPixel = noiseTh + *pDestPtr;
					}
				} else {
					intPixel = srcPixel - enhTh;
				}
			}
			local_10 += *aroundMapPtr * intPixel;
			local_12 += *aroundMapPtr;
			pDestPtr++;
			aroundMapPtr++;
		}
	}
	return (double)local_10 / (double)local_12;
}

int M1_CLocalEnhancer(const struct M1CPCData *cpc,
		      int sharp, struct BandImage *img)
{
	struct SIZE size;
	double NRK;
	uint16_t *rowBuffer, *rowPtr;
	uint16_t *inBasePtr, *inRowPtr, *inPixelPtr;
	int row, col;
	struct POINT pt;
	int i;
	double avgBrightness;

	size.cx = img->cols - img->origin_cols;
	size.cy = img->rows - img->origin_rows;

	switch (cpc->NRK[sharp]) {
	case 3:
		NRK = 3.0;
		break;
	case 2:
		NRK = 2.0;
		break;
	case 1:
		NRK = 1.0;
		break;
	default:
		NRK = 0.5;
		break;
	}

	rowBuffer = malloc(size.cx * size.cy * 2);
	if (!rowBuffer)
		return -1;

	if (img->bytes_per_row < 0)
		inBasePtr = img->imgbuf;
	else
		inBasePtr = (uint16_t*)((uint8_t*)img->imgbuf + (size.cy - 1) * img->bytes_per_row);

	inRowPtr = inBasePtr;
	rowPtr = rowBuffer;

	/* Work out the luminence of each pixel */
	for (col = 0 ; col < size.cy ; col ++) {
		inPixelPtr = inRowPtr;
		for (row = 0 ; row < size.cx ; row ++) {
			*rowPtr = ((inPixelPtr[0] * 0.299 +
				    inPixelPtr[1] * 0.587 +
				    inPixelPtr[2] * 0.114) / 16.0) + 0.5;
			inPixelPtr += 3;
			rowPtr ++;
		}
		inRowPtr -= img->bytes_per_row / sizeof(int16_t);
	}

	inRowPtr = inBasePtr;
	rowPtr = rowBuffer;
	for (pt.y = 0 ; (int)pt.y < size.cy ; pt.y++) {
		inPixelPtr = inRowPtr;
		for (pt.x = 0 ; (int)pt.x < size.cx ; pt.x++) {
			double outVals[3];
			double dVar5;
			double local_100, local_1b0, local_1b8;
			uint8_t local_102, local_101;
			uint16_t uVar2, uVar1;

			memset(outVals, 0, sizeof(outVals));

			/* Get the average brightness of each point */
			avgBrightness = M1_GetBrightnessAverage(rowBuffer, &size, &pt,
								cpc->DtctArea[sharp],
								cpc->EnHTH[sharp],
								cpc->NoISetH[sharp]);

			/* Work out the amount of compensation for this point */
			dVar5 = *rowPtr - avgBrightness;
			if (dVar5 < 0.0) {
				dVar5 *= -1.0;
			}
			if (dVar5 >= cpc->NRTH[sharp]) {
				if (dVar5 >= cpc->NRTH[sharp] + cpc->NRGain[sharp] / NRK) {
					dVar5 = 0.0;
				} else {
					dVar5 = cpc->NRGain[sharp] - NRK * (dVar5 - cpc->NRTH[sharp]);
				}
			} else {
				dVar5 = cpc->NRGain[sharp];
			}

			dVar5 = ((cpc->HDEnhGain[sharp] + avgBrightness * cpc->EnhDarkGain[sharp]) - dVar5) / 32.0;

			if (*rowPtr != 0) {
				avgBrightness /= *rowPtr;
			}
			if (1.0 <= avgBrightness) {
				local_1b0 = 1.00000000 - dVar5 * (avgBrightness - 1.0);
			} else {
				local_1b0 = dVar5 * (1.00000000 - avgBrightness) + 1.0;
			}

			if (0.0 <= local_1b0) {
				if (local_1b0 <= 8.0) {
					local_1b8 = local_1b0;
				} else {
					local_1b8 = 8.0;
				}
			} else {
				local_1b8 = 0.0;
			}

			/* Work out relative pixel weights */
			if (inPixelPtr[1] < inPixelPtr[2]) {
				if (inPixelPtr[2] < *inPixelPtr) {
					local_101 = 0;
					local_102 = 1;
				} else {
					local_102 = inPixelPtr[1] < *inPixelPtr;
					local_101 = 2;
				}
			} else {
				if (inPixelPtr[1] < *inPixelPtr) {
					local_101 = 0;
					local_102 = 1;
				} else {
					if (inPixelPtr[2] < *inPixelPtr) {
						local_102 = 1;
					} else {
						local_102 = 0;
					}
					local_101 = 1;
				}
			}

			/* Figure out the per-pixel compensation */
			uVar2 = inPixelPtr[(int)local_101];
			uVar1 = inPixelPtr[(int)local_102];
			if (1.0 <= local_1b0) {
				local_100 = local_1b8;
			} else {
				if (cpc->CorCol[sharp] == 1) {
					local_100 = 1.0 -
						((1.0 - local_1b8) * (double)((0x4000 - uVar2) + uVar1)) / 16384.0;
				} else if (cpc->CorCol[sharp] == 2) {
					if ((int)(uVar2 - uVar1) < 0x2000) {
						local_100 = 1.0 -
							((1.0 - local_1b8) * (double)((0x2000 - uVar2) + uVar1)) / 16384.0;
					} else {
						local_100 = 1.0;
					}
				} else {
					local_100 = local_1b8;
				}
			}
			dVar5 = *rowPtr * local_100;

			/* Apply the compensation to each point */
			if (((local_100 <= 1.0) || (cpc->HighDownMode[sharp] != 1)) ||
			    (dVar5 <= cpc->HighTH[sharp])) {
				for (i = 0 ; i < 3 ; i++) {
					outVals[i] = inPixelPtr[i] * local_100;
				}
			} else {
				double dVar4 = 1.0 -
					((dVar5 - cpc->HighTH[sharp]) * cpc->HighG[sharp]) /
					(0x400 - cpc->HighTH[sharp]);
				if (*rowPtr <= dVar5 * dVar4) {
					for (i = 0 ; i < 3 ; i++) {
						outVals[i] = inPixelPtr[i] * local_100 * dVar4;
					}
				} else {
					for (i = 0 ; i < 3 ; i++) {
						outVals[i] = inPixelPtr[i];
					}
				}
			}

			/* Finally, spit out the final (capped) values */
			for (i = 0 ; i < 3 ; i++) {
				if (outVals[i] < 0)
					inPixelPtr[i] = 0;
				else if (outVals[i] > 0x3fff)
					inPixelPtr[i] = 0x3fff;
				else
					inPixelPtr[i] = outVals[i];
			}

			inPixelPtr+=3;
			rowPtr++;
		}
		inRowPtr -= img->bytes_per_row / sizeof(uint16_t);
	}

	free(rowBuffer);
	return 0;
}

/* Do the 8bpp->14bpp gamma conversion */
void M1_Gamma8to14(const struct M1CPCData *cpc,
		   const struct BandImage *in, struct BandImage *out)
{
	int rows, cols, row, col;
	const uint8_t *inp;
	uint16_t *outp;

	rows = in->rows - in->origin_rows;
	cols = in->cols - in->origin_cols;

	inp = in->imgbuf;
	outp = (uint16_t*) out->imgbuf;

	for (row = 0 ; row < rows ; row ++) {
		for (col = 0 ; col < cols * 3 ; col+=3) {
			outp[col] = cpc->GNMaR[inp[col]];     /* R */
			outp[col+1] = cpc->GNMaG[inp[col+1]]; /* G */
			outp[col+2] = cpc->GNMaB[inp[col+2]]; /* B */
		}

		inp += in->bytes_per_row;
		outp += out->bytes_per_row / 2;
	}
}

/* Essentially this yields a fixed value for any given print size */
uint8_t M1_CalcOpRateGloss(uint16_t rows, uint16_t cols)
{
	double d;

	rows += 12;

	/* Do not know the significance of this magic number */
	d = (((rows * cols * 0x80) / 1183483560.0) * 100.0) + 0.5;

	/* Truncate to 8 bit integer */
	return (uint8_t) d;
}

/* Assumes rowstride = cols */
uint8_t M1_CalcOpRateMatte(uint16_t rows, uint16_t cols, uint8_t *data)
{
	uint64_t sum = 0;
	int i;
	double d;

	for (i = 0 ; i < (rows * cols) ; i++) {
		sum += data[i];
	}
	sum = (rows * cols * 0xff) - sum;

	/* Do not know the significance of this magic number */
	d = ((sum / 1183483560.0) * 100.0) + 0.5;

	/* Truncate to 8 bit integer */
	return (uint8_t)d;
}

/* Assumes rowstride = cols * 3 */
int M1_CalcRGBRate(uint16_t rows, uint16_t cols, uint8_t *data)
{
	uint64_t sum = 0;
	int i;
	double d;

	for (i = 0 ; i < (rows * cols * 3) ; i++) {
		sum += data[i];
	}
	sum = (rows * cols * 3 * 255) - sum;

	d = ((sum / 3533449320.0) * 100) + 0.5;

	return (uint8_t)d;
}

void M1_DestroyCPCData(struct M1CPCData *dat)
{
	free(dat);
}

struct M1CPCData *M1_GetCPCData(const char *corrtable_path, const char *filename,
				const char *gammafilename)
{
	struct M1CPCData *data;
	FILE *f;
	char buf[4096];
	int line;
	char *ptr;

	const char *delim = " ,\t\n\r";
	if (!filename || !gammafilename)
		return NULL;
	data = malloc(sizeof(*data));
	if (!data)
		return NULL;

	snprintf(buf, sizeof(buf), "%s/%s", corrtable_path, gammafilename);

	f = fopen(buf, "r");
	if (!f)
		goto done_free;

	/* Skip the first two rows */
	for (line = 0 ; line < 2 ; line++) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			goto abort;
	}

	/* Read in the row data */
	for (line = 0 ; line < M1CPCDATA_GAMMA_ROWS ; line++) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			goto abort;

		ptr = strtok(buf, delim);  // Always skip first column
		if (!ptr)
			goto abort;

		/* Pull out the BGR mappings */
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->GNMaB[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->GNMaG[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->GNMaR[line] = strtol(ptr, NULL, 10);
	};

	fclose(f);

	snprintf(buf, sizeof(buf), "%s/%s", corrtable_path, filename);

	/* Now for the CPC Data */
	f = fopen(buf, "r");
	if (!f)
		goto done_free;

	/* Skip the first two rows */
	for (line = 0 ; line < 2 ; line++) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			goto abort;
	}

	/* Read in the row data */
	for (line = 0 ; line < M1CPCDATA_ROWS ; line++) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			goto abort;
		ptr = strtok(buf, delim);  // Always skip first column
		if (!ptr)
			goto abort;

		/* Pull out the mappings */
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->EnHTH[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->NoISetH[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->NRGain[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->NRTH[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->NRK[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->HDEnhGain[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->EnhDarkGain[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->DtctArea[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->CorCol[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->HighDownMode[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->HighTH[line] = strtol(ptr, NULL, 10);
		ptr = strtok(NULL, delim);
		if (!ptr)
			goto abort;
		data->HighG[line] = strtod(ptr, NULL);
	};

	fclose(f);
	return data;
abort:
	fclose(f);
done_free:
	free(data);
	return NULL;
}
