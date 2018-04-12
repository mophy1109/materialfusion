/*
 * @Author: USTB.mophy1109
 * @Date: 2018-04-02 11:13:23
 * @Last Modified by: USTB.mophy1109
 * @Last Modified time: 2018-04-08 16:40:58
 */

#include "CStitching.h"
#include "SiftGPU.h"

#include <GL/gl.h>
#include <algorithm>	   //std::sort
#include <boost/timer.hpp> //timer
#include <iostream>
#include <math.h>
#include <opencv2/calib3d.hpp>
#include <vector>

using namespace cv;
using namespace std;

int CStitching::Stitching(Mat img1, Mat img2, Mat &result) {

	boost::timer totaltime;

	//采用CUDA并行方式进行图像配准
	SiftGPU sift_1;
	SiftGPU sift_2;
	char *myargv[4] = { "-cuda", "0", "-v", "0" };
	sift_1.ParseParam(4, myargv);
	sift_2.ParseParam(4, myargv);

	// test if your hardware support SiftGPU
	int support = sift_1.CreateContextGL();
	if (support != SiftGPU::SIFTGPU_FULL_SUPPORTED) {
		cout << "SiftGPU is not supported!" << endl;
		return 1;
	}

	boost::timer timer;
	int width = img1.cols;
	int height = img1.rows;
	//运行sift算法
	sift_1.RunSIFT(width, height, img1.data, GL_LUMINANCE, GL_UNSIGNED_BYTE);
	sift_2.RunSIFT(width, height, img2.data, GL_LUMINANCE, GL_UNSIGNED_BYTE);
	cout << "runsift() cost time=" << timer.elapsed() << endl;

	timer.restart(); // match time
	//获取关键点和描述子
	int num_1 = sift_1.GetFeatureNum();
	int num_2 = sift_2.GetFeatureNum();
	cout << "features Num of pic1:" << num_1 << endl;
	cout << "features Num of pic2:" << num_2 << endl;
	float *descriptors_1 = new float[128 * num_1];
	float *descriptors_2 = new float[128 * num_2];
	vector<SiftGPU::SiftKeypoint> keys_1(num_1);
	vector<SiftGPU::SiftKeypoint> keys_2(num_2);
	sift_1.GetFeatureVector(&keys_1[0], &descriptors_1[0]);
	sift_2.GetFeatureVector(&keys_2[0], &descriptors_2[0]);

	// Matching descriptor vectors using SiftGPU Macher
	SiftMatchGPU matcher(4096);

	// maximum number to match
	matcher.SetLanguage(SiftMatchGPU::SIFTMATCH_LANGUAGE::SIFTMATCH_CUDA); // set cuda to accelerate

	if (matcher.VerifyContextGL() == 0)
		return 1;

	// set descriptors
	matcher.SetDescriptors(0, num_1, descriptors_1);
	matcher.SetDescriptors(1, num_2, descriptors_2);

	// save offsets for further use
	vector<Point> offsets;
	vector<Point> train_point;
	vector<Point> query_point;

	int match_buf[4096][2];
	int nmatch = matcher.GetSiftMatch(4096, match_buf);
	for (int i = 0; i < nmatch; i++) {
		train_point.push_back(Point(keys_1[match_buf[i][0]].x, keys_1[match_buf[i][0]].y));
		query_point.push_back(Point(keys_2[match_buf[i][1]].x, keys_2[match_buf[i][1]].y));
		// cout << "(" << (string)offsets[i].x << ")" << endl;
	}
	cout << "Found " << nmatch << " matches.      Match cost time=" << timer.elapsed() << endl;

	timer.restart();
	Offset off = CalOffset(train_point, query_point, CalOffsetMethod);
	cout << "AFFINE OFFSET:(" << off.x << "," << off.y << ")      CalOffset cost time=" << timer.elapsed() << endl;
	g_offset.x += off.x;
	g_offset.y += off.y;

	// CalROI();
	cout << "Total cost time of 2 pics: " << totaltime.elapsed() << endl;
	return 0;
}

Offset CStitching::CalOffset(vector<Point> train_point, vector<Point> query_point, CalMethod method) {
	Offset off = { -9999, -9999 }; //返回项中出现-9999等说明出现错误
	switch (method) {
		case AVERAGE: {
			//计算所有偏移量的均值
			double sum_x = 0, sum_y = 0;
			int i;
			for (i = 0; i < train_point.size(); i++) {
				sum_x += query_point[i].x - train_point[i].x;
				sum_y += query_point[i].y - train_point[i].y;
			}
			off = { (int)round(sum_x / i), (int)round(sum_y / i) };
			break;
		}
		case HOMO: {
			break;
		}
		case AFFINE: {
			//利用放射变换模型、RANSAC进行变换矩阵的计算
			//矩阵形式  [m1 m2 m3]
			//         [n1 n2 n3]
			//理想情况下（无旋转和畸变）(m3, n3)即为所求的偏移量
			std::vector<uchar> inliers(train_point.size(), 0);
			Mat AffineMatrix = estimateAffine2D(train_point, query_point, inliers, RANSAC, 3, 2000, 0.99, 10);
			off = { (int)round(AffineMatrix.ptr<double>(0)[2]), (int)round(AffineMatrix.ptr<double>(1)[2]) };
			// std::cout << "estimateAffinePartial2D" << AffineMatrix << "\n" << std::flush;
			break;
		}
		case PROSAC: {
			break;
		}
	}

	// timer
	// boost::timer timer;

	return off;
}

vector<Mat> CStitching::CalROI(const Mat &img1, const Mat &img2) {
	Mat ROI1;
	Mat ROI2;
	int rows1 = img1.rows;
	int cols1 = img1.cols;
	int rows2 = img1.rows;
	int cols2 = img2.cols;

	// if (g_offset.x >= 0 and g_offset.y >= 0):
	//     out = np.zeros((max(rows1, g_offset.y + rows2), max(g_offset.x + cols2, cols1)), dtype='uint8')
	//     #左上角图1，右下角图2
	//     print ('1')
	//     ROI_ltx = g_offset.x
	//     ROI_lty = g_offset.y
	//     ROI_rbx = min(g_offset.x + cols2, cols1)
	//     ROI_rby = min(g_offset.y + rows2, rows1)
	//     out[0:rows1, 0:cols1] = lastResult.copy()
	//     ROIimg1 = out[ROI_lty:ROI_rby, ROI_ltx:ROI_rbx].copy()
	//     out[g_offset.y:g_offset.y + rows2, g_offset.x:g_offset.x + cols2] = img2.copy()
	//     ROIimg2 = out[ROI_lty:ROI_rby, ROI_ltx:ROI_rbx].copy()

	if (g_offset.x >= 0 && g_offset.y >= 0) {
		Mat out = Mat(max(rows1, g_offset.y + rows2), max(g_offset.x + cols2, cols1), CV_8U);
		cout << "type 1 : right down" << endl;
		int ROI_ltx = g_offset.x;
		int ROI_lty = g_offset.y;
		int ROI_rbx = min(g_offset.x + cols2, cols1);
		int ROI_rby = min(g_offset.y + rows2, rows1);
		//将两图先直接赋值给out的对应部分，并提取ROI
		img1.copyTo(out(Rect(0, 0, cols1, rows1)));
		Mat ROIimg1 = out(Rect());
		img2.copyTo(out(Rect(g_offset.y, g_offset.x, g_offset.x + cols2, g_offset.y + rows2)));
	}

	// 	elif (g_offset.x >= 0 and g_offset.y < 0)
	// 	: out = np.zeros((-g_offset.y + rows1, max(g_offset.x + cols2, cols1)), dtype = 'uint8')
	// #左下角图1，向右上角图2
	// 			print('2') ROI_ltx = g_offset.x ROI_lty = -g_offset.y ROI_rbx = min(g_offset.x + cols2, cols1) ROI_rby =
	// 	  rows2 out [-g_offset.y:-g_offset.y + rows1, 0:cols1] = lastResult.copy() ROIimg1 =
	// 	  out [ROI_lty:ROI_rby, ROI_ltx:ROI_rbx].copy() out [0:rows2, g_offset.x:g_offset.x + cols2] = img2.copy() ROIimg2 =
	// 	  out [ROI_lty:ROI_rby, ROI_ltx:ROI_rbx].copy() g_offset.y = 0 elif (g_offset.x < 0 and g_offset.y >= 0)
	// 	: out = np.zeros((max(rows1, g_offset.y + rows2), -g_offset.x + cols1), dtype = 'uint8')
	// #右上角图1，左下角图2
	// 			print('3') ROI_ltx = -g_offset.x ROI_lty = g_offset.y ROI_rbx = cols2 ROI_rby =
	// 	  min(g_offset.y + rows2, rows1) out [0:rows1, -g_offset.x:-g_offset.x + cols1] = lastResult.copy() ROIimg1 =
	// 	  out [ROI_lty:ROI_rby, ROI_ltx:ROI_rbx].copy() out [g_offset.y:g_offset.y + rows2, 0:cols2] = img2.copy() ROIimg2 =
	// 	  out [ROI_lty:ROI_rby, ROI_ltx:ROI_rbx].copy() g_offset.x =
	// 	  0 else : out = np.zeros((-g_offset.y + rows1, -g_offset.x + cols1), dtype = 'uint8')
	// #右下角图1，左上角图2
	// 					 print('4') ROI_ltx = -g_offset.x ROI_lty = -g_offset.y ROI_rbx = cols2 ROI_rby =
	// 			   rows2 out [-g_offset.y:-g_offset.y + rows1, -g_offset.x:-g_offset.x + cols1] = lastResult.copy() ROIimg1 =
	// 			   out [ROI_lty:ROI_rby, ROI_ltx:ROI_rbx].copy() out [0:rows2, 0:cols2] = img2.copy() ROIimg2 =
	// 			   out [ROI_lty:ROI_rby, ROI_ltx:ROI_rbx].copy() g_offset.x = 0 g_offset.y = 0

	vector<Mat> ROI(2);
	ROI.push_back(ROI1);
	ROI.push_back(ROI2);
	return (ROI);
}