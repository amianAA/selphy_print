/*
 *   Kodak 8800/9810 Photo Printer CUPS backend -- libusb-1.0 version
 *
 *   (c) 2021 Solomon Peachy <pizza@shaftnet.org>
 *
 *   The latest version of this program can be found at:
 *
 *     https://git.shaftnet.org/cgit/selphy_print.git
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 3 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 *   SPDX-License-Identifier: GPL-3.0+
 *
 */

#define BACKEND kodak8800_backend

#include "backend_common.h"

/* EK8800 command structures */
struct rtp1_req {
	uint8_t  hdr[4]; /* "RTP1" */
	uint8_t  cmd[4];
	uint32_t max_resplen; /* BE */
	uint32_t payload_len; /* BE */
	uint8_t  payload[];
};

struct rtp1_resp {
	uint8_t  hdr[4]; /* "RTP1" */
	uint8_t  cmd[4];
	uint8_t  sts1[2];
	uint8_t  sts2[3];
	uint8_t  zero[3];
	uint32_t payload_len; /* BE */
	uint8_t  payload[];
};

struct rtp1_counters {
	uint32_t  cutter_count;
	uint32_t  prints_finished;
	uint32_t  ribbon_head;  // units of 300dpi (ie val / 300 == inches)
	uint32_t  paper_total;  // uints of 300dpi
	uint32_t  prints_started;
};

struct rtp1_errorrecord {
	uint8_t  unk;  // 12 == user, 13 == service, 0 == invalid/terminator?
	uint16_t code;
	uint8_t  plane; // 0-3
	uint32_t printnum;
	uint32_t ribbonnum;
	uint32_t papernum;
};

#define RTP_ERROR_JOB_NOT_OPEN     0x0203
#define RTP_ERROR_COMMAND_DISABLED 0x0307
#define RTP_ERROR_RIBBON_TOO_SHORT 0x0420
#define RTP_ERROR_OPERATING_SYS    0x0503
#define RTP_ERROR_DOOR_OPEN        0x0504
#define RTP_ERROR_RIBBON_CHECK     0x2001
#define RTP_ERROR_PAPER_CHECK      0x2004
#define RTP_ERROR_ENGINE_PROTOCOL  0x4302
#define RTP_ERROR_BARCODE_SENSE    0x430A
#define RTP_ERROR_HOST_READ        0xFF02

struct rtp1_errorlog {
	struct rtp1_errorrecord row[32];
};

struct rtp1_mediastatus {
	uint8_t  unk[4]; // 00 01 00 02  00 01 00 01
	                 // 00 03 00 06  00 01 00 01 <<-- error state?
	                 // 00 ff 00 02  00 01 00 01
	uint16_t media_type; // 00 01  (YMCX 8x10 Glossy)
	uint16_t paper_type; // 00 01  (8")
	uint32_t ribbon_remain; /* inches */  // also seen 00 0b ff f4 for error state
	uint32_t paper_remain;  /* in inches */
};

/* These are guesses */
#define PAPER_TYPE_7      0x01
#define PAPER_TYPE_8      0x02
#define PAPER_TYPE_8_5    0x03
#define PAPER_TYPE_A4     0x04

#define MEDIA_TYPE_8x10_G 0x01
#define MEDIA_TYPE_8x10_M 0x02
#define MEDIA_TYPE_8x12_G 0x03
#define MEDIA_TYPE_8x12_M 0x04

struct rtp1_serial {
	uint8_t  serial[64];
};

struct rtp1_mfgmodel {
	uint8_t  mfg[64];
	uint8_t  model[64];
	uint8_t  serial[64];
	uint8_t  fwver[64];
};

const uint8_t rtp_sendimagedata[4] = { 0x00, 0x00, 0x00, 0x00 }; /* Resp len 0 */
const uint8_t rtp_getmaxxfer[4]    = { 0x01, 0x00, 0x00, 0x00 }; /* Resp len 4 (u32) */
const uint8_t rtp_printfooter[4]   = { 0x11, 0x00, 0x00, 0x00 }; /* Resp len 0 */
const uint8_t rtp_getserial[4]     = { 0x81, 0x01, 0x00, 0x00 }; /* Resp len 64 (rtp1_serial) */
const uint8_t rtp_getserialhead[4] = { 0x81, 0x01, 0x01, 0x00 }; /* Resp len 64 (rtp1_serial) */
const uint8_t rtp_getmfgmodel[4]   = { 0x13, 0x00, 0x00, 0x00 }; /* Resp len 256 (rtp1_mfgmodel) */
const uint8_t rtp_getcounters[4]   = { 0x81, 0x04, 0x00, 0x00 }; /* Resp len 20 (rtp1_counters) */
const uint8_t rtp_getmedia[4]      = { 0x06, 0x40, 0x00, 0x00 }; /* Resp len 16 (rtp1_mediastatus) */
const uint8_t rtp_getusererrors[4] = { 0x0c, 0x01, 0x00, 0x00 }; /* Resp len 512 (rtp1_errorlog) */
const uint8_t rtp_getserverrors[4] = { 0x0c, 0x02, 0x00, 0x00 }; /* Resp len 512 (rtp1_errorlog) */
const uint8_t rtp_getifaceerrors[4] = { 0x0c, 0x03, 0x00, 0x00 }; /* Resp len 512 (rtp1_errorlog) */

/* Unknowns */
const uint8_t rtp_unknown1[4]      = { 0x06, 0x00, 0x00, 0x00 }; /* Resp len 0 */
  // Read Status?
const uint8_t rtp_unknown2[4]      = { 0x06, 0x80, 0x00, 0x00 }; /* Resp len 8 */
  // -> 02 00 06 2a ff 00 1d 27
  // -> 02 00 06 2a ff 00 1e 27
  // -> 10 00 02 2a ff 00 26 27
const uint8_t rtp_unknown3[4]      = { 0x10, 0x00, 0x00, 0x00 }; /* Resp len 4 */
  // -> 00 00 00 01    Seen prior to sending data to printer.  status/ready?
  //  ReadCapabilities?
const uint8_t rtp_unknown4[4]      = { 0x06, 0x03, 0x00, 0x00 }; /* Req len 4, resp 14?? */

// *** XXX head temp (&target), ribbon type, paper type, cut & page alignment, media total
//     plus state of cutter, cover, ribbon panel position, etc..

struct rosetta_header {
	uint8_t  esc;
	uint8_t  hdr[15]; /* "MndROSETTA V001" */
	uint8_t  payload[44];
};

struct rosetta_block {
	uint8_t  esc;
	uint8_t  cmd[19]; /* ascii, space-padded */
	uint8_t  zero[4];
	uint32_t payload_len; /* BE */
	uint8_t  payload[];
};

/* Private data structure */
struct kodak8800_printjob {
	size_t jobsize;
	int copies;

	int copies_offset;
	uint8_t *databuf;
};

struct kodak8800_ctx {
	struct dyesub_connection *conn;

	uint8_t jobid;

	char serial[32];
	char fwver[32];

	struct marker marker;
};

/* Helper Functions */
static int rtp1_docmd(struct kodak8800_ctx *ctx, const uint8_t *cmd,
		      const uint8_t *payload, uint32_t payload_len,
		      uint32_t maxresp_len, uint8_t *respbuf)
{
	int ret;
	int num;
	struct rtp1_req req;
	struct rtp1_resp resp;

	/* Fill in header */
	req.hdr[0] = 'R';
	req.hdr[1] = 'T';
	req.hdr[2] = 'P';
	req.hdr[3] = '1';
	memcpy(req.cmd, cmd, sizeof(req.cmd));
	req.max_resplen = cpu_to_be32(maxresp_len);
	req.payload_len = cpu_to_be32(payload_len);

	/* Send over cmd structure */
	if ((ret = send_data(ctx->conn, (uint8_t*) &req,
			     sizeof(req))) != 0)
		return ret;

	/* Send over payload, if any */
	if (payload_len)
		if ((ret = send_data(ctx->conn, payload,
				     payload_len)) != 0)
			return ret;

	/* Read response header */
	ret = read_data(ctx->conn, (uint8_t*) &resp,
			sizeof(resp), &num);
	if (num != (int)sizeof(resp)) {
		ERROR("Short Read! (%d/%d)\n", num, (int)sizeof(resp));
		ret = -4;
		goto done;
	}

	// XXX check response header!  pass up to higher level?

	/* Read response payload, if anything */
	resp.payload_len = be32_to_cpu(resp.payload_len);
	if (resp.payload_len > maxresp_len || (maxresp_len && !respbuf)) {
		ERROR("Oversize response (%d/%d)\n", resp.payload_len, maxresp_len);
		return CUPS_BACKEND_FAILED;
	}
	ret = read_data(ctx->conn, (uint8_t*) respbuf,
			resp.payload_len, &num);
	if (num != (int) resp.payload_len) {
		ERROR("Short Read! (%d/%d)\n", num, (int)resp.payload_len);
		ret = -4;
		goto done;
	}

done:
	return ret;
}

static int rtp1_getmaxxfer(struct kodak8800_ctx *ctx, uint32_t *maxlen)
{
	int ret;

	ret = rtp1_docmd(ctx, rtp_getmaxxfer, NULL, 0, 4, (uint8_t*)maxlen);

	*maxlen = be32_to_cpu(*maxlen);

	return ret;
}

static void kodak8800_cmdline(void)
{
}

static int kodak8800_cmdline_arg(void *vctx, int argc, char **argv)
{
	struct kodak8800_ctx *ctx = vctx;
	int i, j = 0;

	if (!ctx)
		return -1;

	while ((i = getopt(argc, argv, GETOPT_LIST_GLOBAL "")) >= 0) {
		switch(i) {
		GETOPT_PROCESS_GLOBAL
		default:
			break;  /* Ignore completely */
		}

		if (j) return j;
	}

	return CUPS_BACKEND_OK;
}

static void *kodak8800_init(void)
{
	struct kodak8800_ctx *ctx = malloc(sizeof(struct kodak8800_ctx));
	if (!ctx) {
		ERROR("Memory Allocation Failure\n");
		return NULL;
	}
	memset(ctx, 0, sizeof(struct kodak8800_ctx));

	return ctx;
}

static int kodak8800_query_mfgmodel(struct kodak8800_ctx *ctx);

static int kodak8800_attach(void *vctx, struct dyesub_connection *conn, uint8_t jobid)
{
	struct kodak8800_ctx *ctx = vctx;
	int ret;
	struct rtp1_mediastatus media;

	ctx->conn = conn;

        /* Ensure jobid is sane */
        ctx->jobid = jobid & 0x7f;
	if (!ctx->jobid)
		ctx->jobid++;

	if (test_mode < TEST_MODE_NOATTACH) {
		ret = kodak8800_query_mfgmodel(ctx);
		if (ret)
			return CUPS_BACKEND_FAILED;
		ret = rtp1_docmd(ctx, rtp_getmedia, NULL, 0, sizeof(media), (uint8_t*)&media);
		if (ret)
			return CUPS_BACKEND_FAILED;

		media.ribbon_remain = be32_to_cpu(media.ribbon_remain);
		media.paper_remain = be32_to_cpu(media.paper_remain);
		media.media_type = be16_to_cpu(media.media_type);
	} else {
		strcpy(ctx->fwver, "0.0");
		strcpy(ctx->serial, "12345");
		media.ribbon_remain = 92;
		media.paper_remain = 24;
		media.media_type = MEDIA_TYPE_8x10_G;
// XXX		media.unk[0..4]?
	}

	media.ribbon_remain /= 4;
	if (media.ribbon_remain < media.paper_remain)
		ctx->marker.levelnow = media.ribbon_remain;
	else
		ctx->marker.levelnow = media.paper_remain;

	ctx->marker.color = "#00FFFF#FF00FF#FFFF00";
	ctx->marker.numtype = media.media_type;

	if (media.media_type == MEDIA_TYPE_8x10_G) {
		ctx->marker.name = "8810S (8x10)";
		ctx->marker.levelmax = 300;
	} else {
		ctx->marker.name = "8810L (8x12)";
		ctx->marker.levelmax = 250;
	}

	return CUPS_BACKEND_OK;
}

static void kodak8800_cleanup_job(const void *vjob)
{
	const struct kodak8800_printjob *job = vjob;

	if (job->databuf)
		free(job->databuf);

	free((void*)job);
}

static int kodak8800_read_parse(void *vctx, const void **vjob, int data_fd, int copies) {
	struct kodak8800_ctx *ctx = vctx;
	int ret;

	struct kodak8800_printjob *job = NULL;

	if (!ctx)
		return CUPS_BACKEND_FAILED;

	job = malloc(sizeof(*job));
	if (!job) {
		ERROR("Memory allocation failure!\n");
		return CUPS_BACKEND_RETRY_CURRENT;
	}
	memset(job, 0, sizeof(*job));

	/* Read Rosetta data */
	job->databuf = malloc(sizeof(struct rosetta_header));
	if (!job->databuf) {
		ERROR("Memmory allocation failure!\n");
		return CUPS_BACKEND_RETRY;
	}

	/* Read rosetta header */
	ret = read(data_fd, job->databuf, sizeof(struct rosetta_header));
	if (ret < 0 || ret != sizeof(struct rosetta_header)) {
		if (ret == 0) {
			kodak8800_cleanup_job(job);
			return CUPS_BACKEND_CANCEL;
		}
		perror("ERROR: read failed");
		return CUPS_BACKEND_CANCEL;
	}
	job->jobsize += sizeof(struct rosetta_header);

	/* Sanity check header */
	if (memcmp(job->databuf, "\x1bMndROSETTA V001", 16)) {
		ERROR("Invalid ROSETTA header\n");
		kodak8800_cleanup_job(job);
		return CUPS_BACKEND_CANCEL;
	}

	/* Reallocate databuf */
	job->databuf = realloc(job->databuf, 2624*3624*3+4*1024); // XXX better solution here?
	if (!job->databuf) {
		ERROR("Memmory allocation failure!\n");
		kodak8800_cleanup_job(job);
		return CUPS_BACKEND_RETRY;
	}
	/* Read in the data blocks */
	while (1) {
		struct rosetta_block *block = (struct rosetta_block *)(job->databuf + job->jobsize);
		uint32_t payload_len = 0;

		/* Read in block header */
		ret = read(data_fd, block, sizeof(struct rosetta_block));
		if (ret < 0 || ret != sizeof(struct rosetta_block)) {
			if (ret == 0) {
				kodak8800_cleanup_job(job);
				return CUPS_BACKEND_CANCEL;
			}
			perror("ERROR: read failed");
			return CUPS_BACKEND_CANCEL;
		}
		payload_len = be32_to_cpu(block->payload_len);
//		INFO("block %d @ %d \n", payload_len + sizeof(struct rosetta_block), job->jobsize);

		/* Read in block payload */
		ret = read(data_fd, block->payload, payload_len);
		if (ret < 0 || ret != (int) payload_len) {
			if (ret == 0) {
				kodak8800_cleanup_job(job);
				return CUPS_BACKEND_CANCEL;
			}
			perror("ERROR: read failed");
			return CUPS_BACKEND_CANCEL;
		}
		job->jobsize += sizeof(struct rosetta_block);
		job->jobsize += payload_len;

		/* Work out our copies offset */
		if (!memcmp(block->cmd, "FlsPgCopies", 11)) {
			job->copies_offset = job->jobsize - payload_len;
		}
		/* If this is the last block, we're done! */
		if (!memcmp(block->cmd, "MndEndJob", 9))
			break;
	}

	/* Handle copies */
	if (job->copies_offset) {
		uint32_t tmp = 0;
		memcpy(&tmp, job->databuf + job->copies_offset, sizeof(tmp));
		tmp = be32_to_cpu(tmp);
		if ((int)tmp < copies) {
			tmp = be32_to_cpu(copies);
			memcpy(job->databuf + job->copies_offset, &tmp, sizeof(tmp));
		}
	}

	job->copies = copies;
	*vjob = job;

	return CUPS_BACKEND_OK;
}

static int kodak8800_main_loop(void *vctx, const void *vjob) {
	struct kodak8800_ctx *ctx = vctx;

	int ret;

	const struct kodak8800_printjob *job = vjob;

	if (!ctx)
		return CUPS_BACKEND_FAILED;
	if (!job)
		return CUPS_BACKEND_FAILED;

	// query printer for shit

	INFO("Waiting for printer idle\n");

	// update marker levels?

	// RTP1 status etc?

	INFO("Sending image data\n");
	/* Sent over data blocks */
	uint32_t offset = 0;
	while (offset < job->jobsize) {
		uint32_t max_blocksize;
		ret = rtp1_getmaxxfer(ctx, &max_blocksize);
		if (ret)
			return ret;

		ret = rtp1_docmd(ctx, rtp_sendimagedata,
				 job->databuf + offset, max_blocksize,
				 0, NULL);
		if (ret)
			return ret;
	}
	/* Send payload footer */
	ret = rtp1_docmd(ctx, rtp_printfooter,
			 NULL, 0, 0, NULL);
	if (ret)
		return ret;

	INFO("Waiting for printer to acknowledge completion\n");

	// RTP1 status etc?

	// update marker levels?

	INFO("Print complete\n");

	return CUPS_BACKEND_OK;
}

static int kodak8800_query_serno(struct dyesub_connection *conn, char *respbuf, int buf_len)
{
	uint8_t buf[64];
	int ret;

	struct kodak8800_ctx ctx = {
		.conn = conn,
	};

	ret = rtp1_docmd(&ctx, rtp_getserial, NULL, 0, sizeof(buf), buf);

	if (!ret)
		memcpy(respbuf, buf, buf_len);

	return ret;
}

static int kodak8800_query_mfgmodel(struct kodak8800_ctx *ctx)
{
	uint8_t buf[256];
	int ret;

	ret = rtp1_docmd(ctx, rtp_getmfgmodel, NULL, 0, sizeof(buf), buf);

	if (!ret) {
		memcpy(ctx->serial, buf + 64*2, sizeof(ctx->serial));
		memcpy(ctx->fwver, buf + 64*3, sizeof(ctx->fwver));
	}

	return ret;
}

static int kodak8800_query_markers(void *vctx, struct marker **markers, int *count)
{	struct kodak8800_ctx *ctx = vctx;
	struct rtp1_mediastatus media;
	int ret;

	ret = rtp1_docmd(ctx, rtp_getmedia, NULL, 0, sizeof(media), (uint8_t*)&media);
	if (ret)
		return CUPS_BACKEND_FAILED;

	media.ribbon_remain = be32_to_cpu(media.ribbon_remain);
	media.paper_remain = be32_to_cpu(media.paper_remain);

	media.ribbon_remain /= 4;
	if (media.ribbon_remain < media.paper_remain)
		ctx->marker.levelnow = media.ribbon_remain;
	else
		ctx->marker.levelnow = media.paper_remain;

	*markers = &ctx->marker;
	*count = 1;

	return CUPS_BACKEND_OK;
}

static int kodak8800_query_stats(void *vctx, struct printerstats *stats)
{
	struct kodak8800_ctx *ctx= vctx;
	struct rtp1_counters counters;
	int ret;

	stats->mfg = "Kodak";
	stats->model = "8800/9810";

	if (kodak8800_query_mfgmodel(ctx))
		return CUPS_BACKEND_FAILED;

	stats->serial = ctx->serial;
	stats->fwver = ctx->fwver;

	stats->decks = 1;
	stats->mediatype[0] = ctx->marker.name;
	stats->levelmax[0] = ctx->marker.levelmax;
	stats->levelmax[0] = ctx->marker.levelnow;
	stats->name[0] = "Roll";
	stats->status[0] = "Unknown"; // XXX

	ret = rtp1_docmd(ctx, rtp_getcounters, NULL, 0, sizeof(counters), (uint8_t*) &counters);
	if (!ret)
		return ret;
	stats->cnt_life[0] = be32_to_cpu(counters.prints_finished);

	return CUPS_BACKEND_OK;
}


static const char *kodak8800_prefixes[] = {
	"kodak8800", // Family driver, do NOT nuke!
	NULL
};

/* Exported */
const struct dyesub_backend kodak8800_backend = {
	.name = "Kodak 8800/9810",
	.version = "0.01WIP",
	.uri_prefixes = kodak8800_prefixes,
	.cmdline_usage = kodak8800_cmdline,
	.cmdline_arg = kodak8800_cmdline_arg,
	.init = kodak8800_init,
	.attach = kodak8800_attach,
	.cleanup_job = kodak8800_cleanup_job,
	.read_parse = kodak8800_read_parse,
	.main_loop = kodak8800_main_loop,
	.query_serno = kodak8800_query_serno,
	.query_markers = kodak8800_query_markers,
	.query_stats = kodak8800_query_stats,
	.devices = {
		{ 0x040a, 0x4023, P_KODAK_8800, "Kodak", "kodak-8800"},
		{ 0x040a, 0x4023, P_KODAK_8800, "Kodak", "kodak-9810"}, // duplicate
		{ 0, 0, 0, NULL, NULL}
	}
};

/*

 ************* Kodak 8800/9810 Spool format

    General format for blocks (except initial header)
    All multi-byte fields are BIG ENDIAN

   1b xx xx xx xx xx xx xx  xx xx xx xx xx xx xx xx
   xx xx xx xx 00 00 00 00  NN NN NN NN

     [ followed by NN bytes of payload ]
     [ xx is ASCII field name, space (0x20) padded ]

    File header (fixed)

 1b 4d 6e 64 52 4f 53 45  54 54 41 20 56 30 30 31  'MndROSETTA V001'
 2e 30 30 31 30 30 30 30  30 30 32 30 35 32 35 30
 37 32 36 39 36 45 37 34  36 35 37 32 34 32 36 39
 36 45 34 44 36 46 37 34  37 32 36 43

    Job header (fixed)

 1b 4d 6e 64 42 67 6e 4a  6f 62 20 20 50 72 69 6e  'MndBgnJob  Print   '
 74 20 20 20 00 00 00 00  00 00 00 08 56 30 30 31
 2e 30 30 30

    Job Settings start (fixed)

 1b 46 6c 73 53 72 74 4a  62 44 65 66 53 65 74 75  'FlsSrtJbDefSetup   '
 70 20 20 20 00 00 00 00  00 00 00 00

    Job Media selection

 1b 46 6c 73 4a 62 4d 6b  4d 65 64 20 4e 61 6d 65  'FlsJbMkMed Name    '
 20 20 20 20 00 00 00 00  00 00 00 40 59 4d 43 58
 20 38 78 31 32 20 47 6c  6f 73 73 79 00 00 00 00
 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  :: @YMCX 8x12 Glossy
 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
 00 00 00 00 00 00 00 00  00 00 00 00

      ~~ or:

 1b 46 6c 73 4a 62 4d 6b  4d 65 64 20 4e 61 6d 65
 20 20 20 20 00 00 00 00  00 00 00 40 59 4d 43 58
 20 38 78 31 30 20 47 6c  6f 73 73 79 00 00 00 00  :: @YMCX 8x10 Glossy
 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
 00 00 00 00 00 00 00 00  00 00 00 00

    Page Media Selection  (fixed at 8")

 1b 46 6c 73 50 67 4d 65  64 69 61 20 4e 61 6d 65  'FlsPgMedia Name    '
 20 20 20 20 00 00 00 00  00 00 00 40 XX XX XX XX  '7"' '8"' '8.5"' 'A4'
 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
 00 00 00 00 00 00 00 00  00 00 00 00

    Lamination

 1b 46 6c 73 4a 62 4c 61  6d 20 20 20 XX XX XX 20  'FlsJbLam   ???     '
 20 20 20 20 00 00 00 00  00 00 00 00

                       XX XX XX is '4f 66 66' (Off) or '4f 6e 20' (On)

    Job Settings end (fixed)

 1b 46 6c 73 53 74 70 4a  62 44 65 66 20 20 20 20  'FlsStpJbDef        '
 20 20 20 20 00 00 00 00  00 00 00 00

    Page start (fixed)

 1b 4d 6e 64 42 67 6e 4c  50 61 67 65 4e 6f 72 6d  'MndBgnLPageNormal  '
 61 6c 20 20 00 00 00 00  00 00 00 04 00 00 00 01

    Page parameters

 1b 4d 6e 64 53 65 74 4c  50 61 67 65 20 20 20 20  'MdnSetLPage        '
 20 20 20 20 00 00 00 00  00 00 00 08 00 00 XX XX  XX XX == 09 A0 (2464)
 00 00 YY YY                                       YY YY == 0E 28 (3624) 8x12
                                                         == 0B D0 (3024) 8x10

    Image size

 1b 4d 6e 64 49 6d 53 70  65 63 20 20 53 69 7a 65  'MndImSpec  Size    '
 20 20 20 20 00 00 00 00  00 00 00 10 00 00 09 a0  XX XX, YY YY (see above)
 00 00 YY YY 00 00 XX XX  00 00 00 00

    Image Position on page (fixed @ 0x0)

 1b 46 6c 73 49 6d 50 6f  73 69 74 6e 53 70 65 63  'FlsImPositnSpecify '
 69 66 79 20 00 00 00 00  00 00 00 08 00 00 00 00
 00 00 00 00

    Image Sharpening:

 1b 46 6c 73 49 6d 53 68  61 72 70 20 53 65 74 4c  'FlsImSharp SetLevel'
 65 76 65 6c 00 00 00 00  00 00 00 02 ff SS        SS == 12 (Normal)
                                                      == 0  (None)
                                                      == 19 (High)
    Page Copies:

 1b 46 6c 73 50 67 43 6f  70 69 65 73 20 20 20 20  'FlsPgCopies        '
 20 20 20 20 00 00 00 00  00 00 00 04 00 00 00 NN  NN == Copies (01+)

    Other settings (fixed for now)

 1b 46 6c 73 50 67 4d 69  72 72 6f 72 4e 6f 6e 65  'FlsPgMirrorNone    '
 20 20 20 20 00 00 00 00  00 00 00 00

 1b 46 6c 73 50 67 52 6f  74 61 74 65 4e 6f 6e 65  'FlsPgRotateNone    '
 20 20 20 20 00 00 00 00  00 00 00 00

    Cut list

 1b 46 6c 73 43 75 74 4c  69 73 74 20 20 20 20 20  'FlsCutList         '
 20 20 20 20 00 00 00 00  00 00 NN NN [ NN * 2 ]

   8x10:          00 04 00 0c 0b c4
   8x10div2:      00 06 00 0c 05 e8 0b c4
   8x10div2slug2: 00 06 00 0c 05 d5 05 fb 0b c4
   8x12:          00 04 00 0c 0e 1c
   8x12div2:      00 06 00 0c 07 14 0e 1c
   8x12div2slug2: 00 06 00 0c 07 01 07 27 0e 1c

    Image Plane data block

 1b 46 6c 73 44 61 74 61  20 20 20 20 42 6c 6f 63  'FlsData    Block   '
 6b 20 20 20 00 00 00 00  LL LL LL LL              LL LL LL LL == plane len 32BE
 49 6d 61 67 65 20 20 20                           'Image   '

    [[[ plane data, LLLLLLLL - 8 bytes ]]]

 1b 46 6c 73 44 61 74 61  20 20 20 20 42 6c 6f 63  'FlsData    Block   '
 6b 20 20 20 00 00 00 00  LL LL LL LL
 49 6d 61 67 65 20 20 20                           'Image   '

    [[[ plane data, LLLLLLLL - 8 bytes ]]]

 1b 46 6c 73 44 61 74 61  20 20 20 20 42 6c 6f 63  'FlsData    Block   '
 6b 20 20 20 00 00 00 00  LL LL LL LL
 49 6d 61 67 65 20 20 20                           'Image   '

    [[[ plane data, LLLLLLLL -8 bytes ]]]

    End Page (fixed)

 1b 4d 6e 64 45 6e 64 4c  50 61 67 65 20 20 20 20  'MndEndLPage        '
 20 20 20 20 00 00 00 00  00 00 00 00

    End Job (fixed)

 1b 4d 6e 64 45 6e 64 4a  6f 62 20 20 20 20 20 20  'MndEndJob          '
 20 20 20 20 00 00 00 00  00 00 00 00

 *************** Basic command format:  (all multi-bytes are BIG ENDIAN)

>  52 54 50 31 xx xx xx xx  yy yy yy yy l1 l1 l1 l1
>> [ l1l1l1l1 bytes of payload ]
<  52 54 40 31 xx xx xx xx  ss ss s2 s2 s2 00 00 00  l2 l2 l2 l2
<< [ l2l2l2l2 bytes of payload ]

     xx xx xx xx == command
     yy yy yy yy == response buffer length (ie l2l2l2l2 must be <= yyyyyyyy)
     ss ss == status message (10 10 is ok, 10 12 ok, dunno about errors)
     s2 s2 s2 == secondary status (seen 00 00 00, 00 00 02, 05 04 02)

 Specific commands seen when printing:

 52 54 50 31 [....]  "RTP1"

   media status:

>            06 40 00 00  00 10 00 00 00 00 00 00
<            06 40 00 00  10 10 00 00 00 00 00 00  00 00 00 10
<<                        00 01 00 02 00 01 00 01  00 00 04 a4 00 00 01 20

   max xfer len:
>            01 00 00 00  00 00 00 04 00 00 00 00
<            01 00 00 00  10 10 00 00 02 00 00 00  00 00 00 04
<<                        00 18 00 00

   media status:
>            06 40 00 00  00 10 00 00 00 00 00 00
<            06 40 00 00  10 10 00 00 00 00 00 00  00 00 00 10
<<                        00 01 00 02 00 01 00 01  00 00 04 a4 00 00 01 20

   some sort of status?
>            10 00 00 00  00 00 00 04 00 00 00 00
<            10 00 00 00  10 10 00 00 00 00 00 00  00 00 00 04
<<                        00 00 00 01

 [ Each data block gets this sequence ]

>            01 00 00 00  00 00 00 04 00 00 00 00
<            01 00 00 00  10 10 00 00 02 00 00 00  00 00 00 04
<<                        00 18 00 00  <-- Max available xfer buffer size!
>            00 00 00 00  00 00 00 00 ll ll ll ll
>>            [ llllllll bytes of data, in 4K transfers, up to max size]
<            00 00 00 00  10 10 00 00 02 00 00 00  00 00 00 00

 [ after final block ]

>            01 00 00 00  00 00 00 04 00 00 00 00
<            01 00 00 00  10 10 00 00 02 00 00 00  00 00 00 04
<<                        00 00 00 00 <-- finished?

 [ footer ]

>            11 00 00 00  00 00 00 00 00 00 00 00
<            11 00 00 00  10 10 00 00 02 00 00 00  00 00 00 00

 **** COMMANDS:

    Query Printer Serial Number (Matches what's in IEEE1284) =~ 98C070902445

>            81 01 00 00  00 00 ff ff 00 00 00 00
<            81 01 00 00  10 12 05 04 01 00 00 00  00 00 00 40
<<                        39 38 43 30 37 30 39 30  32 34 34 35 00 00 00 00
<<                        00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
<<                        00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
<<                        00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00

        Serial number is ASCII, null-terminated, 64 byte payload

    Query Head Serial Number (??) =~ 6J3867

>            81 01 01 00  00 00 ff ff 00 00 00 00
<            81 01 01 00  10 12 05 04 01 00 00 00  00 00 00 40
<<                        39 38 43 30 37 30 39 30  32 34 34 35 00 00 00 00
<<                        00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
<<                        00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
<<                        00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00

        Serial number is ASCII, null-terminated, 64 byte payload

    Query Mfg, Model, Serial & Firmware info (??)

>            13 00 00 00  00 00 01 00 00 00 00 00
<            13 00 00 00  10 12 05 04 01 00 00 00  00 00 01 00
<<                        45 61 73 74 6d 61 6e 20  4b 6f 64 61 6b 20 43 6f
<<                        6d 70 61 63 79 00 20 20  20 20 20 20 20 20 20 20
<<                        20 20 20 20 20 20 20 20  20 20 20 20 20 20 20 20
<<                        20 20 20 20 20 20 20 20  20 20 20 20 20 20 20 20

		[ followed by three more 64 byte sections ]

        Each section is ASCII, null-terminated/padded, 64 byte payload
        4 sections: Manufacturer, Model, Serial, FW Version

    Query FW sub-versions

>            13 80 00 00  00 00 00 3c 00 00 00 00
<            13 80 00 00  10 12 05 04 01 00 00 00  00 00 00 3c
<<                        39 38 43 30 37 30 39 30  32 34 34 35 00 00 00 00
<<                        00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
<<                        00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
<<                        00 00 00 00 00 00 00 00  00 00 00 00 00

        Five blocks of 12 bytes, not always null-terminated!
        DSP, ??, System, Head, Reset

 */
