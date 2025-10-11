package org.example.controller;

import org.example.model.Alerts;
import org.example.model.Images;
import org.example.service.AlertsService;
import org.example.service.ImagesService;
import org.example.service.AlertImagesService;
import org.example.service.CloudStorageService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.sql.Timestamp;
import java.util.Map;

@RestController
@RequestMapping("/embedded")
@CrossOrigin(origins = "*")
public class EmbeddedController {

    @Autowired
    private AlertsService alertsService;

    @Autowired
    private ImagesService imagesService;

    @Autowired
    private AlertImagesService alertImagesService;

    @Autowired
    private CloudStorageService cloudStorageService;

    private final ObjectMapper objectMapper = new ObjectMapper();

    @PostMapping("/alert-with-image")
    public ResponseEntity<?> receiveAlertWithImage(
            @RequestPart("json") String jsonData,
            @RequestPart("image") MultipartFile image) {

        try {
            // 1. JSON 데이터 파싱
            Map<String, Object> data = objectMapper.readValue(jsonData, Map.class);
            int alertType = (Integer) data.get("alertType");

            // 2. Alert 생성 및 저장
            Alerts alert = new Alerts();
            alert.setAlertType(alertType);
            alert.setTimestamp(new Timestamp(System.currentTimeMillis()));
            Alerts savedAlert = alertsService.saveAlert(alert);

            // 3. 이미지 클라우드 업로드
            String imageUrl = cloudStorageService.uploadFile(image);

            // 4. 이미지 정보 DB 저장
            Images imageEntity = new Images();
            imageEntity.setImageUrl(imageUrl);
            imageEntity.setFileSize((int) image.getSize());
            imageEntity.setFormat(getFileExtension(image.getOriginalFilename()));
            imageEntity.setTimestamp(new Timestamp(System.currentTimeMillis()));
            imageEntity.setWidth(800);
            imageEntity.setHeight(600);
            Images savedImage = imagesService.saveImage(imageEntity);

            // 5. Alert-Image 연관관계 생성
            alertImagesService.createAssociation(savedAlert.getAlertId(), savedImage.getImageId());

            // 6. 응답 데이터 구성
            Map<String, Object> response = Map.of(
                    "success", true,
                    "alertId", savedAlert.getAlertId(),
                    "imageId", savedImage.getImageId(),
                    "imageUrl", imageUrl,
                    "message", "Alert와 이미지가 성공적으로 저장되었습니다."
            );

            return ResponseEntity.status(HttpStatus.CREATED).body(response);

        } catch (Exception e) {
            e.printStackTrace();
            return ResponseEntity.badRequest().body(Map.of(
                    "success", false,
                    "error", e.getMessage()
            ));
        }
    }

    @GetMapping("/health")
    public ResponseEntity<?> healthCheck() {
        return ResponseEntity.ok(Map.of(
                "status", "OK",
                "timestamp", System.currentTimeMillis()
        ));
    }

    private String getFileExtension(String filename) {
        if (filename == null || filename.lastIndexOf(".") == -1) {
            return "unknown";
        }
        return filename.substring(filename.lastIndexOf(".") + 1);
    }
}