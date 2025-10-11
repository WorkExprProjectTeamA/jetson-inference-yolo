package org.example.model;

import com.fasterxml.jackson.annotation.JsonBackReference;
import com.fasterxml.jackson.annotation.JsonIgnore;
import jakarta.persistence.*;
import java.sql.Timestamp;
import java.util.List;
import java.util.Objects;

@Entity
@Table(name = "heartbeats")
public class Heartbeats {

    //field
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private int heartbeatId;

    private Timestamp timestamp;
    private float cpuUsage;
    private float gpuUsage;
    private float memoryUsage;
    private float temperature;
    private Timestamp lastInferenceTime;
    private float inferenceFps;
    private float inferenceSuccessRate;

    //alerts:alert_heartbeats=1:N
    @OneToMany(mappedBy = "heartbeat", cascade = CascadeType.ALL, orphanRemoval = true)
    @JsonBackReference("heartbeat-alerts")  // 기존 @JsonIgnore 대신 사용
    private List<AlertHeartbeats> alertHeartbeats;

    public Heartbeats() {}

    //매개변수 생성자
    public Heartbeats(int heartbeatId, Timestamp timestamp, float cpuUsage, float gpuUsage, float memoryUsage, float temperature, float inferenceFps, Timestamp lastInferenceTime, float inferenceSuccessRate) {
        this.heartbeatId = heartbeatId;
        this.timestamp = timestamp;
        this.cpuUsage = cpuUsage;
        this.gpuUsage = gpuUsage;
        this.memoryUsage = memoryUsage;
        this.temperature = temperature;
        this.inferenceFps = inferenceFps;
        this.lastInferenceTime = lastInferenceTime;
        this.inferenceSuccessRate = inferenceSuccessRate;
    }

    //Getter
    public int getHeartbeatId() {
        return heartbeatId;
    }

    public Timestamp getTimestamp() {
        return timestamp;
    }

    public float getCpuUsage() {
        return cpuUsage;
    }

    public float getGpuUsage() {
        return gpuUsage;
    }

    public float getMemoryUsage() {
        return memoryUsage;
    }

    public float getTemperature() {
        return temperature;
    }

    public Timestamp getLastInferenceTime() {
        return lastInferenceTime;
    }

    public float getInferenceFps() {
        return inferenceFps;
    }

    public float getInferenceSuccessRate() {
        return inferenceSuccessRate;
    }

    public List<AlertHeartbeats> getAlertHeartbeats() {
        return alertHeartbeats;
    }

    //Setter
    public void setHeartbeatId(int heartbeatId) {
        this.heartbeatId = heartbeatId;
    }

    public void setTimestamp(Timestamp timestamp) {
        this.timestamp = timestamp;
    }

    public void setCpuUsage(float cpuUsage) {
        this.cpuUsage = cpuUsage;
    }

    public void setMemoryUsage(float memoryUsage) {
        this.memoryUsage = memoryUsage;
    }

    public void setGpuUsage(float gpuUsage) {
        this.gpuUsage = gpuUsage;
    }

    public void setTemperature(float temperature) {
        this.temperature = temperature;
    }

    public void setLastInferenceTime(Timestamp lastInferenceTime) {
        this.lastInferenceTime = lastInferenceTime;
    }

    public void setInferenceFps(float inferenceFps) {
        this.inferenceFps = inferenceFps;
    }

    public void setInferenceSuccessRate(float inferenceSuccessRate) {
        this.inferenceSuccessRate = inferenceSuccessRate;
    }

    //toString 메서드
    @Override
    public String toString() {
        return "Heartbeats{" +
                "heartbeatId=" + heartbeatId +
                ", timestamp=" + timestamp +
                ", cpuUsage=" + cpuUsage +
                ", gpuUsage=" + gpuUsage +
                ", memoryUsage=" + memoryUsage +
                ", temperature=" + temperature +
                ", lastInferenceTime=" + lastInferenceTime +
                ", inferenceFps=" + inferenceFps +
                ", inferenceSuccessRate=" + inferenceSuccessRate +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof Heartbeats)) return false;
        Heartbeats that = (Heartbeats) o;
        return heartbeatId == that.heartbeatId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(heartbeatId);
    }

}
