#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/ml/ml.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/nonfree/nonfree.hpp"

#define testcases 2
using namespace cv;
using namespace std;

bool compare_response(KeyPoint first, KeyPoint second)
{
  if (first.response < second.response) return true;
  else return false;
}

int main( int argc, char* argv[])
{
  Mat tmp_img;
  int minHessian = 400;
  SurfFeatureDetector detector(minHessian);

  string wally_path = "../../MachineLearningSamples/cleanwally";
  string notwally_path = "../../MachineLearningSamples/notwally";
  vector< vector<KeyPoint> > keypoints;
  for(int i=0; i<testcases; ++i)
  {
    stringstream ss;
    ss << wally_path << i+1 << ".jpg";
    cout << ss.str() << endl;
    tmp_img = imread(ss.str().c_str(), 1);
    vector<KeyPoint> tmp_kp;
    detector.detect(tmp_img, tmp_kp);
    sort(tmp_kp.begin(),tmp_kp.end(), compare_response);
    vector<KeyPoint>::iterator it = tmp_kp.begin();
    it+=100;
    tmp_kp.erase(it, tmp_kp.end());
    keypoints.push_back(tmp_kp);
  }
   
  for(int i=0; i<testcases; ++i)
  {
    stringstream ss;
    ss << notwally_path << i+1 << ".jpg";
    cout << ss.str() << endl;
    tmp_img = imread(ss.str().c_str(), 1);
    vector<KeyPoint> tmp_kp;
    detector.detect(tmp_img, tmp_kp);
    sort(tmp_kp.begin(),tmp_kp.end(), compare_response);
    vector<KeyPoint>::iterator it = tmp_kp.begin();
    it+=100;
    tmp_kp.erase(it, tmp_kp.end());
    keypoints.push_back(tmp_kp);
  }

  int max_keypoints=keypoints[0].size();
  for(int i=1; i<keypoints.size(); ++i)
  {
    if(keypoints[i].size() > max_keypoints)
    {
      max_keypoints = keypoints[i].size();
    }
  }

  cout << "max_keypoints = " << max_keypoints << endl;

  Mat trainData = Mat::zeros(keypoints.size(), 2*max_keypoints, CV_32FC1);
  Mat labels(keypoints.size(), 1, CV_32FC1);
  cout << "defined stuff" << endl;
  
  for(int i=0; i<keypoints.size(); ++i)
  {
    for(int j=0; j<keypoints[i].size(); ++j)
    {
      trainData.at<double>(i,2*j) = keypoints[i][j].pt.x;
      trainData.at<double>(i,2*j+1) = keypoints[i][j].pt.y;
//      trainData.at<double>(i,5*j+2) = keypoints[i][j].size;
//      trainData.at<double>(i,5*j+3) = keypoints[i][j].angle;
//      trainData.at<double>(i,5*j+4) = keypoints[i][j].response;
    }
  }
 

  labels.rowRange(0,testcases).setTo(1);
  labels.rowRange(testcases, 2*testcases).setTo(2);
  
  cout << "labels made " << endl;

  CvSVMParams params;
  params.svm_type    = SVM::C_SVC;
  params.C 		   = 0.1;
  params.kernel_type = SVM::LINEAR;
  params.term_crit   = TermCriteria(CV_TERMCRIT_ITER, (int)1e7, 1e-6);

  cout << "params sorted" << endl;
  
  CvSVM svm;
  svm.train(trainData, labels, Mat(), Mat(), params);
  cout << "svm training done " << endl;

  tmp_img = imread("../../MachineLearningSamples/cleanwally5.jpg", 1);
  cout << "read in image" << endl;
  vector<KeyPoint> tmp_kp;
  detector.detect(tmp_img, tmp_kp);
  cout << "detected "<< endl;
  sort(tmp_kp.begin(),tmp_kp.end(), compare_response);
  vector<KeyPoint>::iterator it = tmp_kp.begin();
  it+=100;
  tmp_kp.erase(it, tmp_kp.end());
  cout << "made keypoints late " << endl;

  Mat testData(1, 2*max_keypoints, CV_32FC1);
  cout << tmp_kp.size()  << " and " << max_keypoints << endl;

  for(int j=0; j<tmp_kp.size() && j<max_keypoints; ++j)
  {
    testData.at<double>(0,2*j) = tmp_kp[j].pt.x;
    testData.at<double>(0,2*j+1) = tmp_kp[j].pt.y;
//    testData.at<double>(0,5*j+2) = tmp_kp[j].size;
//    testData.at<double>(0,5*j+3) = tmp_kp[j].angle;
//    testData.at<double>(0,5*j+4) = tmp_kp[j].response;
  }

  cout << "got to here" << endl; 
 
  float response = svm.predict(testData);
  cout << "Response = " << response << endl;
  if(response == 1)
  {
    cout << "This means that it thinks this is a wally" << endl;
  }
  else if(response == 2)
  {
    cout << "This means that it thinks this is NOT a wally" << endl;
  }
   
  return 0;
}

