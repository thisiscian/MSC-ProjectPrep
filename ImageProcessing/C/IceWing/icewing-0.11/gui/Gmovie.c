/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Frank Loemker
 *
 * Copyright (C) 1999-2009
 * Frank Loemker, Applied Computer Science, Faculty of Technology,
 * Bielefeld University, Germany
 *
 * This file is part of iceWing, a graphical plugin shell.
 *
 * iceWing is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * iceWing is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tools/tools.h"
#include "Gimage_i.h"

#include "avi/avilib.h"
#include "Gcolor.h"
#include "Gtools.h"
#ifdef WITH_JPEG
#include "avi/jpegutils.h"
#endif

#ifdef WITH_FFMPEG
#ifdef FFMPEG_LIBINC
#  include "libavutil/mathematics.h"
#  include "libavcodec/avcodec.h"
#  include "libavformat/avformat.h"
#  include "libswscale/swscale.h"
#else
#  include "ffmpeg/mathematics.h"
#  include "ffmpeg/avcodec.h"
#  include "ffmpeg/avformat.h"
#  include "ffmpeg/swscale.h"
#endif

#if LIBAVUTIL_VERSION_INT > ((50<<16)+(200<<8))
#  define AVERROR_NOFMT			AVERROR_INVALIDDATA
#endif
#if LIBAVUTIL_VERSION_INT < ((50<<16)+(14<<8))
#  define AVMEDIA_TYPE_VIDEO	CODEC_TYPE_VIDEO
#endif
#if LIBAVUTIL_VERSION_INT < ((49<<16)+2)
#  define PIX_FMT_GRAY16		(PIX_FMT_NB+100)
#  define PIX_FMT_GRAY16LE		PIX_FMT_GRAY16
#  define PIX_FMT_GRAY16BE		PIX_FMT_GRAY16
#endif

#if LIBAVCODEC_VERSION_INT < ((53<<16)+(8<<8))
#  define avcodec_open2(cc,c,o)	avcodec_open(cc,c)
#endif
#if LIBAVCODEC_VERSION_INT < ((52<<16)+(30<<8)+2)
#  define AV_PKT_FLAG_KEY		PKT_FLAG_KEY
#endif

#if LIBAVFORMAT_VERSION_INT < ((53<<16)+(17<<8))
#  define avformat_new_stream(f,c) av_new_stream(f,0)
#endif
#if LIBAVFORMAT_VERSION_INT < ((53<<16)+(6<<8))
#  define avformat_find_stream_info(c,o) av_find_stream_info(c)
#endif
#if LIBAVFORMAT_VERSION_INT < ((52<<16)+(103<<8))
#  define avio_open				url_fopen
#  define avio_close			url_fclose
#  define AVIO_FLAG_WRITE		URL_WRONLY
#elif LIBAVFORMAT_VERSION_INT < ((53<<16)+(1<<8))
#  define AVIO_FLAG_WRITE		AVIO_WRONLY
#endif
#if LIBAVFORMAT_VERSION_INT < ((52<<16)+(101<<8))
#  define av_dump_format		dump_format
#endif
#if LIBAVFORMAT_VERSION_INT < ((52<<16)+(45<<8))
#  define av_guess_format		guess_format
#endif
#if LIBAVFORMAT_VERSION_INT < ((52<<16)+(26<<8))
#  define avformat_alloc_context av_alloc_format_context
#endif
#if LIBAVFORMAT_VERSION_INT < ((53<<16)+(25<<8))
#  define avformat_close_input(s) av_close_input_file(*s)
#endif
#endif

#define EVEN_PROD(w,h)	((((w)+1)&(~1)) * (((h)+1)&(~1)))

typedef struct iwMovieIn {
	int fnum_ret;				/* Number of the frame returned by iw_movie_read() */
	iwImage *image;				/* Last (ffmpeg: previously) read frame */

	avi_t *AVI;

#ifdef WITH_FFMPEG
	AVFormatContext *formatCtx;
	int videoStream;
	AVFrame	*frame;				/* Frame read from file, frame data allocated by ffmpeg */
	AVFrame	*frameYUV;			/* Converted frame, frameYUV.data == image2.data */
	int fnum;					/* Frame number of image, starting at 0 */
	int fnum_start;				/* Frame number of first frame */
	AVRational r_frate;			/* Rational frame rate */

	iwImage *image2;			/* Last read frame */
	int fnum2;					/* Frame number of image2 */
	struct SwsContext *convert_ctx;	/* Context for sws_scale */
#endif
	int fcount;					/* Number of frames in video */
	double framerate;

} iwMovieIn;

typedef struct iwMovieOut {
	iwImgFormat format;

	avi_t *AVI;
#ifdef WITH_FFMPEG
	AVFormatContext *formatCtx;
	AVStream *stream;			/* Video output stream */
	AVFrame	*frame;				/* To be written frame */
	uint8_t *outbuf;			/* Buffer for encoded frame */
	int outbuf_size;			/* Size of buffer for encoded frame */
#endif
} iwMovieOut;

struct _iwMovie {
	gboolean in;
	union {
		iwMovieIn i;
		iwMovieOut o;
	} io;
};

/*********************************************************************
  Convert img to YUV444P (IW_IMG_FORMAT_AVI_FFV1), YVU444P
  (IW_IMG_FORMAT_AVI_RAW444), or YUV420P and save in dst
  (which is of size width x height).
*********************************************************************/
void iw_img_convert (const iwImage *img, guchar **dst, int width, int height,
					 iwImgFormat format)
{
	int w, h, xs, ys;
	int colsize, colskip;
	guchar *r, *g, *b, y, u, v;
	guchar *yd = dst[0], *ud = dst[1], *vd = dst[2];
	void (*c_trans) (guchar s1, guchar s2, guchar s3, guchar *r, guchar *g, guchar *b) = NULL;

	w = width;
	h = height;
	if (format == IW_IMG_FORMAT_AVI_RAW444 || format == IW_IMG_FORMAT_AVI_FFV1)
		colsize = w*h;
	else
		colsize = EVEN_PROD(w,h)/4;

	if (img->width < w)
		w = img->width;
	if (img->height < h)
		h = img->height;
	/* -> w, h: To be copied width/height */

	/* Convert to YUV420P / YUV444P and crop/extend to destination size */
	if (img->planes < 3 && img->ctab <= IW_COLFORMAT_MAX) {
		/* Gray source image */
		for (ys=0; ys<h; ys++)
			memcpy (dst[0]+ys*width, img->data[0]+ys*img->width, w);
		if (w < width) {
			for (ys=0; ys<h; ys++)
				memset (w+dst[0]+ys*width, 0, width-w);
		}
		if (h < height)
			memset (dst[0]+h*width, 0, (height-h)*width);
		memset (dst[1], 128, colsize);
		memset (dst[2], 128, colsize);
	} else if (img->ctab == IW_YUV && !img->rowstride
			   && format != IW_IMG_FORMAT_AVI_RAW444
			   && format != IW_IMG_FORMAT_AVI_FFV1) {
		for (ys=0; ys<h; ys++)
			memcpy (dst[0]+ys*width, img->data[0]+ys*img->width, w);
		if (w < width) {
			for (ys=0; ys<h; ys++)
				memset (w+dst[0]+ys*width, 0, width-w);
		}
		if (h < height)
			memset (dst[0]+h*width, 0, (height-h)*width);

		colskip = ((width+1)&(~1))/2 - ((w+1)&(~1))/2;
		for (ys=0; ys<h/2; ys++) {
			g = img->data[1] + ys*2*img->width;
			b = img->data[2] + ys*2*img->width;
			ud = dst[1] + ys*width/2;
			vd = dst[2] + ys*width/2;
			for (xs=w/2; xs>0; xs--) {
				*ud++ = *g;
				*vd++ = *b;
				g += 2;
				b += 2;
			}
			if (w < width) {
				memset (ud, 128, colskip);
				memset (vd, 128, colskip);
			}
		} /* for ys */
		if (h < height) {
			colsize = dst[1] + colsize - ud;
			memset (ud, 128, colsize);
			memset (vd, 128, colsize);
		}
	} else {
		if (img->ctab == IW_HSV)
			c_trans = prev_hsvToRgb;

		r = img->data[0];
		g = img->data[1];
		b = img->data[2];
		if (format == IW_IMG_FORMAT_AVI_RAW444 || format == IW_IMG_FORMAT_AVI_FFV1)
			colskip = width - w;
		else
			colskip = ((width+1)&(~1))/2 - ((w+1)&(~1))/2;

		for (ys=0; ys<h; ys++) {
			for (xs=0; xs<w; xs++) {
				if (img->rowstride) {
					if (img->ctab == IW_YUV) {
						y = *r++;
						u = *r++;
						v = *r++;
					} else if (img->ctab > IW_COLFORMAT_MAX) {
						prev_inline_rgbToYuvCal (img->ctab[*r][0],
												 img->ctab[*r][1],
												 img->ctab[*r][2],
												 &y, &u, &v);
						r++;
					} else if (img->ctab == IW_BGR) {
						prev_inline_rgbToYuvCal (*(r+2), *(r+1), *r, &y, &u, &v);
						r += 3;
					} else if (c_trans) {
						c_trans (*r, *(r+1), *(r+2), &y, &u, &v);
						r += 3;
					} else {	/* if (img->ctab == IW_RGB) */
						prev_inline_rgbToYuvCal (*r, *(r+1), *(r+2), &y, &u, &v);
						r += 3;
					}
				} else {
					if (img->ctab == IW_YUV) {
						y = *r++;
						u = *g++;
						v = *b++;
					} else if (img->ctab > IW_COLFORMAT_MAX) {
						prev_inline_rgbToYuvCal (img->ctab[*r][0],
												 img->ctab[*r][1],
												 img->ctab[*r][2],
												 &y, &u, &v);
						r++;
					} else if (img->ctab == IW_BGR) {
						prev_inline_rgbToYuvCal (*b++, *g++, *r++, &y, &u, &v);
					} else if (c_trans) {
						c_trans (*r++, *g++, *b++, &y, &u, &v);
					} else {	/* if (img->ctab == IW_RGB) */
						prev_inline_rgbToYuvCal (*r++, *g++, *b++, &y, &u, &v);
					}
				}
				*yd++ = y;
				if (format == IW_IMG_FORMAT_AVI_RAW444) {
					/* -> YVU444P */
					*ud++ = v;
					*vd++ = u;
				} else if (format == IW_IMG_FORMAT_AVI_FFV1) {
					/* -> YUV444P */
					*ud++ = u;
					*vd++ = v;
				} else if (xs%2 == 0 && ys%2 == 0) {
					/* -> YUV420P */
					*ud++ = u;
					*vd++ = v;
				}
			} /* for xs */
			if (img->rowstride)
				r += img->rowstride - w*img->planes;
			else {
				r += img->width - w;
				g += img->width - w;
				b += img->width - w;
			}
			if (w < width)
				memset (yd, 0, width - w);
			yd += width - w;
			if (format == IW_IMG_FORMAT_AVI_RAW444 ||
				format == IW_IMG_FORMAT_AVI_FFV1 || ys%2 == 0) {
				if (w < width) {
					memset (ud, 128, colskip);
					memset (vd, 128, colskip);
				}
				ud += colskip;
				vd += colskip;
			}
		} /* for ys */
		if (h < height) {
			memset (yd, 0, (height-h)*width);
			colsize = dst[1] + colsize - ud;
			memset (ud, 128, colsize);
			memset (vd, 128, colsize);
		}
	}
}

#ifdef WITH_FFMPEG

/*********************************************************************
  Seek to a frame before frame number fnum.
*********************************************************************/
static iwImgStatus ffm_seek (iwMovie **movie, const char *fname, int fnum,
							 gboolean *reopened)
{
	iwMovieIn *m = &(*movie)->io.i;
	AVStream *stream = m->formatCtx->streams[m->videoStream];
	int cur_fnum;
	int64_t timestamp;
	enum CodecID id;
	int flags = AVSEEK_FLAG_BACKWARD;
	iwImgStatus status = IW_IMG_STATUS_OK;

	cur_fnum = fnum - 1;

	/* To always get an I frame */
	id = stream->codec->codec_id;
	if (id == CODEC_ID_MPEG1VIDEO || id == CODEC_ID_MPEG2VIDEO)
		cur_fnum -= 16;

	/* Seek to the frame before the desired one and
	   decode to the correct one */
	if (cur_fnum > 0) {
		timestamp =
			av_rescale_rnd (cur_fnum,
							(int64_t)m->r_frate.den * stream->time_base.den,
							(int64_t)m->r_frate.num * stream->time_base.num,
							AV_ROUND_DOWN);
		if (stream->start_time != AV_NOPTS_VALUE)
			timestamp += stream->start_time;
	} else {
		timestamp = 0;
		/* Accept non key frames at the start of the video.
		   Otherwise the seek may fail. */
		flags |= AVSEEK_FLAG_ANY;
	}

	if (av_seek_frame (m->formatCtx, m->videoStream, timestamp, flags) < 0) {
		if (!(flags & AVSEEK_FLAG_ANY)) {
			flags |= AVSEEK_FLAG_ANY;
			if (av_seek_frame (m->formatCtx, m->videoStream, timestamp, flags) < 0)
				status = IW_IMG_STATUS_ERR;
			else
				iw_warning ("Keyframe seek failed, used non keyframe seek.\n"
							"\tAttention: Image may be wrong");
		} else
			status = IW_IMG_STATUS_ERR;
	}

	/* Reopen the file if seek failed and frame is at start of video */
	if (status != IW_IMG_STATUS_OK && fnum < 10) {
		*reopened = TRUE;
		iw_movie_close (*movie);
		*movie = iw_movie_open (fname, &status);
		if (*movie)
			iw_warning ("Seek failed, reopened file %s", fname);
		if (!*movie || fnum < 2)
			return status;
		m = &(*movie)->io.i;
		stream = m->formatCtx->streams[m->videoStream];
	}
	avcodec_flush_buffers (stream->codec);

	return status;
}

static void ffm_swap_images (iwMovieIn *m)
{
	iwImage *img = m->image;

	m->fnum = m->fnum2;
	m->fnum2 = -99;

	m->image = m->image2;
	m->image2 = img;

	m->frameYUV->data[0] = img->data[0];
	m->frameYUV->data[1] = img->data[1];
	m->frameYUV->data[2] = img->data[2];
}

static gboolean ffm_convert (iwMovieIn *m, AVPicture *src, iwImage *dst)
{
	AVCodecContext *codecCtx = m->formatCtx->streams[m->videoStream]->codec;
	int w = codecCtx->width, h = codecCtx->height;
	int pix_fmt;

	m->frameYUV->data[0] = dst->data[0];
	if (dst->planes == 3) {
		m->frameYUV->data[1] = dst->data[1];
		m->frameYUV->data[2] = dst->data[2];
		pix_fmt = PIX_FMT_YUV444P;
	} else if (dst->type == IW_16U) {
		pix_fmt = PIX_FMT_GRAY16;
	} else {
		pix_fmt = PIX_FMT_GRAY8;
	}

	m->convert_ctx = sws_getCachedContext(m->convert_ctx,
										  w, h, codecCtx->pix_fmt,
										  w, h, pix_fmt,
										  SWS_BILINEAR, NULL, NULL, NULL);
	if (!m->convert_ctx) {
		iw_warning ("Unable to init conversion context");
		return FALSE;
	}

	sws_scale (m->convert_ctx, (const uint8_t* const*)src->data, src->linesize, 0, h,
			   m->frameYUV->data, m->frameYUV->linesize);
	return TRUE;
}

/*********************************************************************
  Read and decode frame 'frame'.
*********************************************************************/
static iwImgStatus ffm_read_frame (iwMovie **movie, const char *fname, int frame)
{
	iwMovieIn *m = &(*movie)->io.i;
	AVStream *stream = m->formatCtx->streams[m->videoStream];
	AVCodecContext *codecCtx = stream->codec;
	int	frameFinished;
	int seek_fnum = -99;
	AVPacket packet;
	AVPicture frame_last;
	int fnum_last;
	gboolean next = FALSE, prev = FALSE;
	gboolean reopened = FALSE;

	if (frame == IW_MOVIE_NEXT_FRAME) {
		frame = (m->fnum_ret+1+m->fcount) % m->fcount;
		next = TRUE;
	} else if (frame == IW_MOVIE_PREV_FRAME) {
		frame = (m->fnum_ret-1+m->fcount) % m->fcount;
		prev = TRUE;
	}
	m->fnum_ret = frame;

	if (frame == m->fnum || (!next && frame > m->fnum && frame < m->fnum2)) {
		/* Return buffered image */
		return IW_IMG_STATUS_OK;
	} else if (frame == m->fnum2 || frame == m->fnum2+1 ||
			   (next && frame > m->fnum && frame < m->fnum2)) {
		ffm_swap_images (m);
		if (frame == m->fnum || (next && frame < m->fnum)) {
			/* Return buffered image */
			m->fnum_ret = m->fnum;
			return IW_IMG_STATUS_OK;
		}
	} else if (frame != m->fnum+1) {
		iwImgStatus status = ffm_seek (movie, fname, frame, &reopened);
		if (status != IW_IMG_STATUS_OK) {
			iw_warning ("Unable to seek to frame %d", frame);
			return status;
		}
		/* Seek may have reopened the movie */
		m = &(*movie)->io.i;
		stream = m->formatCtx->streams[m->videoStream];
		codecCtx = stream->codec;
		seek_fnum = -1;
	}

	fnum_last = -1;
	frame_last.data[0] = NULL;
	while (av_read_frame (m->formatCtx, &packet) >= 0) {
		/* Is this a packet from the video stream? */
		if (packet.stream_index == m->videoStream) {
			int fnum;
			int64_t dts = packet.dts;
			if (stream->start_time != AV_NOPTS_VALUE)
				dts -= stream->start_time;
			fnum = av_rescale_rnd (dts,
								   (int64_t)stream->time_base.num * m->r_frate.num,
								   (int64_t)stream->time_base.den * m->r_frate.den,
								   AV_ROUND_NEAR_INF)
				- m->fnum_start;

			/* If seeked to far try to reopen the video */
			if (seek_fnum > -99 && fnum > frame && frame == 0 && !reopened) {
				iwImgStatus status;
				iw_movie_close (*movie);
				*movie = iw_movie_open (fname, &status);
				if (status != IW_IMG_STATUS_OK) {
					iw_warning ("Unable to seek to frame %d", frame);
					return status;
				} else
					iw_warning ("Seek failed, reopened file %s", fname);
				reopened = TRUE;

				m = &(*movie)->io.i;
				stream = m->formatCtx->streams[m->videoStream];
				codecCtx = stream->codec;
				continue;
			}
#ifdef IW_DEBUG
			if (seek_fnum == -1)
				seek_fnum = fnum;
#endif
			if (fnum < frame-2)
				codecCtx->skip_frame = 1; /* No decoding of b frames */
			else
				codecCtx->skip_frame = 0;

#if LIBAVCODEC_VERSION_INT < ((52<<16)+(26<<8))
			avcodec_decode_video (codecCtx, m->frame, &frameFinished,
								  packet.data, packet.size);
#else
			avcodec_decode_video2 (codecCtx, m->frame, &frameFinished, &packet);
#endif
			/* Did we get a complete video frame and is it "near" the final one?
			   HACK: Gaps of a size of more than 50 frames are not supported. */
			if (frameFinished && fnum+50 > frame) {
				if (fnum < frame) {
					/* Remember the frame before the final one */
					if (!frame_last.data[0])
						avpicture_alloc (&frame_last, codecCtx->pix_fmt,
										 codecCtx->width, codecCtx->height);
					av_picture_copy (&frame_last, (AVPicture*)m->frame, codecCtx->pix_fmt,
									 codecCtx->width, codecCtx->height);
					fnum_last = fnum;
				} else {
					gboolean ok;
					/* Convert the image from its native format to YUV/Gray */
					if (fnum == frame || m->fnum < 0 || next) {
						ok = ffm_convert (m, (AVPicture*)m->frame, m->image);
						m->fnum = fnum;
						m->fnum2 = -99;
					} else {
						if (frame_last.data[0]) {
							ffm_convert (m, &frame_last, m->image);
							m->fnum = fnum_last;
						}
						ok = ffm_convert (m, (AVPicture*)m->frame, m->image2);
						m->fnum2 = fnum;
					}

					/*
					if (m->frame->repeat_pict)
						fprintf (stderr, "Repeat %d: %d\n", m->fnum, m->frame->repeat_pict);
					*/
					iw_debug (5, "Frame %d seek %d dst %d last %d: packet: pts %ld"
							  " dur %ld dts %ld  stream: dts %ld",
							  m->fnum, seek_fnum, frame, fnum, (long)packet.pts,
							  (long)packet.duration, (long)packet.dts, (long)stream->cur_dts);

					avpicture_free (&frame_last);
					av_free_packet (&packet);
					if (next || prev)
						m->fnum_ret = m->fnum;
					else
						m->fnum_ret = frame;
					if (ok)
						return IW_IMG_STATUS_OK;
					else
						return IW_IMG_STATUS_ERR;
				}
			}
		}

		/* Free the packet that was allocated by av_read_frame */
		av_free_packet (&packet);
	}
	avpicture_free (&frame_last);
	codecCtx->skip_frame = 0;

	return IW_IMG_STATUS_ERR;
}

/*********************************************************************
  If ffm_open() failed this function is called to free resources and
  return an error message.
*********************************************************************/
static iwMovie* ffm_open_error (iwMovie *m, char *errstr, int err, iwImgStatus *status)
{
	if (status) {
		if (err == IW_IMG_STATUS_FORMAT)
			*status = IW_IMG_STATUS_FORMAT;
		else if (err == AVERROR(ENOMEM))
			*status = IW_IMG_STATUS_MEM;
		else if (err == AVERROR(EIO))
			*status = IW_IMG_STATUS_READ;
		else
			*status = IW_IMG_STATUS_ERR;
	}
	iw_debug (5, "fmm_open() error %d with %s.", err, errstr);

	iw_movie_close (m);
	return NULL;
}

static void ffm_init_lib (void)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		av_register_all();
#ifdef IW_DEBUG
		if (iw_debug_get_level() <= 5)
#endif
			av_log_set_level (AV_LOG_ERROR);
		initialized = TRUE;
	}
}

/*********************************************************************
  Try to open the video file fname.
*********************************************************************/
static iwMovie *ffm_open (const char *fname, iwImgStatus *status)
{
	iwMovie *movie = NULL;
	iwMovieIn *m = NULL;
	AVFormatContext *formatCtx = NULL;
	AVCodecContext *codecCtx = NULL;
	AVStream *stream;
	AVCodec	*codec;
	iwType type;
	int i, err, planes;

	ffm_init_lib();

	/* Open input file */
#if LIBAVFORMAT_VERSION_INT < ((53<<16)+(4<<8))
	if ((err = av_open_input_file (&formatCtx, fname, NULL, 0, NULL)) != 0)
#else
	if ((err = avformat_open_input (&formatCtx, fname, NULL, NULL)) != 0)
#endif
		return ffm_open_error (movie, "av_open_input_file()",
							   err == AVERROR_NOFMT ? IW_IMG_STATUS_FORMAT:err, status);

	movie = calloc (1, sizeof(iwMovie));
	movie->in = TRUE;
	m = &movie->io.i;
	m->formatCtx = formatCtx;
	m->videoStream = -1;

	/* Retrieve stream information */
	if ((err = avformat_find_stream_info (formatCtx, NULL)) < 0)
		return ffm_open_error (movie, "avformat_find_stream_info()", err, status);

	/* Find the first video stream
	   and get a pointer to the codec context for the video stream */
	for (i=0; i<formatCtx->nb_streams; i++) {
		codecCtx = formatCtx->streams[i]->codec;
		if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
			m->videoStream = i;
			break;
		}
	}
	if (m->videoStream == -1)
		return ffm_open_error (movie, "find videoStream", 0, status);

	stream = formatCtx->streams[i];

	/* Find the decoder for the video stream */
	if (!(codec = avcodec_find_decoder (codecCtx->codec_id))) {
		/* Do not try to close the codec */
		m->videoStream = -1;
		return ffm_open_error (movie, "avcodec_find_decoder()", 0, status);
	}
	if ((err = avcodec_open2 (codecCtx, codec, NULL)) < 0)
		return ffm_open_error (movie, "avcodec_open2()", err, status);

	/* Allocate video frames */
	type = IW_8U;
	if (codecCtx->pix_fmt == PIX_FMT_GRAY8 ||
		codecCtx->pix_fmt == PIX_FMT_MONOWHITE ||
		codecCtx->pix_fmt == PIX_FMT_MONOBLACK ||
		codecCtx->pix_fmt == PIX_FMT_GRAY16BE ||
		codecCtx->pix_fmt == PIX_FMT_GRAY16LE) {
		planes = 1;
		if (codecCtx->pix_fmt == PIX_FMT_GRAY16BE ||
			codecCtx->pix_fmt == PIX_FMT_GRAY16LE)
			type = IW_16U;
	} else
		planes = 3;
	m->frame = avcodec_alloc_frame();
	m->frameYUV = avcodec_alloc_frame();
	m->image = iw_img_new_alloc (codecCtx->width, codecCtx->height, planes, type);
	m->image2 = iw_img_new_alloc (codecCtx->width, codecCtx->height, planes, type);
	if (!m->image || !m->image2 || !m->frame || !m->frameYUV)
		return ffm_open_error (movie, "avcodec_alloc_frame()", AVERROR(ENOMEM), status);

	m->frameYUV->data[0] = m->image2->data[0];
	m->frameYUV->linesize[0] = m->image2->width * IW_TYPE_SIZE(m->image2);
	if (planes == 1) {
		m->image->ctab = IW_GRAY;
		m->image2->ctab = IW_GRAY;
	} else {
		m->frameYUV->data[1] = m->image2->data[1];
		m->frameYUV->data[2] = m->image2->data[2];
		m->frameYUV->linesize[1] = m->frameYUV->linesize[0];
		m->frameYUV->linesize[2] = m->frameYUV->linesize[0];
		m->image->ctab = IW_YUV;
		m->image2->ctab = IW_YUV;
	}

	/* At least one DIVX movie had a wrong frame rate of (1,1) */
	if ((stream->r_frame_rate.num == 1 && stream->r_frame_rate.den == 1) ||
		(stream->r_frame_rate.den == 0)) {
		m->r_frate.num = stream->time_base.den;
		m->r_frate.den = stream->time_base.num;
	} else
		m->r_frate = stream->r_frame_rate;
	if (m->r_frate.den == 0)
		return ffm_open_error (movie, "invalid frame rate", AVERROR_INVALIDDATA, status);
	m->framerate = (double)m->r_frate.num / m->r_frate.den;

	/* Some decoder need the first packets -> read them */
	m->fnum_start = 0;
	m->fnum = -1;
	m->fnum2 = -99;
	ffm_read_frame (&movie, fname, 0);
	m->fnum_start = m->fnum;
	m->fnum_ret = -1;
	m->fnum = 0;

	if (formatCtx->duration != AV_NOPTS_VALUE) {
		m->fcount = av_rescale_rnd (formatCtx->duration,
									m->r_frate.num,
									(int64_t)m->r_frate.den * AV_TIME_BASE,
									AV_ROUND_NEAR_INF);
	} else
		m->fcount = -1;

#ifdef IW_DEBUG
	if (iw_debug_get_level() > 5) {
		char buf[100];

		avcodec_string (buf, sizeof(buf), stream->codec, TRUE);
		iw_debug (5, buf);

		/* Dump information about file to stdout */
		if (formatCtx->duration != AV_NOPTS_VALUE) {
			int secs = formatCtx->duration / AV_TIME_BASE;
			int us = formatCtx->duration % AV_TIME_BASE;
			iw_debug (5, "Duration: %ds %dus -> %d frames, FPS: %f",
					  secs, us, m->fcount, m->framerate);
		} else
			iw_debug (4, "Duration: not available, FPS: %f", m->framerate);

		iw_debug (5,"startTime: format %ld stream %ld timeBase: %d/%d frameRate: %d/%d",
				  (long)formatCtx->start_time,
				  (long)stream->start_time,
				  stream->time_base.num, stream->time_base.den,
				  stream->r_frame_rate.num, stream->r_frame_rate.den);
	}
#endif

	return movie;
}

/*********************************************************************
  If ffm_write_frame() failed this function is called to free
  resources and return an error message.
*********************************************************************/
static iwImgStatus ffm_write_error (iwMovie *m, char *errstr, int err)
{
	iwImgStatus status;
	if (err == AVERROR(ENOMEM))
		status = IW_IMG_STATUS_MEM;
	else if (err == AVERROR(EIO))
		status = IW_IMG_STATUS_WRITE;
	else
		status = IW_IMG_STATUS_ERR;
	iw_debug (5, "fmm_write_frame() error %d with %s.", err, errstr);

	iw_movie_close (m);
	return status;
}

/*********************************************************************
  If '*data->movie' is not open, open it as 'fname' with parameters
  from data. Write img as a new frame to the movie '*data->movie'.
*********************************************************************/
static iwImgStatus ffm_write_frame (const iwImgFileData *data, iwImgFormat format,
									const iwImage *img, const char *fname)
{
	AVFormatContext *formatCtx;
	int frate, fbase;
	int out_size;
	int err;
	AVCodecContext *c;
	AVCodec *avcodec;
	iwMovieOut *m;
	uint8_t *frame_buf;

	if (!*data->movie) {
		ffm_init_lib();

		*data->movie = calloc (1, sizeof(iwMovie));
		(*data->movie)->in = FALSE;
		m = &(*data->movie)->io.o;
		m->format = format;

		formatCtx = m->formatCtx = avformat_alloc_context();
		if (!formatCtx)
			return ffm_write_error (*data->movie, "avformat_alloc_context()",
									AVERROR(ENOMEM));

		formatCtx->oformat = av_guess_format ("avi", NULL, NULL);
		gui_strlcpy (formatCtx->filename, fname, sizeof(formatCtx->filename));

		m->stream = avformat_new_stream (formatCtx, NULL);
		if (!m->stream)
			return ffm_write_error (*data->movie, "avformat_new_stream()", 0);

		c = m->stream->codec;
		if (format == IW_IMG_FORMAT_AVI_FFV1) {
			c->codec_id = CODEC_ID_FFV1;
			c->pix_fmt = PIX_FMT_YUV444P;
		} else {
			c->codec_id = CODEC_ID_MPEG4;
			c->pix_fmt = PIX_FMT_YUV420P;
		}
		c->codec_type = AVMEDIA_TYPE_VIDEO;
		c->width = img->width;
		c->height = img->height;
		c->coder_type = 0;				/* FFV1: Golomb-Rice */
		c->context_model = 0;			/* FFV1: small size */
		c->gop_size = 50;

		frate = gui_lrint (data->framerate);
		fbase = 1;
		while (fabs((double)frate/fbase - data->framerate) > 0.001) {
			fbase *= 10;
			frate = gui_lrint (data->framerate * fbase);
		}
		c->time_base.den = frate;
		c->time_base.num = fbase;

		/* Bit rate from 0.05 bpp to 0.5 bpp */
		c->bit_rate = (0.0045*CLAMP(data->quality,0,100) + 0.05) *
			data->framerate*c->width*c->height;

#if LIBAVFORMAT_VERSION_INT < ((53<<16)+4)
		if ((err = av_set_parameters (formatCtx, NULL)) < 0)
			return ffm_write_error (*data->movie, "av_set_parameters()", err);
#endif
#ifdef IW_DEBUG
		if (iw_debug_get_level() > 5)
			av_dump_format (formatCtx, 0, fname, 1);
#endif
		/* Find the video encoder */
		avcodec = avcodec_find_encoder (c->codec_id);
		if (!avcodec)
			return ffm_write_error (*data->movie, "avcodec_find_encoder()", 0);

		/* Open the codec */
		if ((err = avcodec_open2 (c, avcodec, NULL)) < 0)
			return ffm_write_error (*data->movie, "avcodec_open2()", err);

		m->outbuf_size = MAX(FF_MIN_BUFFER_SIZE, c->width*c->height*3);
		m->outbuf = malloc (m->outbuf_size);
		m->frame = avcodec_alloc_frame();
		frame_buf = malloc (avpicture_get_size (c->pix_fmt, c->width, c->height));
		if (!m->outbuf || !m->frame || !frame_buf) {
			if (frame_buf) free (frame_buf);
			return ffm_write_error (*data->movie, "frame allocation",
									AVERROR(ENOMEM));
		}

		avpicture_fill ((AVPicture *)m->frame, frame_buf,
						c->pix_fmt, c->width, c->height);

		if (!(formatCtx->oformat->flags & AVFMT_NOFILE)) {
			if ((err = avio_open (&formatCtx->pb, fname, AVIO_FLAG_WRITE)) < 0)
				return ffm_write_error (*data->movie, "avio_open()", err);
		}
#if LIBAVFORMAT_VERSION_INT < ((53<<16)+(4<<8))
		av_write_header (formatCtx);
#else
		avformat_write_header (formatCtx, NULL);
#endif
	}

	m = &(*data->movie)->io.o;
	c = m->stream->codec;

	/* Convert to correct color space and encode the frame */
	iw_img_convert (img, m->frame->data, c->width, c->height, m->format);
	out_size = avcodec_encode_video (c, m->outbuf, m->outbuf_size, m->frame);
	if (out_size > 0) {
		AVPacket pkt;
		av_init_packet (&pkt);

		if (c->coded_frame->pts != AV_NOPTS_VALUE)
			pkt.pts = av_rescale_q (c->coded_frame->pts, c->time_base, m->stream->time_base);
		else
			pkt.pts = AV_NOPTS_VALUE;
		if (c->coded_frame->key_frame)
			pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index = m->stream->index;
		pkt.data = m->outbuf;
		pkt.size = out_size;

		/* Write the compressed frame in the media file */
		if ((err = av_write_frame (m->formatCtx, &pkt)) != 0)
			return ffm_write_error (*data->movie, "av_write_frame()", err);
	}

	return IW_IMG_STATUS_OK;
}

#else

static iwImgStatus ffm_write_frame (const iwImgFileData *data, iwImgFormat format,
									const iwImage *img, const char *fname)
{
	iw_warning ("Compiled without FFmpeg-support");
	return IW_IMG_STATUS_FORMAT;
}

#endif /* WITH_FFMPEG */

/*********************************************************************
  Try to open the video file fname.
*********************************************************************/
static iwMovie *avi_open (const char *fname, iwImgStatus *status)
{
	avi_t *AVI;

	AVI = AVI_open_input_file (fname, TRUE);
	if (AVI) {
		iwMovie *movie = calloc (1, sizeof(iwMovie));
		if (movie) {
			iwMovieIn *m = &movie->io.i;
			movie->in = TRUE;
			m->AVI = AVI;
			m->framerate = AVI_frame_rate (m->AVI);
			m->fcount = AVI_video_frames (m->AVI);
			m->fnum_ret = -1;

			m->image = iw_img_new_alloc (AVI_video_width (AVI),
										 AVI_video_height (AVI), 3, IW_8U);
			if (m->image) {
				m->image->ctab = IW_YUV;
				return movie;
			}
		}
		iw_movie_close (movie);
		if (status)
			*status = IW_IMG_STATUS_MEM;
	} else if (status)
		*status = IW_IMG_STATUS_OPEN;

	return NULL;
}

/*********************************************************************
  Return TRUE if 'frame' from 'AVI' is a duplicated frame.
*********************************************************************/
static BOOL avi_dup_frame (avi_t *AVI, int frame)
{
	long len = AVI->video_index[frame].len;
	long pos = AVI->video_index[frame].pos;

	return len == 0 ||
		( frame > 0 &&
		  AVI->video_index[frame-1].pos == pos &&
		  AVI->video_index[frame-1].len == len );
}

/*********************************************************************
  Read and decode frame 'frame'.
*********************************************************************/
static iwImgStatus avi_read_frame (iwMovie *movie, int frame)
{
	iwMovieIn *m = &movie->io.i;
	iwImgStatus status = IW_IMG_STATUS_OK;
	long size;
	unsigned int codec;
	int w, h, fpos;
	guchar *buf = NULL;
	avi_t *AVI;

	AVI = m->AVI;
	w = AVI_video_width (AVI);
	h = AVI_video_height (AVI);
	codec = AVI_video_compressor (AVI);

	if (frame == IW_MOVIE_NEXT_FRAME) {
		fpos = m->fnum_ret+1;
	} else if (frame == IW_MOVIE_PREV_FRAME) {
		fpos = m->fnum_ret-1;
	} else
		fpos = frame;
	if (fpos < 0)
		fpos = m->fcount-1;
	else if (fpos >= m->fcount)
		fpos = 0;

	/* Skip duplicated frames */
	if (frame == IW_MOVIE_NEXT_FRAME) {
		for (; fpos<=m->fcount && avi_dup_frame(AVI, fpos); fpos++) /* empty */;
	} else {
		for (; fpos>=0 && avi_dup_frame(AVI, fpos); fpos--) /* empty */;
	}
	if (frame == IW_MOVIE_NEXT_FRAME || frame == IW_MOVIE_PREV_FRAME)
		m->fnum_ret = fpos;
	else
		m->fnum_ret = frame;

	size = AVI_frame_size (AVI, fpos);
	if (size && w>0 && h>0) {
		buf = gui_get_buffer (size);
		AVI_set_video_position (AVI, fpos);
		if (AVI_read_frame (AVI, buf) != size)
			status = IW_IMG_STATUS_READ;
	} else {
		status = IW_IMG_STATUS_ERR;
	}
	if (status == IW_IMG_STATUS_OK) {
		iwImage *img = m->image;

		if (codec == IW_CODEC_444P) {
			memcpy (img->data[0], buf, w*h);
			memcpy (img->data[2], buf+w*h, w*h);
			memcpy (img->data[1], buf+2*w*h, w*h);
		} else {
			guchar *u, *v, *ud, *vd;
			int x, y, w2;

			w2 = EVEN_PROD(w,h)/4;
			if (codec == IW_CODEC_MJPG) {
#ifdef WITH_JPEG
				if (decode_jpeg_raw (buf, size, LAV_NOT_INTERLACED,
									 0, w, h, img->data[0], img->data[1], img->data[2])
					!= 0)
					status = IW_IMG_STATUS_ERR;
#else
				iw_warning ("Compiled without AVI-MJPG-support");
				status = IW_IMG_STATUS_FORMAT;
#endif
				u = img->data[1] + w2 - 1;
				v = img->data[2] + w2 - 1;
			} else {
				memcpy (img->data[0], buf, w*h);
				u = buf + w*h + w2 - 1;
				v = u + w2;
			}
			if (status == IW_IMG_STATUS_OK) {
				/* Convert from YUV420P to YUV444P */
				ud = img->data[1] + w*h - 1;
				vd = img->data[2] + w*h - 1;
				w2 = ((w+1)&(~1))/2;

				/* Last line */
				if (h & 1) {
					/* Last pixel */
					if (w & 1) {
						*ud-- = *u--;
						*vd-- = *v--;
					}
					for (x=w/2; x>0; x--) {
						*ud-- = *u;
						*ud-- = *u--;
						*vd-- = *v;
						*vd-- = *v--;
					}
				}
				for (y=h/2; y>0; y--) {
					/* Last pixel in line */
					if (w & 1) {
						*ud-- = *u--;
						*vd-- = *v--;
					}
					for (x=w/2; x>0; x--) {
						*ud-- = *u;
						*ud-- = *u--;
						*vd-- = *v;
						*vd-- = *v--;
					}
					u += w2;
					v += w2;
					if (w & 1) {
						*ud-- = *u--;
						*vd-- = *v--;
					}
					for (x=w/2; x>0; x--) {
						*ud-- = *u;
						*ud-- = *u--;
						*vd-- = *v;
						*vd-- = *v--;
					}
				}
			}
		}
	}
	if (buf) gui_release_buffer (buf);

	return status;
}

/*********************************************************************
  Close movie file if it is != NULL and free all associated data.
*********************************************************************/
void iw_movie_close (iwMovie *movie)
{
	if (!movie) return;

	if (movie->in) {
		iwMovieIn *m = &movie->io.i;

		if (m->image)
			iw_img_free (m->image, IW_IMG_FREE_ALL);

		if (m->AVI)
			AVI_close (m->AVI);

#ifdef WITH_FFMPEG
		/* Free the frames */
		av_free (m->frameYUV);
		av_free (m->frame);
		if (m->image2)
			iw_img_free (m->image2, IW_IMG_FREE_ALL);

		/* Close the file */
		if (m->formatCtx) {
			if (m->videoStream >= 0
				&& m->formatCtx->streams[m->videoStream]->codec)
				avcodec_close (m->formatCtx->streams[m->videoStream]->codec);
			avformat_close_input (&m->formatCtx);
		}
#endif
	} else {
		iwMovieOut *m = &movie->io.o;

		if (m->AVI)
			AVI_close (m->AVI);

#ifdef WITH_FFMPEG
		if (m->formatCtx) {
			int i;

			av_write_trailer (m->formatCtx);

			if (m->stream) {
				if (m->stream->codec)
					avcodec_close (m->stream->codec);
			}
			if (m->frame) {
				av_free (m->frame->data[0]);
				av_free (m->frame);
			}
			if (m->outbuf)
				free (m->outbuf);

			for (i = 0; i < m->formatCtx->nb_streams; i++) {
				av_freep (&m->formatCtx->streams[i]->codec);
				av_freep (&m->formatCtx->streams[i]);
			}
			if (!(m->formatCtx->oformat->flags & AVFMT_NOFILE)) {
				/* Close the output file */
#if LIBAVFORMAT_VERSION_INT < (52 << 16)
				avio_close (&m->formatCtx->pb);
#else
				avio_close (m->formatCtx->pb);
#endif
			}
			av_free (m->formatCtx);
		}
#endif
	}
	free (movie);
}

/*********************************************************************
  Open the movie file fname for reading.
*********************************************************************/
iwMovie* iw_movie_open (const char *fname, iwImgStatus *status)
{
	iwMovie *movie = NULL;
	if (status) *status = IW_IMG_STATUS_OK;

#ifdef WITH_FFMPEG
	if ((movie = ffm_open (fname, status)))
		return movie;
#endif

	if ((movie = avi_open (fname, status)))
		return movie;

	return movie;
}

/*********************************************************************
  Return frame rate of 'movie'.
*********************************************************************/
float iw_movie_get_framerate (const iwMovie *movie)
{
	if (movie)
		return movie->io.i.framerate;
	else
		return -1;
}

/*********************************************************************
  Return number of frames of 'movie'.
*********************************************************************/
int iw_movie_get_framecnt (const iwMovie *movie)
{
	if (movie)
		return movie->io.i.fcount;
	else
		return -1;
}

/*********************************************************************
  Return the number of the frame, which was the result of the
  last call to iw_movie_read()
*********************************************************************/
int iw_movie_get_framepos (iwMovie *movie)
{
	if (movie)
		return movie->io.i.fnum_ret;
	else
		return -1;
}

/*********************************************************************
  Open the movie 'fname' (if not already open) and return its frame
  'frame'.
  ATTENTION: The returned frame is a pointer to an internally used
  image which is freed by iw_movie_close().
*********************************************************************/
iwImage* iw_movie_read (iwMovie **movie, const char *fname,
						int frame, iwImgStatus *status)
{
	iwImgStatus _status = IW_IMG_STATUS_OK;

	if (!*movie) {
		*movie = iw_movie_open (fname, status);
		if (!*movie) return NULL;
	}

#ifdef WITH_FFMPEG
	if ((*movie)->io.i.formatCtx)
		_status = ffm_read_frame (movie, fname, frame);
#endif

	if ((*movie)->io.i.AVI)
		_status = avi_read_frame (*movie, frame);

	if (status) *status = _status;

	if (_status == IW_IMG_STATUS_OK)
		return (*movie)->io.i.image;
	else
		return NULL;
}

/*********************************************************************
  If '*data->movie' is not open, open it as 'fname' with parameters
  from data. Write img as a new frame to the movie '*data->movie'.
*********************************************************************/
iwImgStatus iw_movie_write (const iwImgFileData *data, iwImgFormat format,
							const iwImage *img, const char *fname)
{
	int w = 0, h = 0, cnt;
	int colsize = 0;
	guchar *writebuf, *buf = NULL, *crop[3];
	iwImgStatus status = IW_IMG_STATUS_OK;
	iwMovieOut *m;

	if (IW_TYPE_SIZE(img) > 1)
		return IW_IMG_STATUS_ERR;

	if (format == IW_IMG_FORMAT_AVI_FFV1 || format == IW_IMG_FORMAT_AVI_MPEG4)
		return ffm_write_frame (data, format, img, fname);

	if (!*data->movie) {
		unsigned int codec;
		avi_t *AVI = AVI_open_output_file (fname);
		if (!AVI)
			return IW_IMG_STATUS_OPEN;

		*data->movie = calloc (1, sizeof(iwMovie));
		(*data->movie)->in = FALSE;
		(*data->movie)->io.o.AVI = AVI;
		(*data->movie)->io.o.format = format;

		switch (format) {
			case IW_IMG_FORMAT_AVI_RAW444:
				codec = IW_CODEC_444P;
				w = img->width;
				h = img->height;
				break;
			case IW_IMG_FORMAT_AVI_RAW420:
				codec = IW_CODEC_I420;
				w = (img->width+1) & (~1);
				h = (img->height+1) & (~1);
				break;
			case IW_IMG_FORMAT_AVI_MJPEG:
#ifdef WITH_JPEG
				codec = IW_CODEC_MJPG;
				w = (img->width+15) & (~15);
				h = (img->height+15) & (~15);
#else
				iw_warning ("Compiled without AVI-MJPG-support");
				return IW_IMG_STATUS_FORMAT;
#endif
				break;
			default:
				iw_error ("Unknown format %d", format);
				return IW_IMG_STATUS_FORMAT;
		}

		AVI_set_video (AVI, w, h, data->framerate, codec);
		AVI_set_audio (AVI, 0, 0, 0, 0);
	}
	m = &(*data->movie)->io.o;
	w = AVI_video_width (m->AVI);
	h = AVI_video_height (m->AVI);

	/* Check if image can be written directly */
	if (m->format == IW_IMG_FORMAT_AVI_RAW444 &&
		img->width == w && img->height == h &&
		!img->rowstride && img->planes >= 3 &&
		img->ctab == IW_YUV) {

		cnt = w*h;
		if (AVI_write_frame_start (m->AVI, cnt*3) ||
			AVI_write_frame (m->AVI, img->data[0], cnt) ||
			AVI_write_frame (m->AVI, img->data[2], cnt) ||
			AVI_write_frame (m->AVI, img->data[1], cnt))

			status = IW_IMG_STATUS_WRITE;
		else
			AVI_write_frame_end (m->AVI);

		return status;
	}

	switch (m->format) {
		case IW_IMG_FORMAT_AVI_RAW444:
			colsize = w*h;				/* YUV444P */
			buf = gui_get_buffer (w*h + colsize*2);
			crop[0] = buf;
			break;
		case IW_IMG_FORMAT_AVI_RAW420:
			colsize = EVEN_PROD(w,h)/4;	/* YUV420P */
			buf = gui_get_buffer (w*h + colsize*2);
			crop[0] = buf;
			break;
		case IW_IMG_FORMAT_AVI_MJPEG:
			colsize = EVEN_PROD(w,h)/4;	/* YUV420P */
			buf = gui_get_buffer (w*h*3/2 + w*h + colsize*2);
			crop[0] = buf+w*h*3/2;
			break;
		default:
			iw_error ("Unknown format %d", m->format);
			break;
	}
	crop[1] = crop[0]+w*h;
	crop[2] = crop[1]+colsize;
	iw_img_convert (img, crop, w, h, m->format);

	if (m->format == IW_IMG_FORMAT_AVI_MJPEG) {
#ifdef WITH_JPEG
		cnt = encode_jpeg_raw (buf, w*h*3/2, CLAMP(data->quality,0,100),
							   LAV_NOT_INTERLACED, 0,
							   w, h, crop[0], crop[1], crop[2]);
#else
		cnt = 0;
#endif
		writebuf = buf;
	} else {
		cnt = w*h + colsize*2;
		writebuf = crop[0];
	}
	if (cnt > 0) {
		if (AVI_write_frame_start (m->AVI, cnt) ||
			AVI_write_frame (m->AVI, writebuf, cnt))
			status = IW_IMG_STATUS_WRITE;
		else
			AVI_write_frame_end (m->AVI);
	} else {
		status = IW_IMG_STATUS_ERR;
	}
	gui_release_buffer (buf);

	return status;
}
