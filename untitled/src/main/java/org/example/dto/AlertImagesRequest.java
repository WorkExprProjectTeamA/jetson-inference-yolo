package org.example.dto;

import java.util.Map;

public class AlertImagesRequest {
    private int alertId;
    private int imageId;

    public AlertImagesRequest() {}

    // Map을 통한 생성자 추가
    public AlertImagesRequest(Map<String, Object> data) {
        this.alertId = (Integer) data.get("alertId");
        this.imageId = (Integer) data.get("imageId");
    }

    public int getAlertId() { return alertId; }
    public void setAlertId(int alertId) { this.alertId = alertId; }
    public int getImageId() { return imageId; }
    public void setImageId(int imageId) { this.imageId = imageId; }
}