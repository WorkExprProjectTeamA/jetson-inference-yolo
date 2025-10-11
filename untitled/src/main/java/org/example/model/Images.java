package org.example.model;

import com.fasterxml.jackson.annotation.JsonBackReference;
import com.fasterxml.jackson.annotation.JsonIgnore;
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import jakarta.persistence.*;
import java.sql.Timestamp;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

@JsonIgnoreProperties(ignoreUnknown = true)
@Entity
@Table(name = "images")
public class Images {

    //field
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private int imageId;

    private Timestamp timestamp;
    //private byte[] imageData;
    private String imageUrl;////클라우드 연동 추가!!
    private int fileSize;
    private int width;
    private int height;
    private String format;

    //alerts:alert_images=1:N
    @OneToMany(mappedBy = "image", cascade = CascadeType.ALL, orphanRemoval = true, fetch = FetchType.LAZY)
    @JsonBackReference("image-alerts")  // 기존 @JsonIgnore 대신 사용
    private List<AlertImages> alertImages;

    //기존 생성자
    public Images() {}

    //매개변수 생성자
    public Images(int imageId, Timestamp timestamp, String imageUrl, int fileSize, int width, int height, String format) {
        this.imageId = imageId;
        this.timestamp = timestamp;
        //this.imageData = imageData;
        this.imageUrl = imageUrl;
        this.fileSize = fileSize;
        this.width = width;
        this.height = height;
        this.format = format;
    }

    //Getter
    public int getImageId() { return imageId;}

    public Timestamp getTimestamp() { return timestamp;}

    //public byte[] getImageData() { return imageData;}

    public String getImageUrl() { return imageUrl; }

    public int getFileSize() { return fileSize;}

    public int getWidth() { return width;}

    public int getHeight() { return height;}

    public String getFormat() { return format;}

    public List<AlertImages> getAlertImages() { return alertImages;}

    //Setter
    public void setImageId(int imageId) { this.imageId = imageId;}

    public void setTimestamp(Timestamp timestamp) { this.timestamp = timestamp;}

    //public void setImageData(byte[] imageData) { this.imageData = imageData;}

    public void setImageUrl(String imageUrl) { this.imageUrl = imageUrl; }

    public void setFileSize(int fileSize) { this.fileSize = fileSize;}

    public void setWidth(int width) { this.width = width;}

    public void setHeight(int height) { this.height = height;}

    public void setFormat(String format) { this.format = format;}

    public void setAlertImages(List<AlertImages> alertImages) { this.alertImages = alertImages;}

    //toString 메서드
    @Override
    public String toString() {
        return "Images{" +
                "imageId=" + imageId +
                ", timestamp=" + timestamp +
                //", imageDataLength=" + (imageData != null ? imageData.length : 0) +
                ", fileSize=" + fileSize +
                ", width=" + width +
                ", height=" + height +
                ", format='" + format + '\'' +
                ", alertImagesCount=" + (alertImages != null ? alertImages.size() : 0) +
                '}';
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof Images)) return false;
        Images images = (Images) o;
        return imageId == images.imageId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(imageId);
    }

    // 편의 메서드
    //public long getImageSize() {
      //  return imageData != null ? imageData.length : 0;
    //}

    public String getResolution() {
        return width + "x" + height;
    }

    public double getAspectRatio() {
        return height != 0 ? (double) width / height : 0;
    }

}