//=============================================================================
// Copyright © 2008 Point Grey Research, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of Point
// Grey Research, Inc. ("Confidential Information").  You shall not
// disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with PGR.
//
// PGR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. PGR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================
//=============================================================================
// $Id: MultipleCameraEx.cpp,v 1.17 2010-02-26 01:00:50 soowei Exp $
//=============================================================================

#include "stdafx.h"

#include "FlyCapture2.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <Windows.h>
#include <chrono>
#include <future>
#include <fstream>
#include <thread>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <cuda_runtime.h>

using namespace FlyCapture2;
using namespace std;

Error error;

cv::Mat threshold(cv::Mat M, int thresh, bool useGPU)
{
	/*This function will generate another matrix that has the same dimensions as the input matrix
	 and will use GPU if the given flag is specified to do so*/
	cv::Mat1i New(M.rows, M.cols);
	if (useGPU)
	{
		//Add this later
	}
	else
	{
		for (int i = 0; i < M.rows; i++)
		{
			for (int j = 0; j < M.cols; j++)
			{
				if (M.at<int>(i, j) >= thresh)
				{
					New.at<int>(i, j) = 255;
				}
				else
				{
					New.at<int>(i, j) = 0;
				}
			}
		}
	}
	return New;
}

bool checkImage(string filename, int th1, int th2, bool useGPU)
{
	auto t1 = chrono::high_resolution_clock::now();
	cv::Mat Image = cv::imread(filename, cv::IMREAD_GRAYSCALE);
	if (useGPU)
	{
		/*cv::cuda::GpuMat thresh;
		cv::cuda::CannyEdgeDetector *canny = cv::cuda::createCannyEdgeDetector(th1,th2);
		canny->detect(Image, thresh);
		int s = cv::cuda::countNonZero(thresh);
		auto t2 = chrono::high_resolution_clock::now();
		cout << "Nonzero entries are " + to_string(s) << endl;
		cout << " Time spent 1-2: " << chrono::duration_cast<chrono::milliseconds>(t2 - t1).count() << endl; */
	}
	else
	{
		cv::Mat thresh = threshold(Image, 64, 0);
		//int s = cv::countNonZero(thresh);
		auto t2 = chrono::high_resolution_clock::now();
		//cout << "Rows are " << to_string(s) << endl;
		cout << " Time spent 1-2: " << chrono::duration_cast<chrono::milliseconds>(t2 - t1).count() << endl;
		cv::namedWindow("Name", cv::WINDOW_AUTOSIZE);
		cv::imshow("Name", thresh);
		cv::waitKey(0);
		system("PAUSE");
	}
	return false;
}

bool checkIm(Image &Im, unsigned int thresh, unsigned int rows, unsigned int columns, unsigned int required)
{
	unsigned int Count = 0;
	for (unsigned int i = 0; i < rows; i++)
	{
		for (unsigned int j = 0; j < columns; j++)
		{
			if (*Im(i, j) > thresh)
			{
				Count++;
				if (Count >= required)
				{
					return true;
				}
			}
		}
	}
	return false;
}

Error CheckCamBuffer(Camera* Cam, Image &Im)
{
	Error Er;
	Cam->RetrieveBuffer(&Im);
	return Er;
}

void PrintError(Error error)
{
	error.PrintErrorTrace();
}

int setProperty(Camera &camera, Property &property, PropertyType type,
	const float f)
{
	property.type = type;
	error = camera.GetProperty(&property);
	if (error != PGRERROR_OK)
	{
		PrintError(error);
		return -1;
	}

	property.absControl = true;
	property.onePush = false;
	property.onOff = true;
	property.autoManualMode = false;

	property.absValue = f;

	error = camera.SetProperty(&property);
	if (error != PGRERROR_OK)
	{
		PrintError(error);
		return -1;
	}

	return 0;
}

string get_time()
{
	//Function to get the current timestamp and turn it into a string
	SYSTEMTIME st;
	GetLocalTime(&st);
	string stime;
	unsigned int year = st.wYear;
	unsigned int month = st.wMonth;
	unsigned int hours = st.wHour;
	unsigned int minutes = st.wMinute;
	unsigned int seconds = st.wSecond;
	unsigned int milliseconds = st.wMilliseconds;
	unsigned int day = st.wDay;
	//Concatenate the time stamp string
	stime += to_string(year);
	stime += "-";
	stime += to_string(month);
	stime += "-";
	stime += to_string(day);
	stime += "-";
	stime += to_string(hours);
	stime += "-";
	stime += to_string(minutes);
	stime += "-";
	stime += to_string(seconds);
	stime += "-";
	stime += to_string(milliseconds);
	return stime;
}

class SSS_Camera
{
	/*Put comments about this system in here as we figure out more...*/
public:
	unsigned int id;
	Camera* cam;
	Image Im;
	Image Im1;
	string path;
	string msg;
	unsigned int count;
	string filename;

	SSS_Camera()
	{
		//Default constructor...
	}

	//Constructor method...
	SSS_Camera(unsigned int id, Camera* cam, string path)
	{
		//Image im; //New image assigned to this camera
		Image im;
		Image im1;
		this->id = id;
		this->cam = cam;
		this->Im = im;
		this->path = path + "/Flake";
		this->msg = "Successfully started image capture for camera " + to_string(this->id);
		if (this->id == 1)
		{
			//When high speed camera is being run
			//Image im1;
			this->Im1 = im1;
		}
	}
	//The method to be run within the separate threads
	void run_Cam()
	{
		Error error;
		//cout << this->msg << endl;
		string id = to_string(this->id);
		string root = this->path;
		unsigned int count = 0;
		unsigned int rows = 0;
		unsigned int cols = 0;
		if (this->id == 1)
		{
			while (true)
			{
				error = this->cam->RetrieveBuffer(&this->Im);
				error = this->cam->RetrieveBuffer(&this->Im1);
				Image* Img = &this->Im;
				if (count == 0)
				{
					rows = Img->GetRows();
					cols = Img->GetCols(); //Rows and columns should be the same for every image
				}

				//Filtering operation

				if (/*checkIm(this->Im,64,rows, cols, 100)*/ true)
				{
					//If true save both images
					string t = get_time();
					string file = root + to_string(count) + "_Cam" + id + "_1_" + t + ".bmp";
					string file1 = root + to_string(count) + "_Cam" + id + "_2_" + t + ".bmp";

					error = this->Im.Save(file.c_str());
					error = this->Im1.Save(file1.c_str());
				}
				count++;
			}
			this->count = count;
		}
		else
		{
			while (true)
			{
				error = this->cam->RetrieveBuffer(&this->Im);
				if (/*checkIm(this->Im, 64, rows, cols, 100)*/ true)
				{
					string t = get_time();
					string file = root + to_string(count) + "_Cam" + id + "_" + t + ".bmp";

					error = this->Im.Save(file.c_str());
				}
				count++;
			}
			this->count = count;
		}
	}
};

class Snowflake_System
{
	/*This class will maintain all of the overarching processes of the system, including:
	->Number of cameras and general camera management
	->Asychronous timing in order to timeout certain process
	->Ability to start additional asynchronus threads for other necessary programs such as the empty image filter
	->Ability to time processes using the chrono library
	->Ability to error check the system and break out of certain processes
	*/
public:
	unsigned int numActiveCameras;
	string start_time_stamp;
	string end_time_stamp;
	Camera** sortedCams;
	unsigned int finalCount;
	string lastPic_time_stamp;
	string current_time;

	//The constructor method...
	Snowflake_System(unsigned int &numCams, Camera** sortedCams)
	{
		this->numActiveCameras = numCams;
		this->sortedCams = sortedCams;
		this->start_time_stamp = get_time();
	}
	//Method to clear the camera buffers 
	void clearCamBuffers()
	{
		unsigned int num = this->numActiveCameras;
		unsigned int buffCount = 1;
		//Check to see if all buffers are empty and wait until the buffer is not empty
		Image Im;
		//future* <Error> err = new future<Error>[7];
		future<Error> err;
		while (buffCount != 0)
		{
			buffCount = 0;
			for (unsigned int i = 0; i < num; i++)
			{
				chrono::system_clock::time_point duration = chrono::system_clock::now() + chrono::seconds(1);
				err = async(CheckCamBuffer, this->sortedCams[i], Im); //Run asynchronus thread
				if (future_status::ready == err.wait_until(duration)) //Check to see if the buffer ever finishes running
				{
					buffCount++; //Increment the buffer couter if it did not finish
				}
			}
			cout << "Number of Residual Buffers Found: " << to_string(buffCount) << endl;
		}
	}

	void set_time(string &t)
	{
		//This function will set the current time of the system corresponding to each picture that is taken
		this->lastPic_time_stamp = t;
	}

	void set_current_time()
	{
		this->current_time = get_time(); //Update the current time
	}
};

int get_milliseconds()
{
	//This function is intended for timing processes in milliseconds
	SYSTEMTIME t;
	GetLocalTime(&t);
	int seconds = (t.wSecond) * 1000;
	int milliseconds = t.wMilliseconds;
	int time = seconds + milliseconds;
	return time;
}

void PrintBuildInfo()
{
	FC2Version fc2Version;
	Utilities::GetLibraryVersion(&fc2Version);

	ostringstream version;
	version << "FlyCapture2 library version: " << fc2Version.major << "." << fc2Version.minor << "." << fc2Version.type << "." << fc2Version.build;
	cout << version.str() << endl;

	ostringstream timeStamp;
	timeStamp << "Application build date: " << __DATE__ << " " << __TIME__;
	cout << timeStamp.str() << endl << endl;
}

void PrintCameraInfo(CameraInfo* pCamInfo)
{
	cout << endl;
	cout << "*** CAMERA INFORMATION ***" << endl;
	cout << "Serial number -" << pCamInfo->serialNumber << endl;
	cout << "Camera model - " << pCamInfo->modelName << endl;
	cout << "Camera vendor - " << pCamInfo->vendorName << endl;
	cout << "Sensor - " << pCamInfo->sensorInfo << endl;
	cout << "Resolution - " << pCamInfo->sensorResolution << endl;
	cout << "Firmware version - " << pCamInfo->firmwareVersion << endl;
	cout << "Firmware build time - " << pCamInfo->firmwareBuildTime << endl << endl;


}

/**
* Method for setting the a given property of the referenced camera.
* @param Camera &camera - The Camera object to set parameters on.
*/


int main(int /*argc*/, char** /*argv*/)
{
	//Commands to start running the background program
	fstream logFile;
	string path = "E:/LogFiles/log_" + get_time() + ".txt";
	logFile.open(path, fstream::out);
	logFile << "Starting Time: " << get_time() << "\n";
	PrintBuildInfo();
	EmbeddedImageInfo	EmbeddedInfo;

	BusManager busMgr;
	unsigned int numCameras;
	error = busMgr.GetNumOfCameras(&numCameras);
	if (error != PGRERROR_OK)
	{
		PrintError(error);
		return -1;
	}

	cout << "Number of cameras detected: " << numCameras << endl;
	logFile << "Number of cameras detected: " << numCameras << "\n";

	if (numCameras < 1)
	{
		cout << "Insufficient number of cameras... press Enter to exit." << endl;
		logFile << "INSUFFICIENT NUMBER OF CAMERAS DETECTED. THIS ERROR IS FATAL. ";
		cin.ignore();
		return -1;
	}

	Camera** ppCameras = new Camera*[numCameras];

	// Connect to all detected cameras and attempt to set them to
	// a common video mode and frame rate
	for (unsigned int i = 0; i < numCameras; i++)
	{
		ppCameras[i] = new Camera();

		PGRGuid guid;
		error = busMgr.GetCameraFromIndex(i, &guid);
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}

		// Connect to a camera
		error = ppCameras[i]->Connect(&guid);
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}

		// Get the camera information
		CameraInfo camInfo;
		error = ppCameras[i]->GetCameraInfo(&camInfo);
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}
		PrintCameraInfo(&camInfo);
	}
	Camera** ppCamerasSorted = new Camera*[numCameras];
	for (unsigned int i = 0; i < numCameras; i++)
	{
		PGRGuid guid;
		error = busMgr.GetCameraFromIndex(i, &guid);
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}
		CameraInfo camInfo;
		error = ppCameras[i]->GetCameraInfo(&camInfo);
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}
		//Initializing the camera serial numbers and sorting the array of camera objects
		if (camInfo.serialNumber == 15444692) {
			ppCamerasSorted[0] = ppCameras[i]; //15444692 = camera 0
			cout << "Camera 0 is declared" << "\n";
		}
		if (camInfo.serialNumber == 13510226) {
			ppCamerasSorted[1] = ppCameras[i]; //SPEED CAMERA!!! 13510226 = camera 1
			cout << "Camera 1 is declared" << "\n";
		}
		if (camInfo.serialNumber == 15444697) {
			ppCamerasSorted[2] = ppCameras[i]; //15444697 = camera 2
			cout << "Camera 2 is declared" << "\n";
		}
		if (camInfo.serialNumber == 15444696) {
			ppCamerasSorted[3] = ppCameras[i]; //15444696 = camera 3
			cout << "Camera 3 is declared" << "\n";
		}
		if (camInfo.serialNumber == 15405697) {
			ppCamerasSorted[4] = ppCameras[i]; //15405697 = camera 4
			cout << "Camera 4 is declared" << "\n";
		}
		if (camInfo.serialNumber == 15444687) {
			ppCamerasSorted[5] = ppCameras[i]; //15444687 = camera 5
			cout << "Camera 5 is declared" << "\n";
		}
		if (camInfo.serialNumber == 15444691) {
			ppCamerasSorted[6] = ppCameras[i]; //15444691 = camera 6
			cout << "Camera 6 is declared" << "\n";
		}
	}
	for (unsigned int i = 0; i < numCameras; i++)
	{

		//---------------------------------------------------
		//WE NEED TO CHANGE THE GRABMODE
		//---------------------------------------------------
		FC2Config BufferFrame;
		// Set buffered mode
		error = ppCamerasSorted[i]->GetConfiguration(&BufferFrame);
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}
		BufferFrame.numBuffers = 50;
		BufferFrame.grabMode = BUFFER_FRAMES;
		BufferFrame.highPerformanceRetrieveBuffer = true;
		ppCamerasSorted[i]->SetConfiguration(&BufferFrame);
		error = ppCamerasSorted[i]->GetConfiguration(&BufferFrame);
		cout << "Buffer Mode: " << BufferFrame.grabMode << endl;
		cout << "Number of Buffers: " << BufferFrame.numBuffers << endl;
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}

		PGRGuid guid;

		// Get current trigger settings
		TriggerMode triggerMode;
		error = ppCamerasSorted[i]->GetTriggerMode(&triggerMode);
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}

		// Set camera to trigger mode 0 (standard ext. trigger)
		//   Set GPIO to receive input from pin 0
		triggerMode.onOff = true;
		triggerMode.mode = 0;
		triggerMode.parameter = 0;
		triggerMode.polarity = 0;
		triggerMode.source = 0;
		error = ppCamerasSorted[i]->SetTriggerMode(&triggerMode);
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}
		cout << "  Successfully set to external trigger, low polarity, source GPIO 0." << endl;

		// Set the shutter property of the camera
		Property shutterProp;
		const float k_shutterVal = .04f;

		//cout << "(" << camInfo.serialNumber << ")" << endl;
		if (setProperty(*ppCamerasSorted[i], shutterProp, SHUTTER, k_shutterVal) != 0)
		{
			cout << "  SMAS: could not set shutter property." << endl;
		}
		else
		{
			cout << "  Successfully set shutter time to " << fixed << setprecision(2) << k_shutterVal << " ms" << endl;
		}

		// Set the gain property of the camera
		Property gainProp;
		const float k_gainVal = 16.0f;

		if (setProperty(*ppCamerasSorted[i], gainProp, GAIN, k_gainVal) != 0)
		{
			cout << "  SMAS: could not set gain property." << endl;
		}
		else
		{
			cout << "  Successfully set gain to " << fixed << setprecision(2) << k_gainVal << " dB" << endl;
		}

		// Get current embedded image info
		error = ppCamerasSorted[i]->GetEmbeddedImageInfo(&EmbeddedInfo);
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}

		// If camera supports timestamping, set to true
		if (EmbeddedInfo.timestamp.available == true)
		{
			EmbeddedInfo.timestamp.onOff = true;
			cout << "  Successfully enabled timestamping." << endl;
		}
		else
		{
			cout << "Timestamp is not available!" << endl;
		}

		// If camera supports frame counting, set to true
		if (EmbeddedInfo.frameCounter.available == true)
		{
			EmbeddedInfo.frameCounter.onOff = true;
			cout << "  Successfully enabled frame counting." << endl;
		}
		else
		{
			cout << "Framecounter is not avalable!" << endl;
		}

		// Sets embedded info
		error = ppCamerasSorted[i]->SetEmbeddedImageInfo(&EmbeddedInfo);
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			return -1;
		}

		// Initializing camera capture capability
		error = ppCamerasSorted[i]->StartCapture();
		if (error != PGRERROR_OK)
		{
			PrintError(error);
			cout << "Error starting to capture images. Error from camera " << i << endl
				<< "Press Enter to exit." << endl;
			cin.ignore();
			return -1;
		}
	}

	cout << "\nBeginning capture. Waiting for signal..." << endl;
	logFile << "The sequential setup has been completed. All cameras detected have been declared. \n";
	SSS_Camera* CamList = new SSS_Camera[numCameras];
	string p = "E:/SSSrain";
	bool exitC = false;
	for (unsigned int i = 0; i < numCameras; i++)
	{
		//Clear all camera data first
		error = ppCamerasSorted[i]->ResetStats();
		CamList[i] = SSS_Camera(i, ppCamerasSorted[i], p);
	}
	logFile << "All objects for the parallel threads have been declared. \n";
	//Initialize the system object and clear all of the camera buffers
	//Snowflake_System* Sys = new Snowflake_System(numCameras, ppCamerasSorted);
	/*
	Image image;
	Image image2;
	unsigned int count = 0;

	while (true)
	{
		// Display the timestamps for all cameras to show that the image
		// capture is synchronized for each image
		auto t1 = chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < numCameras; i++)
		{
			if (i == 1)
			{
				//auto t1 = chrono::high_resolution_clock::now();
				error = ppCamerasSorted[i]->RetrieveBuffer(&image); //This gets stuck if the buffer is empty?
				//auto t2 = chrono::high_resolution_clock::now();
				error = ppCamerasSorted[i]->RetrieveBuffer(&image2);
				//auto t3 = chrono::high_resolution_clock::now();
				//cout << " Time spent 1-2: " << chrono::duration_cast<chrono::milliseconds>(t2-t1).count() << endl;
				//cout << " Time spent 2-3: " << chrono::duration_cast<chrono::milliseconds>(t3-t2).count() << endl;
				if (error != PGRERROR_OK)
				{
					PrintError(error);
					return -1;
				}
				//  Save image to disk
				string t = get_time();
				string path = "E:/Flake" + to_string(count) + "_Cam" + to_string(1) + "_1_" + t + ".bmp";
				string path1 = "E:/Flake" + to_string(count) + "_Cam" + to_string(1) + "_2_" + t + ".bmp";
				//Sys->set_time(t);
				error = image.Save(path.c_str());
				error = image2.Save(path1.c_str());
				if (error != PGRERROR_OK)
				{
					PrintError(error);                                                                                    
				}
			}
			else
			{
				//auto t1 = chrono::high_resolution_clock::now();
				error = ppCamerasSorted[i]->RetrieveBuffer(&image); //This gets stuck if the buffer is empty?
				//auto t2 = chrono::high_resolution_clock::now();
				//cout << " Time spent High Res:                                                                                                                                                                                                                                                                                          " << chrono::duration_cast<chrono::milliseconds>(t2-t1).count() << endl;
				if (error != PGRERROR_OK)
				{
					PrintError(error);
				}
				//  Save image to disk
				string path = "E:/Flake" + to_string(count) + "_Cam" + to_string(i) + "_" + get_time() + ".bmp";
				error = image.Save(path.c_str());
			}
		}
		count++;
		auto t2 = chrono::high_resolution_clock::now();
		cout << " Time spent Overall: " << chrono::duration_cast<chrono::milliseconds>(t2 - t1).count() << endl;
		cout << count << endl;

	}

	for (unsigned int i = 0; i < numCameras; i++)
	{
		ppCameras[i]->StopCapture();
		ppCameras[i]->Disconnect();
		delete ppCameras[i];
	}
	delete[] ppCameras;
	delete[] ppCamerasSorted;

	cout << "Done! Press Enter to exit..." << endl;
	cin.ignore();
	*/

	//This is the portion of the code that starts to perform the parallel setup
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	unsigned int nThreads = sysinfo.dwNumberOfProcessors;
	cout << "The program has detected " + to_string(nThreads) + " processors on this CPU." << endl;
	logFile << "The number of processors detected is: " << to_string(nThreads) << ". Time is: " << get_time() << "\n";
	nThreads = numCameras;
	thread* threadList = new thread[nThreads];
	for (unsigned int i = 0; i < nThreads; i++)
	{
		threadList[i] = thread(&SSS_Camera::run_Cam, CamList[i]);
	}
	logFile << "Threads are running. Time is: " << get_time() << "\n";
	logFile.close();
	string command;
	cout << "There are now " + to_string(nThreads) + " threads running!" << endl;
	cout << "Enter 'compress' to stop the camera code and start compressing and post-processing images. " << endl << endl;
	cout << "Enter 'exit' to just stop the camera capture and exit the application. " << endl << endl;
	while (true)
	{
		cin >> command;
		if ((command == "exit") || (command == "compress"))
		{
			break;
		}
		cout << "You typed: " << command << " which is not a valid command. ";
	}
	exitC = true;
	if (command == "exit")
	{
		for (unsigned int i = 0; i < nThreads; i++)
		{
			threadList[i].join(); //Wait for the threads to finish...
		}
		return 0;
	}
	for (unsigned int i = 0; i < nThreads; i++)
	{
		threadList[i].join(); //Wait for the threads to finish...
	}
	cout << "Threads have been terminated. Compression is starting. " << endl; 
	system("compress.bat");
	cout << "The program has been terminated. This message is only here for debugging purposes. " << endl;
	system("PAUSE");
	return 0;
}
