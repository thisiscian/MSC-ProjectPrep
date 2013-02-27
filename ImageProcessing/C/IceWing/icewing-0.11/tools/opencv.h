/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

#ifndef iw_opencv_H
#define iw_opencv_H

#include "gui/Grender.h"

typedef struct _IplImage IplImage;
namespace cv {
	class Mat;
}

/*********************************************************************
  Render the IplImage/cv::Mat img in b with the color transformation ctab.
*********************************************************************/
void iw_opencv_render (prevBuffer *b, const IplImage *img, iwColtab ctab);
void iw_opencv_render (prevBuffer *b, const cv::Mat *img, iwColtab ctab);

/*********************************************************************
  Convert img to an IplImage and return it. Returned image is
  allocated with cvCreateImage() and must be freed with
  cvReleaseImage().
  swapRB == TRUE: Swap planes 0 and 2 during conversion.
*********************************************************************/
IplImage* iw_opencv_from_img (const iwImage *img, BOOL swapRB);

/*********************************************************************
  Convert img to a cv::Mat and return it. Returned image is
  allocated and must be freed with the cv::Mat destructor.
  swapRB == TRUE: Swap planes 0 and 2 during conversion.
*********************************************************************/
cv::Mat* iw_opencvMat_from_img (const iwImage *img, BOOL swapRB);

/*********************************************************************
  Convert img to an iwImage and return it. Returned image is
  allocated with iw_img_new_alloc() and must be freed with
  iw_img_free (image, IW_IMG_FREE_ALL).
  swapRB == TRUE: Swap planes 0 and 2 during conversion.
*********************************************************************/
iwImage* iw_opencv_to_img (const IplImage *img, BOOL swapRB);
iwImage* iw_opencv_to_img (const cv::Mat *img, BOOL swapRB);

/*********************************************************************
  Convert img to an interleaved iwImage and return it.
  Returned image is allocated and must be freed with
  iw_img_free (image, IW_IMG_FREE_ALL).
  swapRB == TRUE: Swap planes 0 and 2 during conversion.
*********************************************************************/
iwImage* iw_opencv_to_img_interleaved (const IplImage *img, BOOL swapRB);
iwImage* iw_opencv_to_img_interleaved (const cv::Mat *img, BOOL swapRB);

#endif /* iw_opencv_H */
