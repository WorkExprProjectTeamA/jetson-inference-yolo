package org.example.model;

import com.fasterxml.jackson.annotation.JsonBackReference;
import com.fasterxml.jackson.annotation.JsonIgnore;
import com.fasterxml.jackson.annotation.JsonManagedReference;
import jakarta.persistence.*;

import java.util.Objects;

@Entity
@Table(name = "alert_heartbeats")
public class AlertHeartbeats {

    //key
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private int alertHeartbeatId;

    @ManyToOne(fetch = FetchType.LAZY)
    @JsonBackReference("alert-heartbeats")  // 기존 @JsonIgnore 대신 사용
    @JoinColumn(name = "alert_id", nullable = false)
    private Alerts alert;

    @ManyToOne(fetch = FetchType.LAZY)
    @JsonManagedReference("heartbeat-alerts")  // 추가
    @JoinColumn(name = "heartbeat_id", nullable = false)
    private Heartbeats heartbeat;

    public AlertHeartbeats() {}

    // 생성자
    public AlertHeartbeats(Alerts alert, Heartbeats heartbeat) {
        this.alert = alert;
        this.heartbeat = heartbeat;
    }

    //Getter
    public int getAlertHeartbeatId() {
        return alertHeartbeatId;
    }

    public Alerts getAlert() {
        return alert;
    }

    public Heartbeats getHeartbeat() {
        return heartbeat;
    }

    //Setter
    public void setAlertHeartbeatId(int alertHeartbeatId) {
        this.alertHeartbeatId = alertHeartbeatId;
    }

    public void setAlert(Alerts alert) {
        this.alert = alert;
    }

    public void setHeartbeat(Heartbeats heartbeat) {
        this.heartbeat = heartbeat;
    }

    //toString 메서드
    @Override
    public String toString() {
        return "AlertHeartbeats{" +
                "alertHeartbeatId=" + alertHeartbeatId +
                ", alertId=" + (alert != null ? alert.getAlertId() : null) +
                ", heartbeatId=" + (heartbeat != null ? heartbeat.getHeartbeatId() : null) +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof AlertHeartbeats)) return false;
        AlertHeartbeats that = (AlertHeartbeats) o;
        return alertHeartbeatId == that.alertHeartbeatId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(alertHeartbeatId);
    }

}
