package org.example.model;

import com.fasterxml.jackson.annotation.JsonBackReference;
import com.fasterxml.jackson.annotation.JsonIgnore;
import com.fasterxml.jackson.annotation.JsonManagedReference;
import jakarta.persistence.*;
import java.util.Objects;

@Entity
@Table(name = "alert_images")
public class AlertImages {

    //key
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private int alertImageId;

    @ManyToOne(fetch = FetchType.LAZY)
    @JsonBackReference("alert-images")  // 기존 @JsonIgnore 대신 사용
    @JoinColumn(name = "alert_id", nullable = false)
    private Alerts alert;

    @ManyToOne(fetch = FetchType.LAZY)
    @JsonManagedReference("image-alerts")  // 추가
    @JoinColumn(name = "image_id", nullable = false)
    private Images image;

    public AlertImages() {}

    //매개변수 생성자
    public AlertImages(Alerts alert, Images image) {
        this.alert = alert;
        this.image = image;
    }

    //Getter
    public int getAlertImageId() {
        return alertImageId;
    }

    public Alerts getAlert() {
        return alert;
    }

    public Images getImage() {
        return image;
    }

    //Setter
    public void setAlertImageId(int alertImageId) {
        this.alertImageId = alertImageId;
    }

    public void setAlert(Alerts alert) {
        this.alert = alert;
    }

    public void setImage(Images image) {
        this.image = image;
    }


    //toString 메서드
    @Override
    public String toString() {
        return "AlertImages{" +
                "alertImageId=" + alertImageId +
                ", alertId=" + (alert != null ? alert.getAlertId() : null) +
                ", imageId=" + (image != null ? image.getImageId() : null) +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof AlertImages)) return false;
        AlertImages that = (AlertImages) o;
        return alertImageId == that.alertImageId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(alertImageId);
    }

}
