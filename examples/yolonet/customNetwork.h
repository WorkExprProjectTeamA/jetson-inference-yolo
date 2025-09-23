#ifndef __CUSTOM_NETWORK_H__
#define __CUSTOM_NETWORK_H__

//추가
#include <curl/curl.h>
#include <thread>
#include <ctime>
#include <fstream>
#include <sstream>
#include <atomic>

// CUDA 타입(uchar3)을 위한 헤더
#include <jetson-utils/cudaUtility.h>

// MQTT 헤더 추가
#include <MQTTClient.h>
#include <chrono>
#include <sys/sysinfo.h>


class AlertSender {
private:
    std::string server_url;
    
public:
    AlertSender(const std::string& url);
    
    ~AlertSender();
    
	bool sendAlert(int alertType, const std::string& imagePath);
};

// MQTT Heartbeat 전송 클래스
class MqttHeartbeatSender {
public:
    MqttHeartbeatSender(const std::string& address, const std::string& client_id, const std::string& heartbeat_topic);
    
    ~MqttHeartbeatSender();
    
    bool connect();
    
    bool sendHeartbeat();

    // 성능 추적 함수들
    static void recordFrame();

    static void recordDetection();

    float getInferenceFps();

    float getInferenceSuccessRate();

private:
    float getCpuUsage();
    float getGpuUsage();
    float getMemoryUsage();
    float getCpuTemperature();
    std::string createHeartbeatJson();

    MQTTClient client;
    std::string broker_address;
    std::string topic;
    bool connected;

    // 성능 추적 변수들
    static std::atomic<int> totalFrames;
    static std::atomic<int> totalDetections;
    static std::chrono::high_resolution_clock::time_point lastStatsReset;
};

std::string saveCurrentFrame(uchar3* image, int width, int height);

#endif