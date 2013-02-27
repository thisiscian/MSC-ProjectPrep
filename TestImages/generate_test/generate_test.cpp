//
// generate_test.cpp
// 
// This program reads 64x64 pixel images from a given folder, and in a random
// order, concatenates them into one image.

#include <iostream>
#include <dirent.h>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"

using namespace cv;
using namespace std;

int main(int argc, char* argv[])
{
	int i,j,k;
	int x,y, max_x=0, max_y=0, padding_x, padding_y;
	DIR *in_dir;
	Mat final_image;
	vector<Mat> icon_image, padded_icon;
	vector<string> entries;
	string outpath;

	srand(time(NULL));

	// Test for the correct number of arguments
	if(argc == 3)
	{
		outpath = argv[2];
	}
	else if(argc <= 1)
	{
		cerr << "Error: Incorrect number of arguments used" << endl;
		cerr << "Usage is " << argv[0] << " icon_folder [output_path]" << endl;
		return 1;
	}
	else
	{
		outpath = "test.jpeg";
	}

	// Set in_dir to the specified directory and test it's validity
	// If it is valid, add all regular-file entries to the 'entries' vector
	in_dir = opendir(argv[1]);
	if(in_dir == NULL)
	{
		cerr << "Error: Could not open directory '" << argv[2] << "'" << endl;
		cerr << "Exiting..."<< endl;
		return 1;
	}
	else
	{
		for(dirent* entry = readdir(in_dir); entry!= NULL; entry = readdir(in_dir))
		{
    	if (entry == NULL && entries.empty())
      {
      	cerr << "Error! Could not initialise current entry" << std::endl;
      }
			if(entry->d_type == DT_REG)
			{
				entries.push_back(entry->d_name);
				entries.back().insert(0, "/");
				entries.back().insert(0, argv[1]);
			}
		}
	}
	random_shuffle(entries.begin(), entries.end());
	icon_image.resize(entries.size());
	padded_icon.resize(entries.size());
	
	// Find the best way to distribute the contents of 'in_dir'
	// by making 'x' and 'y' as close to creating a square as possible
	y=sqrt(entries.size());
	while(y>0 && entries.size()%y != 0)
	{
		y--;
	}
	x = entries.size()/y;

	for(i=0; i<entries.size(); i++)
	{
		icon_image[i] = imread(entries[i].c_str(), 1);
		if(max_x < icon_image[i].rows)
		{
			max_x = icon_image[i].rows;
		}
		if(max_y < icon_image[i].cols)
		{
			max_y = icon_image[i].cols;
		}
	}
	final_image.create(max_y*y,max_x*x,CV_8UC3);
	final_image.setTo(255);
	// Generate matrix that will contain all the entries of 'in_dir'
	i=0;
	while(i<icon_image.size())
	{
		padding_x = (max_x-icon_image[i].rows)/2+max_x*(i%x);
		padding_y = (max_y-icon_image[i].cols)/2+max_y*((i-i%x)/x);
		icon_image[i].copyTo(final_image(Rect(padding_x, padding_y, icon_image[i].rows, icon_image[i].cols)));	
		++i;
	}
	cout << "Generating file '" << outpath << "'" << endl;
	imwrite(outpath, final_image);
	return 0;
}
