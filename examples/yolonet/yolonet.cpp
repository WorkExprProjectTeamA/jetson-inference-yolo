/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <signal.h>

#include <sys/syscall.h>
#include <unistd.h>
#include <nvtx3/nvToolsExt.h>

#include "yolonet.h"
#include "gstSpeaker.h"

bool signal_received = false;

// ImageBuffer* ImageBuffer::instance = nullptr;
// std::mutex ImageBuffer::instanceMutex;

// DetectionResults* DetectionResults::instance = nullptr;
// std::mutex DetectionResults::instanceMutex;

void sig_handler(int signo)
{
	if( signo == SIGINT )
	{
		LogVerbose("received SIGINT\n");
		signal_received = true;
	}
}

int usage()
{
	printf("usage: yolonet [--help] [--network=NETWORK] [--threshold=THRESHOLD] ...\n");
	printf("                 input [output]\n\n");
	printf("Locate objects in a video/image stream using an object detection DNN.\n");
	printf("See below for additional arguments that may not be shown above.\n\n");
	printf("positional arguments:\n");
	printf("    input           resource URI of input stream  (see videoSource below)\n");
	printf("    output          resource URI of output stream (see videoOutput below)\n\n");

	printf("%s", yoloNet::Usage());
	printf("%s", objectTracker::Usage());
	printf("%s", videoSource::Usage());
	printf("%s", videoOutput::Usage());
	printf("%s", Log::Usage());

	return 0;
}


int main( int argc, char** argv )
{
	/*
	 * parse command line
	 */
	commandLine cmdLine(argc, argv);

	if( cmdLine.GetFlag("help") )
		return usage();


	/*
	 * attach signal handler
	 */
	if( signal(SIGINT, sig_handler) == SIG_ERR )
		LogError("can't catch SIGINT\n");


	/*
	 * create input stream
	 */
	videoSource* input = videoSource::Create(cmdLine, ARG_POSITION(0));

	if( !input )
	{
		LogError("yolonet:  failed to create input stream\n");
		return 1;
	}


	/*
	 * create output stream
	 */
	videoOutput* output = videoOutput::Create(cmdLine, ARG_POSITION(1));
	
	if( !output )
	{
		LogError("yolonet:  failed to create output stream\n");	
		return 1;
	}
	

	yoloNet* net = yoloNet::Create("", "networks/custom-detection/det.onnx", 0.0f, "networks/custom-detection/labels.txt", "");
	
	if( !net )
	{
		LogError("detectnet:  failed to load yoloNet model\n");
		return 1;
	}

	// parse overlay flags
	const uint32_t overlayFlags = yoloNet::OverlayFlagsFromStr(cmdLine.GetString("overlay", "box,labels,conf"));


	// gstSpeaker gstSpeaker("/home/cook/ws/jetson-inference-yolo/data/voices/");


	// // HTTP 클라이언트 초기화
	// AlertSender alertSender("http://58.143.147.48:18080");

	// MQTT 클라이언트 초기화
	// MqttHeartbeatSender mqttSender("tcp://58.143.147.48:11883", "jetson_client", "heartbeat/data");
	// if (!mqttSender.connect()) {
    // 	LogError("MQTT 연결 실패, heartbeat 기능 비활성화\n");
	// }

	// 5분마다 heartbeat 전송하는 스레드 시작
	// std::thread heartbeatThread([&mqttSender]() {
    // 	while (!signal_received) {
    //     	mqttSender.sendHeartbeat();
	// 		LogVerbose("Send heartbeat thread\n");
	// 		//std::this_thread::sleep_for(std::chrono::minutes(1));
	// 		std::this_thread::sleep_for(std::chrono::seconds(10));
    // 	}
	// });
	// heartbeatThread.detach();


    while( !signal_received )
	{
		// capture next image
		uchar3* image = NULL;
		int status = 0;
		
		nvtxRangePush("YOLONet::Capture");
		if( !input->Capture(&image, &status) )
		{
			if( status == videoSource::TIMEOUT )
				continue;
			
			break; // EOS
		}
		nvtxRangePop();

		// detect objects in the frame
		yoloNet::Detection* detections = NULL;
	
		nvtxRangePush("YOLONet::Detect");
		const int numDetections = net->Detect(image, input->GetWidth(), input->GetHeight(), &detections, overlayFlags);
		nvtxRangePop();

		
		if( numDetections > 0 )
		{
			LogVerbose("%i objects detected\n", numDetections);
		
			for( int n=0; n < numDetections; n++ )
			{
				LogVerbose("\ndetected obj %i  class #%u (%s)  confidence=%f\n", n, detections[n].ClassID, net->GetClassDesc(detections[n].ClassID), detections[n].Confidence);
				LogVerbose("bounding box %i  (%.2f, %.2f)  (%.2f, %.2f)  w=%.2f  h=%.2f\n", n, detections[n].Left, detections[n].Top, detections[n].Right, detections[n].Bottom, detections[n].Width(), detections[n].Height()); 
			

				// gstSpeaker.play_warning_sequence_async(0);
				// if (detections[n].ClassID == 0 || detections[n].ClassID == 1) {
				// 	gstSpeaker.play_warning_sequence_async(detections[n].ClassID);
				// }

				// if( detections[n].TrackID >= 0 ) // is this a tracked object?
				// 	LogVerbose("tracking  ID %i  status=%i  frames=%i  lost=%i\n", detections[n].TrackID, detections[n].TrackStatus, detections[n].TrackFrames, detections[n].TrackLost);
			}
		}
        
        // std::string imagePath = saveCurrentFrame(image, input->GetWidth(), input->GetHeight());

		// // 서버에 알림 전송
		// std::thread([&alertSender, imagePath]() {
        //     alertSender.sendAlert(1, imagePath);
        // }).detach();

		if( output != NULL )
		{
			output->Render(image, input->GetWidth(), input->GetHeight());

			// update the status bar
			char str[256];
			sprintf(str, "TensorRT %i.%i.%i | %s | Network %.0f FPS", NV_TENSORRT_MAJOR, NV_TENSORRT_MINOR, NV_TENSORRT_PATCH, precisionTypeToStr(net->GetPrecision()), net->GetNetworkFPS());
			output->SetStatus(str);

			// check if the user quit
			if( !output->IsStreaming() )
				break;
		}

		// print out timing info
		// net->PrintProfilerTimes();
	}

	/*
	 * destroy resources
	 */
	LogVerbose("yolonet:  shutting down...\n");
	
	SAFE_DELETE(input);
	SAFE_DELETE(output);
	SAFE_DELETE(net);

	LogVerbose("yolonet:  shutdown complete.\n");

	return 0;
}

// void captureImages(videoSource* input) {
//     LogVerbose("Image Capture Thread OS ID: %ld\n", syscall(SYS_gettid));
//     ImageBuffer* buffer = ImageBuffer::getInstance();

//     while (!signal_received) {
//         uchar3* image = NULL;
//         int status = 0;

//         if (!input->Capture(&image, &status)) {
//             if (status == videoSource::TIMEOUT) {
//                 continue;
//             }
//             break;
//         }

//         buffer->setImage(image, input->GetWidth(), input->GetHeight());
//     }
// }

// void processInference(yoloNet* net, uint32_t overlayFlags) {
//     LogVerbose("Inference Thread OS ID: %ld\n", syscall(SYS_gettid));
//     ImageBuffer* imageBuffer = ImageBuffer::getInstance();
//     DetectionResults* results = DetectionResults::getInstance();

//     while (!signal_received) {
//         uchar3* image;
//         int width, height;

//         if (!imageBuffer->getImage(image, width, height)) {
//             std::this_thread::sleep_for(std::chrono::milliseconds(10));
//             continue;
//         }

//         MqttHeartbeatSender::recordFrame();

//         yoloNet::Detection* detections = NULL;
//         int numDetections = net->Detect(image, width, height, &detections, overlayFlags);

//         results->setResults(detections, numDetections, image, width, height);
//     }
// }

// void renderOutput(videoOutput* output, yoloNet* net) {
//     LogVerbose("Output Thread OS ID: %ld\n", syscall(SYS_gettid));
//     DetectionResults* results = DetectionResults::getInstance();

//     while (!signal_received) {
//         yoloNet::Detection* detections;
//         int numDetections;
//         uchar3* image;
//         int width, height;

//         if (!results->getResults(detections, numDetections, image, width, height)) {
//             std::this_thread::sleep_for(std::chrono::milliseconds(10));
//             continue;
//         }

//         if (numDetections > 0) {
//             std::pair<int, int> alertClassPriority = {-1, 0};

//             LogVerbose("%i objects detected\n", numDetections);

//             for (int n = 0; n < numDetections; n++) {
//                 LogVerbose("\ndetected obj %i  class #%u (%s)  confidence=%f\n",
//                           n, detections[n].ClassID, net->GetClassDesc(detections[n].ClassID),
//                           detections[n].Confidence);
//                 LogVerbose("bounding box %i  (%.2f, %.2f)  (%.2f, %.2f)  w=%.2f  h=%.2f\n",
//                           n, detections[n].Left, detections[n].Top, detections[n].Right,
//                           detections[n].Bottom, detections[n].Width(), detections[n].Height());

//                 auto it = alertClassPriorities.find(detections[n].ClassID);
//                 if (it != alertClassPriorities.end()) {
//                     if (alertClassPriority.second < it->second) {
//                         alertClassPriority.first = detections[n].ClassID;
//                         alertClassPriority.second = it->second;
//                     }
//                 }
//             }

//             // if (alertClassPriority.first > -1) {
//             //     std::string imagePath = saveCurrentFrame(image, width, height);
//             //     std::thread([&alertSender, imagePath]() {
//             //         alertSender.sendAlert(1, imagePath);
//             //     }).detach();
//             // }
//         }

//         if (output != NULL) {
//             output->Render(image, width, height);

//             char str[256];
//             sprintf(str, "TensorRT %i.%i.%i | %s | Network %.0f FPS",
//                    NV_TENSORRT_MAJOR, NV_TENSORRT_MINOR, NV_TENSORRT_PATCH,
//                    precisionTypeToStr(net->GetPrecision()), net->GetNetworkFPS());
//             output->SetStatus(str);

//             if (!output->IsStreaming()) {
//                 break;
//             }
//         }
//     }
// }