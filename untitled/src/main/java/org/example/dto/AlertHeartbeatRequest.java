package org.example.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class AlertHeartbeatRequest {
    @JsonProperty("alertId")
    private int alertId;

    @JsonProperty("heartbeatId")
    private int heartbeatId;

    public AlertHeartbeatRequest() {}

    public AlertHeartbeatRequest(int alertId, int heartbeatId) {
        this.alertId = alertId;
        this.heartbeatId = heartbeatId;
    }

    public int getAlertId() { return alertId; }
    public void setAlertId(int alertId) { this.alertId = alertId; }
    public int getHeartbeatId() { return heartbeatId; }
    public void setHeartbeatId(int heartbeatId) { this.heartbeatId = heartbeatId; }

    @Override
    public String toString() {
        return "AlertHeartbeatRequest{alertId=" + alertId + ", heartbeatId=" + heartbeatId + "}";
    }
}