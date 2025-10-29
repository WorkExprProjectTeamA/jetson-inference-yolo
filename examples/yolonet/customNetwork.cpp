#include "customNetwork.h"
#include <jetson-utils/logging.h>

// static 변수 정의
std::atomic<int> MqttHeartbeatSender::totalFrames(0);
std::atomic<int> MqttHeartbeatSender::totalDetections(0);
std::chrono::high_resolution_clock::time_point MqttHeartbeatSender::lastStatsReset;

// AlertSender
AlertSender::AlertSender(const std::string& url) : server_url(url) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    LogVerbose("AlertSender Constructor finished.");
}

AlertSender::~AlertSender() {
    curl_global_cleanup();
}

bool AlertSender::sendAlert(int alertType, const std::string& imagePath) {
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


// MqttHeartbeatSender
MqttHeartbeatSender::MqttHeartbeatSender(const std::string& address, const std::string& client_id, const std::string& heartbeat_topic)
    : broker_address(address), topic(heartbeat_topic), connected(false) {

    LogVerbose("MqttHeartbeatSender constructor entered.\n");
    MQTTClient_create(&client, broker_address.c_str(), client_id.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
    
    LogVerbose("Finished creating mqtt client.\n");
    // 통계 초기화
    lastStatsReset = std::chrono::high_resolution_clock::now();
    totalFrames = 0;
    totalDetections = 0;
    LogVerbose("Finished MqttHeartbeatsSender constructor.\n");
}

MqttHeartbeatSender::~MqttHeartbeatSender() {
    if (connected) {
        MQTTClient_disconnect(client, 10000);
    }
    MQTTClient_destroy(&client);
}

bool MqttHeartbeatSender::connect() {
    LogVerbose("MqttHeartbeatSender::connect() entered\n");
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

bool MqttHeartbeatSender::sendHeartbeat() {
    LogVerbose("MqttHeartbeatSender entered.\n");

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

void MqttHeartbeatSender::recordFrame() {
    totalFrames++;
}

void MqttHeartbeatSender::recordDetection() {
    totalDetections++;
}

float MqttHeartbeatSender::getInferenceFps() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastStatsReset);
    if (duration.count() > 0) {
        return (float)totalFrames / duration.count();
    }
    return 0.0f;
}

float MqttHeartbeatSender::getInferenceSuccessRate() {
    if (totalFrames > 0) {
        return ((float)totalDetections / totalFrames) * 100.0f;
    }
    return 0.0f;
}

float MqttHeartbeatSender::getCpuUsage() {
    static long prevTotal = 0, prevIdle = 0;

    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 0.0f;

    std::string line;
    std::getline(file, line);
    std::istringstream iss(line);

    std::string cpu;
    long user, nice, system, idle, iowait, irq, softirq, steal;
    iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    long total = user + nice + system + idle + iowait + irq + softirq + steal;
    long totalIdle = idle + iowait;

    long totalDiff = total - prevTotal;
    long idleDiff = totalIdle - prevIdle;

    float cpuPercent = 0.0f;
    if (totalDiff > 0) {
        cpuPercent = 100.0f * (totalDiff - idleDiff) / totalDiff;
    }

    prevTotal = total;
    prevIdle = totalIdle;

    return cpuPercent;
}

float MqttHeartbeatSender::getGpuUsage() {
    std::ifstream file("/sys/devices/platform/bus@0/17000000.gpu/load");
    if (!file.is_open()) return 0.0f;

    int load;
    file >> load;
    return (float)load;
}

float MqttHeartbeatSender::getMemoryUsage() {
    struct sysinfo info;
    if (sysinfo(&info) != 0) return 0.0f;

    float total = info.totalram;
    float free = info.freeram + info.bufferram;
    return ((total - free) / total) * 100.0f;
}

float MqttHeartbeatSender::getCpuTemperature() {
    std::ifstream file("/sys/devices/virtual/thermal/thermal_zone0/temp");
    if (!file.is_open()) return 0.0f;

    int temp;
    file >> temp;
    return temp / 1000.0f; // 밀리도를 도로 변환
}

std::string MqttHeartbeatSender::createHeartbeatJson() {
    // 실제 시스템 데이터 수집
    float cpuUsage = getCpuUsage();
    float gpuUsage = getGpuUsage();
    float memoryUsage = getMemoryUsage();
    float temperature = getCpuTemperature();
    float inferenceFps = getInferenceFps();
    float inferenceSuccessRate = getInferenceSuccessRate();

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