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

#include "videoSource.h"
#include "videoOutput.h"

#include "yoloNet.h"
#include "objectTracker.h"
#include "gstSpeaker.h"
#include "customNetwork.h"

#include <signal.h>

bool signal_recieved = false;

static std::unordered_map<int, int> alertClassPriorities = {
    // 최고위험 (4) - 화재/폭발/즉시위험
    {26, 4}, // combustible_flammable_materials
    {39, 4}, // combustible_intrusion_welding_zone 
    {40, 4}, // smoking_in_non_smoking_area 
    {25, 4}, // foreign_material_water_oil
    {33, 4}, // cargo_collapse_during_transport
	{7, 4},  // smoking 
    // 고위험 (3) - 인명사고 위험
    {34, 3}, // person_in_forklift_pathway
    {45, 3}, // person_behind_reversing_vehicle
    {1, 3},  // person_without_workwear
    {32, 3}, // unstable_individual_cargo_loading
    {36, 3}, // poor_loading_condition_collapse
	{55, 1}, // forklift_driving_outside_designated_area
    // 중위험 (2) - 구조적 위험/안전규정 위반
    {30, 2}, // stacking_three_levels_or_more 
	{31, 2}, // poor_rack_storage_condition
    {35, 2}, // safety_rule_violation
    {41, 2}, // person_in_cargo_compartment_loading
    {42, 2}, // person_in_cargo_compartment_unloading 
    // 저위험 (1) - 예방/관리 차원
    {24, 1}, // welding_equipment 
    {27, 1}, // sandwich_panel 
    {28, 1}, // forklift_transport_limited_visibility 
    {29, 1}, // rack_loading_surrounding_obstacles 
    {37, 1}, // person_in_external_work_zone 
    {38, 1}, // hand_pallet_cart_two_level_stacking 
    {43, 1}, // insufficient_forklift_path_marking
    {44, 1}, // obstacle_in_front_dock_door 
    {46, 1}, // disorganized_empty_pallets
    {47, 1}, // leaning_inside_rack_safety_line
    {48, 1}, // pallet_warping_damage_corrosion 
    {49, 1}, // person_riding_freight_elevator 
    {50, 1}, // power_strip_without_overload_breaker
    {51, 1}, // missing_fire_extinguisher 
    {52, 1}, // restricted_area_door_open 
    {53, 1}, // cargo_in_escape_route
    {54, 1}  // dock_unloading_disconnected 
};

void sig_handler(int signo)
{
	if( signo == SIGINT )
	{
		LogVerbose("received SIGINT\n");
		signal_recieved = true;
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
	

	yoloNet* net = yoloNet::Create("", "networks/custom-detection/det.onnx", 0.0f, 
						"networks/custom-detection/labels.txt", "");
	
	if( !net )
	{
		LogError("detectnet:  failed to load detectNet model\n");
		return 1;
	}

	// parse overlay flags
	const uint32_t overlayFlags = yoloNet::OverlayFlagsFromStr(cmdLine.GetString("overlay", "box,labels,conf"));


	// // HTTP 클라이언트 초기화
	// AlertSender alertSender("http://localhost:8080");

	// // MQTT 클라이언트 초기화
	// MqttHeartbeatSender mqttSender("tcp://localhost:1883", "jetson_client", "heartbeat/data");
	// if (!mqttSender.connect()) {
    // 	LogError("MQTT 연결 실패, heartbeat 기능 비활성화\n");
	// }

	// // 5분마다 heartbeat 전송하는 스레드 시작
	// std::thread heartbeatThread([&mqttSender]() {
    // 	while (!signal_recieved) {
    //     	std::this_thread::sleep_for(std::chrono::minutes(5));
    //     	mqttSender.sendHeartbeat();
    // 	}
	// });
	// heartbeatThread.detach();


	gstSpeaker gstSpeaker;

	/*
	 * processing loop
	 */
	while( !signal_recieved )
	{
		// capture next image
		uchar3* image = NULL;
		int status = 0;
		
		if( !input->Capture(&image, &status) )
		{
			if( status == videoSource::TIMEOUT )
				continue;
			
			break; // EOS
		}

		// detect objects in the frame
		yoloNet::Detection* detections = NULL;

		// // 프레임 처리 기록
		// MqttHeartbeatSender::recordFrame();

		int numDetections = net->Detect(image, input->GetWidth(), input->GetHeight(), &detections, overlayFlags);

		if( numDetections > 0 )
		{
			std::pair<int, int> alertClassPriority = {-1, 0};

			LogVerbose("%i objects detected\n", numDetections);

			for( int n=0; n < numDetections; n++ )
			{
				LogVerbose("\ndetected obj %i  class #%u (%s)  confidence=%f\n", n, detections[n].ClassID, net->GetClassDesc(detections[n].ClassID), detections[n].Confidence);
				LogVerbose("bounding box %i  (%.2f, %.2f)  (%.2f, %.2f)  w=%.2f  h=%.2f\n", n, detections[n].Left, detections[n].Top, detections[n].Right, detections[n].Bottom, detections[n].Width(), detections[n].Height()); 
			
				auto it = alertClassPriorities.find(detections[n].ClassID);

				if (it != alertClassPriorities.end()) {
					if (alertClassPriority.second < it->second) {
						alertClassPriority.first = detections[n].ClassID;
						alertClassPriority.second = it->second;
					}
				}
			}

			if (alertClassPriority.first > -1) {
				std::thread([alertClassPriority, &gstSpeaker]() {
					// 음성 파일 경로 생성
					std::string basePath = "/home/cook/ws/jetson-inference-yolo/data/voices/";
					std::string filename = std::to_string(alertClassPriority.first) + ".mp3";
					std::string fullPath = basePath + filename;
		  
					if (access(fullPath.c_str(), F_OK) == 0) {
						LogVerbose("Playing alert sound: %s\n", fullPath.c_str());
						gstSpeaker.play_mp3(fullPath);
					} else {
						LogError("Voice file not found: %s\n", fullPath.c_str());
					}
		  
					std::this_thread::sleep_for(std::chrono::seconds(10));
				}).detach();

				// render outputs
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
			}
		}	



		// //추가 (위에꺼 교체)
		// if( numDetections > 0 )
		// {
		// 	// 객체 감지 성공 기록
		// 	MqttHeartbeatSender::recordDetection();

		// 	LogVerbose("%i objects detected\n", numDetections);

		// 	if (numAlerts > 0) {
		// 		// 현재 프레임을 이미지로 저장
		// 		std::string imagePath = saveCurrentFrame(image, input->GetWidth(), input->GetHeight());

		// 		// 서버에 알림 전송
				// std::thread([&alertSender, imagePath]() {
				// 	alertSender.sendAlert(1, imagePath);
				// }).detach();
		// 	}

		// 	for( int n=0; n < numDetections; n++ )
		// 	{
		// 		LogVerbose("\ndetected obj %i  class #%u (%s)  confidence=%f\n", n, detections[n].ClassID, net->GetClassDesc(detections[n].ClassID), detections[n].Confidence);
		// 		LogVerbose("bounding box %i  (%.2f, %.2f)  (%.2f, %.2f)  w=%.2f  h=%.2f\n", n, detections[n].Left, detections[n].Top, detections[n].Right, detections[n].Bottom, detections[n].Width(), detections[n].Height()); 
		
		// 		if( detections[n].TrackID >= 0 )
		// 			LogVerbose("tracking  ID %i  status=%i  frames=%i  lost=%i\n", detections[n].TrackID, detections[n].TrackStatus, detections[n].TrackFrames, detections[n].TrackLost);
		// 	}
		// }


		// print out timing info
		net->PrintProfilerTimes();
	}
	

	/*
	 * destroy resources
	 */
	LogVerbose("yolonet:  shutting down...\n");

	std::this_thread::sleep_for(std::chrono::seconds(5));
	
	SAFE_DELETE(input);
	SAFE_DELETE(output);
	SAFE_DELETE(net);

	LogVerbose("yolonet:  shutdown complete.\n");

	return 0;
}

