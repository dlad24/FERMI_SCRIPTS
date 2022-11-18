// vidcapture.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <iostream>
#include <queue>
#include <fstream>
#include <string>
#include <vector>
#include <numeric>
#include <time.h>
#include <sstream>

using namespace cv;



int main(int argc, char*argv[])
{
	//Get filename from arg
	std::string filename;
	if (argc > 1) { 
		filename.assign(argv[1]);
		std::cout << "opening file... " << filename << std::endl;
	}
	else {
		filename.assign("spark1.avi");
	}

	//Initialize camera as vcap
	utils::logging::setLogLevel(utils::logging::LogLevel::LOG_LEVEL_SILENT); // removes ugly opencv info logging messsages
	VideoCapture vcap(filename);  // Opens file specified for analysis
	if (!vcap.isOpened()) {
		std::cerr << "ERROR: Could not open camera" << std::endl;
		return 1;
	}

	//create background subtractor objects
	Ptr<BackgroundSubtractor> pBackSub;
	pBackSub = createBackgroundSubtractorKNN();

	//Declare some useful variables
	int frame_count = 0;
	int current_frame = 0;
	int spark_no = 0;

	//Main Loop
	while (true) {
		Mat frame;
		Mat fgMask;
		vcap >> frame; //grab frame from video
		if (frame.empty()) {
			break;
		}
		Mat thresh;

		threshold(frame, thresh, 100, 255,THRESH_BINARY); // apply thresholding to frame

		

		//if (waitKey(10) >= 0)break;

		pBackSub->apply(frame,fgMask); // apply background subtraction algorithm
		//imshow("thresh", fgMask);

		// Determine if spark is present in frame
		// counts white pixels in thresholded image, if greater than threshold spark is present
		// Also ensures at least 6 frames have past since last spark was detected to prevent duplicate events
		if (countNonZero(fgMask) > 25 and frame_count > (current_frame + 6)) { 
			std::cout << "Spark!" << std::endl;

			std::string tag(filename.substr(0, filename.size() - 4));
			imwrite(tag + std::to_string(spark_no)+".bmp", fgMask); // Write sparking frame to file
			current_frame = frame_count;
			spark_no++;
		}
		frame_count++;

	}

	return 0;
}
