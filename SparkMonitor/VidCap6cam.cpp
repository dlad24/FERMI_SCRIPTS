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
#include <chrono>

using namespace cv;


// All of this code is identical to VidCapture, refer to documentation there for anything not pertaining to the 6 camera system

int main()
{
	//Initialize camera as vcap
	utils::logging::setLogLevel(utils::logging::LogLevel::LOG_LEVEL_SILENT); // removes ugly opencv info logging messsages


	// Grab cameras and place them in CamList array
	std::vector<VideoCapture> CamList;
	for (int i = 0; i < 6; i++) {
		VideoCapture cam(i,CAP_DSHOW);
		CamList.push_back(cam);
	}

	//Debug/check the cameras to make sure they're active
	// Set the properties of each camera in the list
	for (auto cam : CamList) {
		if (!cam.isOpened()) {
			std::cerr << "ERROR: Could not open camera" << std::endl;
			return 1;
		}

		//leave these properties disabled until the system is powerful enough to run them

		//Set the resolution of each camera, maximum value is 1920x1080p
		//Typically the system will run at about 1/3 that value to maintain frame rates
		//cam.set(CAP_PROP_FRAME_WIDTH, 1920);
		//cam.set(CAP_PROP_FRAME_HEIGHT, 1080);

		//For arducam MJPG is the only format that supports 1080p/30fps
		cam.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G')); 

		//Adjust the exposure to increase the low light sensitivity
		//cam.set(CAP_PROP_EXPOSURE, -4); // adjust value to get about 15fps for maximum spark capture rate (lower val = lower exposure time = higher framerate)
	}

	int bgnoise = 0;
	//std::cout << "Enter background level: ";
	//std::cin >> bgnoise;

	char optical_trigger_grabber;


	bool use_optical_trigger = false;
	while (true) {
		std::cout << "Use Optical Trigger? Y/N: ";
		std::cin >> optical_trigger_grabber;
		if (optical_trigger_grabber == 'y' || optical_trigger_grabber == 'Y') {
			use_optical_trigger = true;
			break;
		}
		if (optical_trigger_grabber == 'n' || optical_trigger_grabber == 'N') {
			break;
		}

	}


	//Grab proper dimension settings from camera
	// adjust for the size of 6 cameras on a single image
	int frame_width = CamList[0].get(CAP_PROP_FRAME_WIDTH) * 3;
	int frame_height = CamList[0].get(CAP_PROP_FRAME_HEIGHT) * 2;

	//default filenames
	std::string str("spark_1.avi");
	std::string filename("spark_1.avi");

	//create background subtractor objects
	Ptr<BackgroundSubtractor> pBackSub;
	pBackSub = createBackgroundSubtractorKNN();


	//Mat object's store individual frames captured from camera
	// buffer stores the last (buf_length) frames -> written to file if spark is captured
	std::queue<Mat> buffer;
	std::vector<int> noise(30, 0);
	int noise_counter = 0;

	int buf_length = 90; // ~4 seconds @ 100fps
	int frame_activity = 0;
	bool bg_flag = true; 
	int frame_number = 0;
	time_t t = time(NULL);
	
	auto start = std::chrono::system_clock::now();
	int frames = 0;
	//Main loop
	while (true) {
		char bg_file[100];

		//Loops until spark is noticed
		while (true) {

			// Loop through each camera in the array and save image captured to framesArr
			std::vector<Mat> framesArr;
			for (auto cam : CamList) {
				Mat frame;
				cam >> frame; //grab frame from camera
				flip(frame, frame, 0); // for some reason each camera takes photos the upside down and backwards, correct for them
				flip(frame, frame, 1);
				framesArr.push_back(frame);
			}
			Mat fgMask;

			//**** This is where you have to select the correct order of cameras***
			//Images from the camera are stored in framesArr 0,1,2,3,4,5
			//Reorder the position of frames in order to be the most legible
			Mat topRow;
			Mat matArrayTop[] = { framesArr[1],framesArr[5],framesArr[2] }; // Arrange frames in the top row of the array from left to right
			hconcat(matArrayTop, 3, topRow); // merge images into single Mat
			Mat matArrayBot[] = { framesArr[4],framesArr[3],framesArr[0] }; // Arrange frames in the bottom row of the array from left to right
			Mat botRow;
			hconcat(matArrayBot, 3, botRow); // merge images into single Mat
			Mat matArrayFinal[] = { topRow,botRow }; 
			Mat frame;
			vconcat(matArrayFinal, 2, frame); // merge top and bottom row into single Mat


			Mat thresh;
			threshold(frame, thresh, 200, 255, THRESH_BINARY);
			cvtColor(thresh, thresh, COLOR_BGR2GRAY);
			if (countNonZero(thresh) > 200000) {
				continue;
			}


			buffer.push(frame); //pushes the frame into the buffer
			

			//Removes oldest frame if buffer exceeds max length
			if (buffer.size() > buf_length) {
				buffer.pop();
			}

			//Ctrl+c twice to break
			if (waitKey(10) >= 0)break;
			
			//opens the flag file to check if spark was detected
			std::ifstream file("flag.txt");
			std::getline(file, str); // grabs first line of file

			//Python code dumps filename with timestamp into flag.txt when spark is detected
			//C code monitors flag file to see when it should save video
			if (!str.empty() && buffer.size() > buf_length * .9) { // flag file has text and buffer is full
				filename.assign(str); // grab filename from flag.txt
				file.close();//close the flag.txt
				std::ofstream ofs("flag.txt", std::ios::out | std::ios::trunc); // wipe the file
				ofs.close(); // close file
				std::cout << "Droege found spark" << std::endl;
				break;
			}
			file.close(); // if nothing in file, close normally
			
			if (use_optical_trigger) {
				//Check for sparks via movement
				pBackSub->apply(thresh, fgMask); //apply bgsubtraction mask
				//imshow("Frame", fgMask); //shows frame on screen


				frame_activity = countNonZero(fgMask); // count how many white pixels in bgsubtracted image
				//std::cout << frame_activity << " ~~~~ " << (std::accumulate(noise.begin(), noise.end(), 0) * 0.07)  << " ~~~~ " << buffer.size() <<  std::endl; //debugging
				bgnoise = (std::accumulate(noise.begin(), noise.end(), 0) * 0.07);
				if (frame_activity > bgnoise && bg_flag == true && buffer.size() > buf_length * .9) { // compute moving average of noise in frames -> spark when past threshold multiplier of value
					struct tm timeinfo;
					localtime_s(&timeinfo, &t);
					std::strftime(bg_file, 100, "cam_trigger_%Y_%m_%d-%H_%M_%S.avi", &timeinfo); // create filename from timestamp
					filename.assign(std::string(bg_file));
					std::cout << "Camera found spark: " << filename << std::endl;
					break; // exit loop and save video
				}
			}

			//debug, helps to see how well the code is running
			if (frames == 120) {
				auto end = std::chrono::system_clock::now();
				std::chrono::duration<double> elapsed_seconds = end - start;
				std::time_t end_time = std::chrono::system_clock::to_time_t(end);
				std::cout << "elapsed time: " << elapsed_seconds.count() << "s" << " Framerate: " << 120 / elapsed_seconds.count() << std::endl;
			}

			//this is all hte same as the single camera system
			if (use_optical_trigger) {
				noise[noise_counter] = frame_activity; // log noise from frame to noise vector
				noise_counter++;
				if (noise_counter >= 29) { // cycles through all values in vector, then starts again from 0
					noise_counter = 0;
					bg_flag = true; // prevents false flags while noise vector is not full
				}
				if (frames % 500 == 0) {
					std::cout << "current background noise: " << frame_activity << std::endl;
				}

			}
			frames++;
			
		}

		//When spark is found, exit loop above and write video to file
		VideoWriter video(filename, VideoWriter::fourcc('M', 'J', 'P', 'G'), 15, Size(frame_width, frame_height), true); // Videowriter object initialized(filename, avi file format, framerate, frame size)
		//write to file
		while (!buffer.empty()) {
			video.write(buffer.front());
			buffer.pop();
		}
		std::string command = "start \"\" cmd /c \"backgroundsubtractor.exe\" " + filename;
		const char* comm = command.c_str();
		system(comm);
		video.release(); // release videowriter object from memory

		//Ctrl+c twice to break
		if ((char)27 == waitKey(10)) break;
	}


	return 0;
}
