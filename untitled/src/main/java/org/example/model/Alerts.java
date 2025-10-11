package org.example.model;

import com.fasterxml.jackson.annotation.JsonManagedReference;
import jakarta.persistence.*;
import java.sql.Timestamp;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

@Entity
@Table(name = "alerts")
public class Alerts {

    //key
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    @Column(name = "alert_id")
    private int alertId;

    @Column(name = "alert_type")
    private int alertType;

    @Column(name = "timestamp")
    private Timestamp timestamp;

    //alerts:alert_images=1:N
    @OneToMany(mappedBy = "alert", cascade = CascadeType.ALL, orphanRemoval = true)
    @JsonManagedReference("alert-images")  // 추가
    private List<AlertImages> alertImages = new ArrayList<>();

    //alerts:alert_heartbeats=1:N
    @OneToMany(mappedBy = "alert", cascade = CascadeType.ALL, orphanRemoval = true)
    @JsonManagedReference("alert-heartbeats")  // 추가
    private List<AlertHeartbeats> alertHeartbeats = new ArrayList<>();

    public Alerts() {}

    //매개변수 생성자
    public Alerts(int alertType, Timestamp timestamp) {
        this.alertType = alertType;
        this.timestamp = timestamp;
    }

    //Getter
    public int getAlertId() {
        return alertId;
    }

    public int getAlertType() { return alertType; }

    public Timestamp getTimestamp() {
        return timestamp;
    }

    public List<AlertImages> getAlertImages() {
        return alertImages;
    }

    public List<AlertHeartbeats> getAlertHeartbeats() {
        return alertHeartbeats;
    }

    //Setter
    public void setAlertId(int alertId) {
        this.alertId = alertId;
    }

    public void setTimestamp(Timestamp timestamp) {
        this.timestamp = timestamp;
    }

    public void setAlertType(int alertType) {
        this.alertType = alertType;
    }

    public void setAlertImages(List<AlertImages> alertImages) {
        this.alertImages = alertImages;
    }

    public void setAlertHeartbeats(List<AlertHeartbeats> alertHeartbeats) {
        this.alertHeartbeats = alertHeartbeats;
    }

    //toString 메서드
    @Override
    public String toString() {
        return "Alerts{" +
                "alertId=" + alertId +
                ", alertType=" + alertType +
                ", timestamp=" + timestamp +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof Alerts)) return false;
        Alerts alerts = (Alerts) o;
        return alertId == alerts.alertId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(alertId);
    }
}
