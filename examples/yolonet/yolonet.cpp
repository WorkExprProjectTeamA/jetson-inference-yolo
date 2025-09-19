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

#include <signal.h>

//추가
#include <curl/curl.h>
#include <thread>
#include <ctime>
#include <fstream>
#include <sstream>

// MQTT 헤더 추가
#include <MQTTClient.h>
#include <chrono>
#include <sys/sysinfo.h>


bool signal_recieved = false;

//추가
// 이미지 저장 함수 (독립 함수)
std::string saveCurrentFrame(uchar3* image, int width, int height) {
    time_t rawtime;
    struct tm * timeinfo;
    char timestamp[80];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);
    
    std::string filename = "alert_" + std::string(timestamp) + ".jpg";
    
    std::ofstream file(filename, std::ios::binary);
    file.write((char*)image, width * height * 3);
    file.close();
    
    LogVerbose("Image saved: %s\n", filename.c_str());
    return filename;
}


// 이 아래에 추가
class AlertSender {
private:
    std::string server_url;
    
public:
    AlertSender(const std::string& url) : server_url(url) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~AlertSender() {
        curl_global_cleanup();
    }
    
	bool sendAlert(int alertType, const std::string& imagePath) {
        CURL *curl;
        CURLcode res;
        curl_httppost *formpost = nullptr;
        curl_httppost *lastptr = nullptr;
        
        curl = curl_easy_init();
        if (!curl) return false;
        
        std::string jsonData = "{\"alertType\":" + std::to_string(alertType) + "}";
        
        curl_formadd(&formpost, &lastptr,
                    CURLFORM_COPYNAME, "json",
                    CURLFORM_COPYCONTENTS, jsonData.c_str(),
                    CURLFORM_CONTENTTYPE, "application/json",
                    CURLFORM_END);
        
        curl_formadd(&formpost, &lastptr,
                    CURLFORM_COPYNAME, "image",
                    CURLFORM_FILE, imagePath.c_str(),
                    CURLFORM_CONTENTTYPE, "image/jpeg",
                    CURLFORM_END);
        
        std::string fullUrl = server_url + "/embedded/alert-with-image";
        curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            LogVerbose("Alert sent successfully to server\n");
        } else {
            LogError("Failed to send alert: %s\n", curl_easy_strerror(res));
        }
        
        curl_formfree(formpost);
        curl_easy_cleanup(curl);
        
        return (res == CURLE_OK);
    }
};

// MQTT Heartbeat 전송 클래스
class MqttHeartbeatSender {
private:
    MQTTClient client;
    std::string broker_address;
    std::string topic;
    bool connected;
    
public:
    MqttHeartbeatSender(const std::string& address, const std::string& client_id, const std::string& heartbeat_topic) 
        : broker_address(address), topic(heartbeat_topic), connected(false) {
        
        MQTTClient_create(&client, broker_address.c_str(), client_id.c_str(), 
                         MQTTCLIENT_PERSISTENCE_NONE, NULL);
    }
    
    ~MqttHeartbeatSender() {
        if (connected) {
            MQTTClient_disconnect(client, 10000);
        }
        MQTTClient_destroy(&client);
    }
    
    bool connect() {
        MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
        conn_opts.keepAliveInterval = 20;
        conn_opts.cleansession = 1;
        
        int rc = MQTTClient_connect(client, &conn_opts);
        if (rc != MQTTCLIENT_SUCCESS) {
            LogError("MQTT 연결 실패: %d\n", rc);
            return false;
        }
        
        connected = true;
        LogVerbose("MQTT 브로커 연결 성공\n");
        return true;
    }
    
    bool sendHeartbeat() {
        if (!connected) {
            LogError("MQTT 연결되지 않음\n");
            return false;
        }
        
        // JSON 생성 (간단한 시스템 데이터)
        std::string json = createHeartbeatJson();
        
        // MQTT 메시지 전송
        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = (void*)json.c_str();
        pubmsg.payloadlen = json.length();
        pubmsg.qos = 1;
        pubmsg.retained = 0;
        
        MQTTClient_deliveryToken token;
        int rc = MQTTClient_publishMessage(client, topic.c_str(), &pubmsg, &token);
        
        if (rc != MQTTCLIENT_SUCCESS) {
            LogError("MQTT 메시지 전송 실패: %d\n", rc);
            return false;
        }
        
        LogVerbose("Heartbeat 전송 완료: %s\n", json.c_str());
        return true;
    }
    
private:
    std::string createHeartbeatJson() {
        // 간단한 시스템 데이터 생성
        float cpuUsage = 45.0f + (rand() % 30);
        float gpuUsage = 60.0f + (rand() % 25);
        float memoryUsage = 50.0f + (rand() % 40);
        float temperature = 65.0f + (rand() % 15);
        float inferenceFps = 20.0f + (rand() % 10);
        float inferenceSuccessRate = 90.0f + (rand() % 10);
        
        std::ostringstream json;
        json << "{";
        json << "\"cpuUsage\":" << cpuUsage << ",";
        json << "\"gpuUsage\":" << gpuUsage << ",";
        json << "\"memoryUsage\":" << memoryUsage << ",";
        json << "\"temperature\":" << temperature << ",";
        json << "\"inferenceFps\":" << inferenceFps << ",";
        json << "\"inferenceSuccessRate\":" << inferenceSuccessRate;
        json << "}";
        return json.str();
    }
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
	printf("usage: detectnet [--help] [--network=NETWORK] [--threshold=THRESHOLD] ...\n");
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
		LogError("detectnet:  failed to create input stream\n");
		return 1;
	}


	/*
	 * create output stream
	 */
	videoOutput* output = videoOutput::Create(cmdLine, ARG_POSITION(1));
	
	if( !output )
	{
		LogError("detectnet:  failed to create output stream\n");	
		return 1;
	}
	

	yoloNet* net = yoloNet::Create("", "networks/custom-detection/det.onnx", 0.0f, 
						"networks/custom-detection/labels.txt", "");
	
	if( !net )
	{
		LogError("detectnet:  failed to load detectNet model\n");
		return 1;
	}

	//추가
	// HTTP 클라이언트 초기화
	AlertSender alertSender("http://localhost:8080");

	// MQTT 클라이언트 초기화
	MqttHeartbeatSender mqttSender("tcp://localhost:1883", "jetson_client", "heartbeat/data");
	if (!mqttSender.connect()) {
    	LogError("MQTT 연결 실패, heartbeat 기능 비활성화\n");
	}

	// 5분마다 heartbeat 전송하는 스레드 시작
	std::thread heartbeatThread([&mqttSender]() {
    	while (!signal_recieved) {
        	std::this_thread::sleep_for(std::chrono::minutes(5));
        	mqttSender.sendHeartbeat();
    	}
	});
	heartbeatThread.detach();

	
	// parse overlay flags
	const uint32_t overlayFlags = yoloNet::OverlayFlagsFromStr(cmdLine.GetString("overlay", "box,labels,conf"));
	

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
	
		const int numDetections = net->Detect(image, input->GetWidth(), input->GetHeight(), &detections, overlayFlags);
		
		/*
		if( numDetections > 0 )
		{
			LogVerbose("%i objects detected\n", numDetections);
		
			for( int n=0; n < numDetections; n++ )
			{
				LogVerbose("\ndetected obj %i  class #%u (%s)  confidence=%f\n", n, detections[n].ClassID, net->GetClassDesc(detections[n].ClassID), detections[n].Confidence);
				LogVerbose("bounding box %i  (%.2f, %.2f)  (%.2f, %.2f)  w=%.2f  h=%.2f\n", n, detections[n].Left, detections[n].Top, detections[n].Right, detections[n].Bottom, detections[n].Width(), detections[n].Height()); 
			
				if( detections[n].TrackID >= 0 ) // is this a tracked object?
					LogVerbose("tracking  ID %i  status=%i  frames=%i  lost=%i\n", detections[n].TrackID, detections[n].TrackStatus, detections[n].TrackFrames, detections[n].TrackLost);
			}
		}	
		*/

		//추가 (위에꺼 교체)
		if( numDetections > 0 )
		{
			LogVerbose("%i objects detected\n", numDetections);

			// 현재 프레임을 이미지로 저장
			std::string imagePath = saveCurrentFrame(image, input->GetWidth(), input->GetHeight());
		
			// 서버에 알림 전송
			std::thread([&alertSender, imagePath]() {
				alertSender.sendAlert(1, imagePath);
			}).detach();

			for( int n=0; n < numDetections; n++ )
			{
				LogVerbose("\ndetected obj %i  class #%u (%s)  confidence=%f\n", n, detections[n].ClassID, net->GetClassDesc(detections[n].ClassID), detections[n].Confidence);
				LogVerbose("bounding box %i  (%.2f, %.2f)  (%.2f, %.2f)  w=%.2f  h=%.2f\n", n, detections[n].Left, detections[n].Top, detections[n].Right, detections[n].Bottom, detections[n].Width(), detections[n].Height()); 
		
				if( detections[n].TrackID >= 0 )
					LogVerbose("tracking  ID %i  status=%i  frames=%i  lost=%i\n", detections[n].TrackID, detections[n].TrackStatus, detections[n].TrackFrames, detections[n].TrackLost);
			}
		}


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

		// print out timing info
		net->PrintProfilerTimes();
	}
	

	/*
	 * destroy resources
	 */
	LogVerbose("detectnet:  shutting down...\n");
	
	SAFE_DELETE(input);
	SAFE_DELETE(output);
	SAFE_DELETE(net);

	LogVerbose("detectnet:  shutdown complete.\n");
	return 0;
}
//////