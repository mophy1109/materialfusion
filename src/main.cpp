/*
 * @Author: USTB.mophy1109
 * @Date: 2018-03-29 16:30:25
 * @Last Modified by: USTB.mophy1109
 * @Last Modified time: 2018-05-30 16:08:20
 */

#include "CFusion.h"
#include "CUtil.h"
#include "Preprocess.h"
#include <algorithm>
#include <boost/timer.hpp> //timer
#include <dirent.h>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/xfeatures2d.hpp>
#include <string>

using namespace std;
using namespace cv;

void mytestReconstruct() {
	// test the laplacian pyramid
	Mat img1 = imread("/media/cwh1001/Workspace/CUDA_projects/material_fusion/data/500X/00001.jpg", IMREAD_GRAYSCALE);
	Mat img2 = imread("/media/cwh1001/Workspace/CUDA_projects/material_fusion/data/500X/00002.jpg", IMREAD_GRAYSCALE);
	Mat_<float> f_img1;
	img1.convertTo(f_img1, CV_32F, 1.0 / 255.0);

	vector<Mat_<float>> lap = LaplacianPyramid(f_img1);
	Mat imgr = reconstruct(lap);
	imshow("origin", img1);
	imshow("result", imgr);
	waitKey(0);
}

void testProcess() {
	char src[255] = "../data/videos/500px.avi";
	char tar[255] = "../data/out90/";

	// ProcessWithoutSampling(tar);

	ProcessWithSampling(src);
}

int main(int argc, char **argv) {
	// for (int i = 0; i < argc; i++) {
	// 	cout << "argument[" << i << "] is: " << argv[i] << endl;
	// }
	if (argc > 1) {
		if (strcmp(argv[1], "-video") == 0) {
			char *src = argv[2];
			ProcessWithSampling(src);
		} else if (strcmp(argv[1], "-image") == 0) {
			char *src = argv[2];
			ProcessWithoutSampling(src);
		} else {
			cout << argv[1] << endl;

			cout << "params error" << endl;
			return 1;
		}
	} else {
		testProcess();
	}

	return 0;
}