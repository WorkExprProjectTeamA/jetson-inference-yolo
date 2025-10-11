package org.example.service;

import org.example.model.Heartbeats;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.integration.annotation.ServiceActivator;
import org.springframework.stereotype.Service;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.sql.Timestamp;

@Service
public class MqttHeartbeatService {

    @Autowired
    private HeartbeatsService heartbeatsService;
    @Autowired
    private SseHeartbeatEmitterService sseHeartbeatEmitterService;


    private final ObjectMapper objectMapper = new ObjectMapper();

    @ServiceActivator(inputChannel = "mqttInputChannel")
    public void handleHeartbeat(String payload) {
        try {
            System.out.println("MQTT 메시지 수신: " + payload);

            // JSON 파싱
            HeartbeatData data = objectMapper.readValue(payload, HeartbeatData.class);

            // Heartbeat 엔티티 생성
            Heartbeats heartbeat = new Heartbeats();
            heartbeat.setCpuUsage(data.getCpuUsage());
            heartbeat.setGpuUsage(data.getGpuUsage());
            heartbeat.setMemoryUsage(data.getMemoryUsage());
            heartbeat.setTemperature(data.getTemperature());
            heartbeat.setInferenceFps(data.getInferenceFps());
            heartbeat.setInferenceSuccessRate(data.getInferenceSuccessRate());
            heartbeat.setTimestamp(new Timestamp(System.currentTimeMillis()));

            // 데이터베이스 저장
            heartbeatsService.saveHeartbeat(heartbeat);

            System.out.println("Heartbeat 데이터 저장 완료");

            // SSE 전송
            sseHeartbeatEmitterService.sendHeartbeat(data);

            System.out.println("Heartbeat 데이터 저장 및 SSE 전송 완료");

        } catch (Exception e) {
            System.err.println("MQTT 메시지 처리 실패: " + e.getMessage());
            e.printStackTrace();
        }
    }

    // 내부 DTO 클래스
    public static class HeartbeatData {
        private float cpuUsage;
        private float gpuUsage;
        private float memoryUsage;
        private float temperature;
        private float inferenceFps;
        private float inferenceSuccessRate;

        // Getter/Setter
        public float getCpuUsage() { return cpuUsage; }
        public void setCpuUsage(float cpuUsage) { this.cpuUsage = cpuUsage; }
        public float getGpuUsage() { return gpuUsage; }
        public void setGpuUsage(float gpuUsage) { this.gpuUsage = gpuUsage; }
        public float getMemoryUsage() { return memoryUsage; }
        public void setMemoryUsage(float memoryUsage) { this.memoryUsage = memoryUsage; }
        public float getTemperature() { return temperature; }
        public void setTemperature(float temperature) { this.temperature = temperature; }
        public float getInferenceFps() { return inferenceFps; }
        public void setInferenceFps(float inferenceFps) { this.inferenceFps = inferenceFps; }
        public float getInferenceSuccessRate() { return inferenceSuccessRate; }
        public void setInferenceSuccessRate(float inferenceSuccessRate) { this.inferenceSuccessRate = inferenceSuccessRate; }
    }
}