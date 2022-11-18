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

int main()
{
	//Initialize camera as vcap
	utils::logging::setLogLevel(utils::logging::LogLevel::LOG_LEVEL_SILENT); // removes ugly opencv info logging messsages
    
    //Connect to camera
	VideoCapture vcap(0, CAP_DSHOW); // vcap object is how the code connects to the camera

	// Check if camera opened properly
	if (!vcap.isOpened()) {
		std::cerr << "ERROR: Could not open camera" << std::endl;
		return 1;
	}

	//Set camera resolution, format, and exposure
	vcap.set(CAP_PROP_FRAME_WIDTH, 1920); // set width
	vcap.set(CAP_PROP_FRAME_HEIGHT, 1080); // set height

	// FOURCC is the video format used to readout from the camera
	// For arducam MJPG is the only format that supports 1080p/30fps
	vcap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G')); 
	vcap.set(CAP_PROP_EXPOSURE, -4); // adjust value to get about 15fps for maximum spark capture rate (lower val = lower exposure time = higher framerate)


	//grab frame dimensions for saving files
	int frame_width = vcap.get(CAP_PROP_FRAME_WIDTH);
	int frame_height = vcap.get(CAP_PROP_FRAME_HEIGHT);

	// Amount of noise present in the frame, updated later
	int bgnoise = 0;
	//std::cout << "Enter background level: ";
	//std::cin >> bgnoise;

	//Prompts the user whether or not to use the optical trigger in addition to current trigger
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


	//default filenames
	std::string str("spark_1.avi");
	std::string filename("spark_1.avi");

	//create background subtractor objects
	Ptr<BackgroundSubtractor> pBackSub; // Background subtractor object, apply to frame to get subtraction
	pBackSub = createBackgroundSubtractorKNN(); // KNN is chosen for mix of detection and speed, other ones didnt work as well, can be upgraded if you figure out OpenCV with video card acceleration


	//Mat object's store individual frames captured from camera
	// buffer stores the last (buf_length) frames -> written to file if spark is captured
	std::queue<Mat> buffer; // stores frames for writeout
	std::vector<int> noise(30, 0); // noise array
	int noise_counter = 0; // noise counter

	int buf_length = 90; // ~4 seconds @ 100fps
	int frame_activity = 0; // amount of active pixels in frame
	bool bg_flag = true; //  flag to ensure background array is filled before checking for sparks // prevents automatic sparks at start of run
	int frame_number = 0;
	time_t t = time(NULL);
	
	auto start = std::chrono::system_clock::now(); // grabs start time
	int frames = 0; // counts number of frames


	//Main loop
	while (true) {
		char bg_file[100];

		//Loops until spark is noticed
		while (true) {
            Mat frame; // Mat object is just an array representation of an image, each pixel contains RGB data
			Mat fgMask; // Mat object for storing background subtraction
			vcap >> frame; // Grab latest frame from camera


			// To improve background subtraction, first threshold the image, in this case all pixels with brightness between 200 and 255 are turned white everything else black
			Mat thresh;
			threshold(frame, thresh, 200, 255, THRESH_BINARY);
			cvtColor(thresh, thresh, COLOR_BGR2GRAY);
			if (countNonZero(thresh) > 200000) { // ignore image if lights are on
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
			if (!str.empty() && buffer.size() > buf_length * .9) { // flag file has text and buffer is full, if buffer isnt full then chances are its a duplicate event
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

				bgnoise = (std::accumulate(noise.begin(), noise.end(), 0) * 0.07); // finds the sparking threshold based off the average background noise in frame

				if (frame_activity > bgnoise && bg_flag == true && buffer.size() > buf_length * .9) { // compute moving average of noise in frames -> spark when past threshold multiplier of value
					struct tm timeinfo;
					localtime_s(&timeinfo, &t);
					std::strftime(bg_file, 100, "cam_trigger_%Y_%m_%d-%H_%M_%S.avi", &timeinfo); // create filename from timestamp
					filename.assign(std::string(bg_file));
					std::cout << "Camera found spark: " << filename << std::endl;
					break; // exit loop and save video
				}
			}

			//used to debug, prints out framerate after 120 frames are collected
			if (frames == 120) {
				auto end = std::chrono::system_clock::now();
				std::chrono::duration<double> elapsed_seconds = end - start;
				std::time_t end_time = std::chrono::system_clock::to_time_t(end);
				std::cout << "elapsed time: " << elapsed_seconds.count() << "s" << " Framerate: " << 120 / elapsed_seconds.count() << std::endl;
			}

			// if optical trigger is in use, updates the noise array for use in thresholding
			if (use_optical_trigger) {
				noise[noise_counter] = frame_activity; // log noise from frame to noise vector
				noise_counter++;
				if (noise_counter >= 29) { // cycles through all values in vector, then starts again from 0
					noise_counter = 0;
					bg_flag = true; // prevents false flags while noise vector is not full
				}
				if (frames % 500 == 0) { // heartbeat to let you know code is still working
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
		// run the background subtraction code on the video
		std::string command = "start \"\" cmd /c \"backgroundsubtractor.exe\" " + filename;
		const char* comm = command.c_str();
		system(comm);
		video.release(); // release videowriter object from memory

		//Ctrl+c twice to break
		if ((char)27 == waitKey(10)) break;
	}


	return 0;
}
