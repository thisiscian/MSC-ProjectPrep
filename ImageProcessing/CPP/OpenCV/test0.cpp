#include<iostream>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/nonfree/nonfree.hpp"

using namespace std;
using namespace cv;

int main()
{
	Mat grey_wally, dst, abs_dst, wally = imread("UltraWally.jpg");
	int kernel_size = 3;
	int scale = 1;
	int delta = 0;
	int ddepth = CV_16S;
	char* window_name = "Edge Wally";

	namedWindow(window_name, CV_WINDOW_AUTOSIZE);
	imshow(window_name, wally);
	waitKey(0);

	GaussianBlur(wally,wally, Size(3,3), 0, 0, BORDER_DEFAULT);
	
	imshow(window_name, wally);
	waitKey(0);

	cvtColor(wally, grey_wally, CV_RGB2GRAY);
	
	imshow(window_name, grey_wally);
	waitKey(0);
	
	Laplacian(grey_wally, dst, ddepth, kernel_size, scale, delta, BORDER_DEFAULT);
	convertScaleAbs(dst, abs_dst);
	
	imshow(window_name, abs_dst);
	waitKey(0);
	return 0;
}
