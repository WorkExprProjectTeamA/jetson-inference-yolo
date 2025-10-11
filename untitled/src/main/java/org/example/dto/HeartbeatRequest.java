package org.example.dto;

public class HeartbeatRequest {
    private float cpuUsage;
    private float gpuUsage;
    private float memoryUsage;
    private float temperature;
    private float inferenceFps;
    private float inferenceSuccessRate;

    public HeartbeatRequest() {}

    // 모든 getter 메소드들
    public float getCpuUsage() { return cpuUsage; }
    public float getGpuUsage() { return gpuUsage; }
    public float getMemoryUsage() { return memoryUsage; }
    public float getTemperature() { return temperature; }
    public float getInferenceFps() { return inferenceFps; }
    public float getInferenceSuccessRate() { return inferenceSuccessRate; }

    // 모든 setter 메소드들
    public void setCpuUsage(float cpuUsage) { this.cpuUsage = cpuUsage; }
    public void setGpuUsage(float gpuUsage) { this.gpuUsage = gpuUsage; }
    public void setMemoryUsage(float memoryUsage) { this.memoryUsage = memoryUsage; }
    public void setTemperature(float temperature) { this.temperature = temperature; }
    public void setInferenceFps(float inferenceFps) { this.inferenceFps = inferenceFps; }
    public void setInferenceSuccessRate(float inferenceSuccessRate) { this.inferenceSuccessRate = inferenceSuccessRate; }
}