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

   More recently, the CP98xx family now uses this library.  These
   models are expected to function:

     * Mitsubishi CP9800DW
     * Mitsubishi CP9810DW
     * Mitsubishi CP9820DW-S

   Even more recently, the CP-M1 family now uses this library.  These
   models are expected to function:

     * Mitsubishi CP-M1

   Finally, the CP-D30 family utilizes this library.  This includes these
   models:

     * Mitsubishi CP-D30DW

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

          [http://www.gnu.org/licenses/gpl-3.0.html]

   SPDX-License-Identifier: GPL-3.0+

*/

#ifndef __MITSU_D70_H
#define __MITSU_D70_H

#define LIB_APIVERSION 8

#include <stdint.h>

/* Defines an image.  Note that origin_cols/origin_rows should always = 0 */
struct BandImage {
	   void  *imgbuf;      // @ 0
	 int32_t bytes_per_row;// @ 4 (respect 8bpp and 16bpp!)
	uint16_t origin_cols;  // @ 8 (left)
	uint16_t origin_rows;  // @12 (top)
	uint16_t cols;         // @16 (right)
	uint16_t rows;         // @20 (bottom)
	                       // @24
};

/* Get version */
int lib70x_getapiversion(void);

/* print annoucement blurb */
void dump_announce(FILE *fp);

/* Forward-declaration */
struct CPCData;

/* Load and parse CPC data. */
struct CPCData *get_CPCData(const char *filename);

/* Destroy the CPC data */
void destroy_CPCData(struct CPCData *data);

/* Perform all processing on the 8bpp packed BGR input image, and generate a
   fully-corrected 16bpp YMC packed output image.
   Returns 0 if successful, non-zero for error */
int do_image_effect70(struct CPCData *cpc, struct CPCData *ecpc,
		      struct BandImage *input, struct BandImage *output,
		      int sharpen, int reverse, uint8_t rew[2]);
int do_image_effect60(struct CPCData *cpc, struct CPCData *ecpc,
		      struct BandImage *input, struct BandImage *output,
		      int sharpen, int reverse, uint8_t rew[2]);
int do_image_effect80(struct CPCData *cpc, struct CPCData *ecpc,
		      struct BandImage *input, struct BandImage *output,
		      int sharpen, int reverse, uint8_t rew[2]);

/* Converts the packed 16bpp YMC image into 16bpp YMC planes, with
   proper padding after each plane.  Calls the callback function for each
   block. */
int send_image_data(struct BandImage *out, void *context,
		    int (*callback_fn)(void *context, void *buffer, uint32_t len));

/* 3D Color Look-Up-Table */
#define COLORCONV_RGB 0
#define COLORCONV_BGR 1

#define LUT_LEN 14739

/* Forward-Declaration */
struct CColorConv3D;

/* Read the LUT off of disk */
int CColorConv3D_Get3DColorTable(uint8_t *buf, const char *filename);
/* Parse the LUT from disk */
struct CColorConv3D *CColorConv3D_Load3DColorTable(const uint8_t *ptr);
/* Destroy the LUT */
void CColorConv3D_Destroy3DColorTable(struct CColorConv3D *this);

/* Run a packed 8bpp rgb or bgr image through the LUT */
void CColorConv3D_DoColorConv(struct CColorConv3D *this, uint8_t *data, uint16_t cols, uint16_t rows, uint32_t bytes_per_row, int rgb_bgr);
/* Run a planar rgb image through the LUT */
void CColorConv3D_DoColorConvPlane(struct CColorConv3D *this, uint8_t *data_r, uint8_t *data_g, uint8_t *data_b, uint32_t planelen);

/* CP98xx family stuff */
struct mitsu98xx_data;  /* Forward declaration */

struct mitsu98xx_data *CP98xx_GetData(const char *filename);
void CP98xx_DestroyData(const struct mitsu98xx_data *data);

int CP98xx_DoConvert(const struct mitsu98xx_data *table,
		     const struct BandImage *input,
		     struct BandImage *output,
		     uint8_t type, int sharpness, int already_reversed);

/* CP-M1 family stuff */

struct M1CPCData; /* Forward-Declaration */

int M1_CLocalEnhancer(const struct M1CPCData *cpc,
		      int sharp, struct BandImage *img);
void M1_Gamma8to14(const struct M1CPCData *cpc,
		   const struct BandImage *in, struct BandImage *out);

int M1_CalcRGBRate(uint16_t rows, uint16_t cols, uint8_t *data);
uint8_t M1_CalcOpRateMatte(uint16_t rows, uint16_t cols, uint8_t *data);
uint8_t M1_CalcOpRateGloss(uint16_t rows, uint16_t cols);

struct M1CPCData *M1_GetCPCData(const char *corrtable_path,
				const char *filename,
				const char *gammafilename);
void M1_DestroyCPCData(struct M1CPCData *dat);


/* CPD30 family stuff */
struct mitsu_cpd30_data;  /* Forward declaration */

struct mitsu_cpd30_data *CPD30_GetData(const char *filename);
void CPD30_DestroyData(const struct mitsu_cpd30_data *data);

int CPD30_DoConvert(const struct mitsu_cpd30_data *table,
		     const struct BandImage *input,
		     struct BandImage *output,
		     uint8_t type, int sharpness);

#endif /* __MITSU_D70_H */
