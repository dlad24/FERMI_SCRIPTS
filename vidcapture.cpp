/// vidcapture.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <iostream>
#include <queue>
#include <fstream>
#include <string>
#include <chrono>
#include "SimpleSerial.h"
#include <Windows.h>


using namespace cv;



int main()
{
	utils::logging::setLogLevel(utils::logging::LogLevel::LOG_LEVEL_SILENT); // removes ugly opencv info logging messsages
	char com_port[] = "\\\\.\\COM3"; // connect to pico @ com3 -> change if pico moves to different port
	DWORD COM_BAUD_RATE = CBR_115200; // baudrate 1125200
	SimpleSerial Serial(com_port, COM_BAUD_RATE);
	int reply_wait_time = 2; // increase if empty bits/slow readout
	string syntax_type = "json"; //pico output has to be in the form {%f} for serial library to catch it

	//Connect to Pico
	if (!Serial.connected_) {
		std::cout << "Failed to connect to pico... Check if COM port is connected and not in use" << std::endl;
		waitKey(1000);
		return 0;
	}


	//Connect to camera
	VideoCapture vcap(0, CAP_DSHOW);
	if (!vcap.isOpened()) {
		std::cerr << "ERROR: Could not open camera" << std::endl;
		return 1;
	}

	//Set camera resolution, format, and exposure
	vcap.set(CAP_PROP_FRAME_WIDTH, 1920);
	vcap.set(CAP_PROP_FRAME_HEIGHT, 1080);
	vcap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G')); //For arducam MJPG is the only format that supports 1080p/30fps
	vcap.set(CAP_PROP_EXPOSURE, -4); // adjust value to get about 15fps for maximum spark capture rate


	//grab frame dimensions for saving files
	int frame_width = vcap.get(CAP_PROP_FRAME_WIDTH);
	int frame_height = vcap.get(CAP_PROP_FRAME_HEIGHT);

	//initialize some variables for later use
	std::string filename("spark_1.avi");
	int frames = 0;
	int framerate = 0;
	float threshold = 0.07;  //****SPARKING THRESHOLD VALUE**  adjust to levels right above background
	float val = 0;
	int heartbeat = 0;


	//create logging file to track events
	char tt[100];
	time_t now = time(nullptr);
	auto tm_info = localtime(&now);
	strftime(tt, 100, "%Y-%m-%d_%H-%M-%S.csv", tm_info);
	ofstream log_file(tt);

	//Buffer saves most recent (90) frames to be saved in case of an event
	std::queue<Mat> buffer;

	auto start = std::chrono::system_clock::now();

	std::cout << "Process started... press ESC to quit" << std::endl;

	//Main loop repeatedly checks for sparks, then writes to file
	while (true) {


		//Check for sparks and records video
		while (true) {
			Mat frame;
			vcap >> frame; //save image to frame object
			imshow("Frame", frame); //show image - remove for increased code execution

			//Save image to buffer, if max length is reached, drop the oldest image from queue
			buffer.push(frame);
			if (buffer.size() > 90) {
				buffer.pop();
			}
			if (waitKey(10) >= 0)break; // required so imshow doesn't freeze

			string incoming = Serial.ReadSerialPort(reply_wait_time, syntax_type); // grab current value from pico
			val = 0;


			//try/catch block prevents bad serial bytes from killing the system
			try {
				val = std::stof(incoming); //convert to float value
				heartbeat++;
				if (heartbeat % 1000 == 0) {// heartbeat just prints current and time occasionally to let you know the code is still alive
					auto curr_time = std::chrono::system_clock::now();
					auto curr_time_formatted = std::chrono::system_clock::to_time_t(curr_time);
					std::cout << "Current reading: " << val << " Time: " << std::ctime(&curr_time_formatted) << std::endl;
					heartbeat = 1;
				}
			}
			catch (...) {
				std::cout << "Bad bytes.. skipping values" << std::endl;
				continue;
			}

			//std::cout << val << std::endl;

			//Spark detection
			if (val > threshold) { //if current is greater than threshold value, spark is detected
				std::cout << "Spark Detected" << std::endl;
				char tt[100];
				time_t now = time(nullptr);
				auto tm_info = localtime(&now);
				strftime(tt, 100, "%Y-%m-%d_%H-%M-%S.avi", tm_info);// create filename with timestamp of event
				filename.assign(tt);
				log_file << filename << "\n"; // write filename to log file

				break;//exit loop for video writing
			}

			//Logs 120 frames to get an approximate framerate of the system
			//useful for debugging
			if (frames == 120) {
				auto end = std::chrono::system_clock::now();
				std::chrono::duration<double> elapsed_seconds = end - start;
				std::time_t end_time = std::chrono::system_clock::to_time_t(end);
				framerate = 120 / elapsed_seconds.count();
				std::cout << "\nfinished computation at " << std::ctime(&end_time)
					<< "elapsed time: " << elapsed_seconds.count() << "s" << " Framerate: " << 120 / elapsed_seconds.count() << std::endl;
			}
			frames++;

		}
		//write frames from buffer to avi file
		VideoWriter video(filename, VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, Size(frame_width, frame_height), true);
		while (!buffer.empty()) {
			video.write(buffer.front());
			buffer.pop();
		}

		// Uncomment block below to run background subtraction in parallel
		
		//std::string command = "start \"\" cmd /c \"backgroundsubtractor.exe\" " + filename;
		//const char* comm = command.c_str();
		//system(comm);
		video.release();
		if ((char)27 == waitKey(10)) break; // press esc to properly exit the program
	}

	log_file.close();

	return 0;
}
