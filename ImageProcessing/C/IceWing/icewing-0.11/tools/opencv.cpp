/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

#include "config.h"
#include <string.h>

#include "main/plugin_i.h"
#include "opencv.h"

/* From cxtypes.h */
#define IPL_DEPTH_SIGN	0x80000000
#define IPL_DEPTH_8U	8
#define IPL_DEPTH_16U	16
#define IPL_DEPTH_32S	(IPL_DEPTH_SIGN|32)
#define IPL_DEPTH_32F	32
#define IPL_DEPTH_64F	64

/* OpenCV 1 data type of an image */
typedef struct _IplImage {
	int	 nSize;			/* sizeof(IplImage) */
	int	 ID;			/* version (=0) */
	int	 nChannels;		/* Most of OpenCV functions support 1,2,3 or 4 channels */
	int	 alphaChannel;	/* ignored by OpenCV */
	int	 depth;			/* pixel depth in bits: IPL_DEPTH_8U, IPL_DEPTH_8S, IPL_DEPTH_16S,
						   IPL_DEPTH_32S, IPL_DEPTH_32F and IPL_DEPTH_64F are supported */
	char colorModel[4]; /* ignored by OpenCV */
	char channelSeq[4]; /* ditto */
	int	 dataOrder;		/* 0 - interleaved color channels, 1 - separate color channels.
						   cvCreateImage can only create interleaved images */
	int	 origin;		/* 0 - top-left origin,
						   1 - bottom-left origin (Windows bitmaps style) */
	int	 align;			/* Alignment of image rows (4 or 8).
						   OpenCV ignores it and uses widthStep instead */
	int	 width;			/* image width in pixels */
	int	 height;		/* image height in pixels */
	struct _IplROI *roi;/* image ROI. if NULL, the whole image is selected */
	struct _IplImage *maskROI; /* must be NULL */
	void  *imageId;		/* ditto */
	struct _IplTileInfo *tileInfo; /* ditto */
	int	 imageSize;		/* image data size in bytes
						   (==image->height*image->widthStep
						   in case of interleaved data) */
	char *imageData;	/* pointer to aligned image data */
	int	 widthStep;		/* size of aligned image row in bytes */
	int	 BorderMode[4]; /* ignored by OpenCV */
	int	 BorderConst[4]; /* ditto */
	char *imageDataOrigin; /* pointer to very origin of image data
							  (not necessarily aligned) -
							  needed for correct deallocation */
} _IplImage;

#define CV_MAT_CHANNEL(mat) ((((mat)->flags >> 3) & 511) + 1)
namespace cv {

/* OpenCV 2 data type of an image */
class Mat {
public:
	int	  flags;
	int	  dims;
	int	  rows, cols;
	uchar *data;
	int	  *refcount;
	uchar *datastart;
	uchar *dataend;
	uchar *datalimit;
	void  *allocator;
	struct MSize {
		int *p;
	};
	struct MStep {
		size_t *p;
		size_t buf[2];
	};
	MSize size;
	MStep step;
};
}

typedef struct {
	int width;
	int height;
} CvSize;

static inline CvSize cvSize (int width, int height)
{
	CvSize s;

	s.width = width;
	s.height = height;

	return s;
}

static BOOL initialized = FALSE;
static IplImage* (*cvCreateImage) (CvSize size, int depth, int channels);
static void (*cvCreateMat) (cv::Mat *mat, int d, const int *sizes, int type) = NULL;

/*********************************************************************
  Get the first pointer to the functions cvCreateImage() and
  cv::Mat::create(int, int const*, int).
*********************************************************************/
static void opencv_init (void)
{
	if (initialized) return;

	cvCreateImage = (IplImage* (*) (CvSize, int, int)) plug_get_symbol ("cvCreateImage");
	if (!cvCreateImage)
		iw_error ("\n"
				  "\tUnable to find the function cvCreateImage() in any loaded plugin !\n"
				  "\tPerhaps no loaded plugin is linked against the OpenCV library ?");

	cvCreateMat = (void (*) (cv::Mat*, int, const int*, int)) plug_get_symbol ("_ZN2cv3Mat6createEiPKii");
	initialized = TRUE;
}

/*********************************************************************
  Convert from OpenCV1 depth to iwImage type.
*********************************************************************/
static iwType depth2type (int depth)
{
	switch (depth) {
		case IPL_DEPTH_8U:
			return IW_8U; break;
		case IPL_DEPTH_16U:
			return IW_16U; break;
		case IPL_DEPTH_32S:
			return IW_32S; break;
		case IPL_DEPTH_32F:
			return IW_FLOAT; break;
		case IPL_DEPTH_64F:
			return IW_DOUBLE; break;
		default:
			iw_error ("Unsupported image depth %d", depth);
	}
	return IW_8U;
}

/*********************************************************************
  Convert from iwImage type to OpenCV1 depth.
*********************************************************************/
static int type2depth (iwType type)
{
	switch (type) {
		case IW_8U:
			return IPL_DEPTH_8U; break;
		case IW_16U:
			return IPL_DEPTH_16U; break;
		case IW_32S:
			return IPL_DEPTH_32S; break;
		case IW_FLOAT:
			return IPL_DEPTH_32F; break;
		case IW_DOUBLE:
			return IPL_DEPTH_64F; break;
		default:
			iw_error ("Unsupported image type %d", type);
	}
	return IPL_DEPTH_8U;
}

/*********************************************************************
  Convert from OpenCV2 depth to iwImage type.
*********************************************************************/
static iwType cvdepth2type (int cvDepth)
{
	static iwType depths[] = {IW_8U, (iwType)-1, IW_16U, (iwType)-1, IW_32S, IW_FLOAT, IW_DOUBLE, (iwType)-1};
	cvDepth &= 7;
	if (depths[cvDepth] == (iwType)-1)
		iw_error ("Unsupported image depth %d", cvDepth);
	return depths[cvDepth];
}

/*********************************************************************
  Convert from iwImage type to OpenCV2 depth.
*********************************************************************/
static int type2cvdepth (iwType type)
{
	switch (type) {
		case IW_8U:
			return 0; break;
		case IW_16U:
			return 2; break;
		case IW_32S:
			return 4; break;
		case IW_FLOAT:
			return 5; break;
		case IW_DOUBLE:
			return 6; break;
		default:
			iw_error ("Unsupported image type %d", type);
	}
	return 0;
}

/*********************************************************************
  Render the IplImage img in b with the color transformation ctab.
*********************************************************************/
void iw_opencv_render (prevBuffer *b, const IplImage *img, iwColtab ctab)
{
	prevDataImage previmg;
	iwImage i;
	uchar *planeptr[1];

	opencv_init();

	iw_img_init (&i);
	i.type = depth2type (img->depth);
	i.planes = img->nChannels;
	i.width = img->width;
	i.height = img->height;
	i.ctab = ctab;
	i.data = planeptr;
	i.data[0] = (uchar*)img->imageData;
	i.rowstride = img->widthStep;
	previmg.i = &i;
	previmg.x = 0;
	previmg.y = 0;

	prev_render_imgs (b, &previmg, 1, RENDER_CLEAR, i.width, i.height);
}

/*********************************************************************
  Render the OpenCV2 image img in b with the color transformation ctab.
*********************************************************************/
void iw_opencv_render (prevBuffer *b, const cv::Mat *img, iwColtab ctab)
{
	prevDataImage previmg;
	iwImage i;
	uchar *planeptr[1];

	iw_assert (img->dims == 2, "Unsupported dimension %d", img->dims);
	opencv_init();

	iw_img_init (&i);
	i.type = cvdepth2type (img->flags);
	i.planes = CV_MAT_CHANNEL(img);
	i.width = img->cols;
	i.height = img->rows;
	i.ctab = ctab;
	i.data = planeptr;
	i.data[0] = img->data;
	i.rowstride = img->step.p[0];
	previmg.i = &i;
	previmg.x = 0;
	previmg.y = 0;

	prev_render_imgs (b, &previmg, 1, RENDER_CLEAR, i.width, i.height);
}

static void from_iwImage (const iwImage *img, BOOL swapRB, uchar *d, int widthStep)
{
	int w = img->width, h = img->height;
	int bytes = IW_TYPE_SIZE(img);
	int pixbytes = bytes * img->planes;
	int x, y, p, rowstride, linestep;
	uchar *s;

	rowstride = img->rowstride;
	if (!rowstride && img->planes == 1)
		rowstride = img->width*bytes;

	swapRB = swapRB && img->planes > 2;
	linestep = widthStep - w*pixbytes;

	if (rowstride) {
		s = img->data[0];
		if (swapRB) {
			rowstride -= w*pixbytes;

			for (y = 0; y < h; y++) {
				memcpy (d, s, w*pixbytes);
				if (img->type == IW_8U) {
					for (x = 0; x < w; x++) {
						d[0] = s[2];
						d[2] = s[0];
						d += pixbytes;
						s += pixbytes;
					}
				} else {
					for (x = 0; x < w; x++) {
						memcpy (d, &s[bytes*2], bytes);
						memcpy (&d[bytes*2], s, bytes);
						d += pixbytes;
						s += pixbytes;
					}
				}
				d += linestep;
				s += rowstride;
			}
		} else {
			w *= pixbytes;
			for (y = 0; y < h; y++) {
				memcpy (d, s, w);
				d += widthStep;
				s += rowstride;
			}
		}
	} else {
		uchar **data = img->data;
		int off = 0;
		int planes = img->planes;

		if (swapRB) {
			uchar *r;
			data = (uchar**)malloc (sizeof(uchar*)*img->planes);
			memcpy (data, img->data, sizeof(uchar*)*img->planes);
			r = data[0];
			data[0] = data[2];
			data[2] = r;
		}
		if (img->type == IW_8U) {
			for (y = 0; y < h; y++) {
				for (x = 0; x < w; x++) {
					for (p = 0; p < planes; p++)
						*d++ = data[p][off];
					off++;
				}
				d += linestep;
			}
		} else {
			guint16 *d16 = (guint16*)d;
			int b;
			bytes /= 2;
			linestep /= 2;

			for (y = 0; y < h; y++) {
				for (x = 0; x < w; x++) {
					for (p = 0; p < planes; p++) {
						for (b = 0; b < bytes; b++)
							*d16++ = ((guint16**)data)[p][off+b];
					}
					off += bytes;
				}
				d16 += linestep;
			}
		}
		if (swapRB)
			free (data);
	}
}

/*********************************************************************
  Convert img to an IplImage and return it. Returned image is
  allocated with cvCreateImage() and must be freed with
  cvReleaseImage().
  swapRB == TRUE: Swap planes 0 and 2 during conversion.
*********************************************************************/
IplImage* iw_opencv_from_img (const iwImage *img, BOOL swapRB)
{
	IplImage *i;

	opencv_init();
	i = cvCreateImage (cvSize(img->width, img->height), type2depth (img->type), img->planes);
	from_iwImage (img, swapRB, (uchar*)i->imageData, i->widthStep);

	return i;
}

/*********************************************************************
  Convert img to a cv::Mat and return it. Returned image is
  allocated and must be freed with the cv::Mat destructor.
  swapRB == TRUE: Swap planes 0 and 2 during conversion.
*********************************************************************/
cv::Mat* iw_opencvMat_from_img (const iwImage *img, BOOL swapRB)
{
	cv::Mat *i;

	opencv_init();
	if (!cvCreateMat)
		iw_error ("\n"
				  "\tUnable to find the function cv::Mat::create() in any loaded plugin !\n"
				  "\tPerhaps no loaded plugin is linked against the OpenCV 2 library ?");

	i = new cv::Mat();
	i->step.p = i->step.buf;
	i->size.p = &i->rows;

	int sz[] = {img->height, img->width};
	cvCreateMat (i, 2, sz, type2cvdepth(img->type) + ((img->planes-1)<<3));

	from_iwImage (img, swapRB, i->data, i->step.p[0]);

	return i;
}

/*********************************************************************
  Copy image data from s to dest.
  widthStep: Size of aligned image row from s in bytes.
  swapRB == TRUE: Swap planes 0 and 2 during conversion.
*********************************************************************/
static iwImage* to_iwImg (iwImage *dest, uchar *s, int widthStep, BOOL swapRB)
{
	int x, y, p, bytes, w, h;

	if (!dest) return NULL;
	opencv_init();

	w = dest->width;
	h = dest->height;
	bytes = IW_TYPE_SIZE(dest);
	swapRB = swapRB && dest->planes > 2;

	if (dest->planes == 1) {
		uchar *d = dest->data[0];
		w *= bytes;
		for (y = 0; y < h; y++) {
			memcpy (d, s, w);
			s += widthStep;
			d += w;
		}
	} else {
		uchar **data = dest->data;
		int linestep = widthStep - w*bytes*dest->planes;
		int off = 0;

		if (swapRB) {
			uchar *r;
			data = (uchar**)malloc (sizeof(uchar*)*dest->planes);
			memcpy (data, dest->data, sizeof(uchar*)*dest->planes);
			r = data[0];
			data[0] = data[2];
			data[2] = r;
		}
		if (dest->type == IW_8U) {
			for (y = 0; y < h; y++) {
				for (x = 0; x < w; x++) {
					for (p = 0; p < dest->planes; p++)
						data[p][off] = *s++;
					off++;
				}
				s += linestep;
			}
		} else {
			guint16 *s16 = (guint16*)s;
			int b;
			bytes /= 2;
			linestep /= 2;

			for (y = 0; y < h; y++) {
				for (x = 0; x < w; x++) {
					for (p = 0; p < dest->planes; p++) {
						for (b = 0; b < bytes; b++)
							((guint16**)data)[p][off+b] = *s16++;
					}
					off += bytes;
				}
				s16 += linestep;
			}
		}
		if (swapRB)
			free (data);
	}
	return dest;
}

/*********************************************************************
  Convert img to an iwImage and return it. Returned image is
  allocated with iw_img_new_alloc() and must be freed with
  iw_img_free (image, IW_IMG_FREE_ALL).
  swapRB == TRUE: Swap planes 0 and 2 during conversion.
*********************************************************************/
iwImage* iw_opencv_to_img (const IplImage *img, BOOL swapRB)
{
	iwImage *i = iw_img_new_alloc (img->width, img->height, img->nChannels,
								   depth2type(img->depth));
	return to_iwImg (i, (uchar*)img->imageData, img->widthStep, swapRB);
}
iwImage* iw_opencv_to_img (const cv::Mat *img, BOOL swapRB)
{
	iwImage *i = iw_img_new_alloc (img->cols, img->rows, CV_MAT_CHANNEL(img),
								   cvdepth2type(img->flags));
	return to_iwImg (i, img->data, img->step.p[0], swapRB);
}

/*********************************************************************
  Allocate memory for image dest and copy data from s to it.
  swapRB == TRUE: Swap planes 0 and 2 during conversion.
*********************************************************************/
static iwImage* to_iwImg_interleaved (iwImage *dest, uchar *s, BOOL swapRB)
{
	int w = dest->width, h = dest->height;
	uchar *d;

	opencv_init();

	/* Allocate destination image */
	if (!iw_img_allocate (dest)) {
		iw_img_free (dest, IW_IMG_FREE_ALL);
		return NULL;
	}

	/* Copy image */
	d = dest->data[0];
	memcpy (d, s, dest->rowstride * h);

	if (swapRB && dest->planes > 2) {
		int x, y;
		int bytes = IW_TYPE_SIZE(dest);
		int pixbytes = bytes * dest->planes;

		for (y = 0; y < h; y++) {
			if (dest->type == IW_8U) {
				for (x = 0; x < w; x++) {
					d[0] = s[2];
					d[2] = s[0];
					d += pixbytes;
					s += pixbytes;
				}
			} else {
				for (x = 0; x < w; x++) {
					memcpy (d, &s[bytes*2], bytes);
					memcpy (&d[bytes*2], s, bytes);
					d += pixbytes;
					s += pixbytes;
				}
			}
			s += dest->rowstride - w*pixbytes;
			d += dest->rowstride - w*pixbytes;
		}
	}
	return dest;
}

/*********************************************************************
  Convert img to an interleaved iwImage and return it.
  Returned image is allocated and must be freed with
  iw_img_free (image, IW_IMG_FREE_ALL).
  swapRB == TRUE: Swap planes 0 and 2 during conversion.
*********************************************************************/
iwImage* iw_opencv_to_img_interleaved (const IplImage *img, BOOL swapRB)
{
	iwImage *i = iw_img_new();
	i->width = img->width;
	i->height = img->height;
	i->planes = img->nChannels;
	i->type = depth2type (img->depth);
	i->rowstride = img->widthStep;
	return to_iwImg_interleaved (i, (uchar*)img->imageData, swapRB);
}
iwImage* iw_opencv_to_img_interleaved (const cv::Mat *img, BOOL swapRB)
{
	iwImage *i = iw_img_new();
	i->width = img->cols;
	i->height = img->rows;
	i->planes = CV_MAT_CHANNEL(img);
	i->type = cvdepth2type (img->flags);
	i->rowstride = img->step.p[0];
	return to_iwImg_interleaved (i, img->data, swapRB);
}
