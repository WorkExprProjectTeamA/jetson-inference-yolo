package org.example.service;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.example.model.AlertImages;
import org.example.model.Alerts;
import org.example.model.Images;
import org.example.repository.AlertImagesRepository;
import org.example.repository.AlertsRepository;
import org.example.repository.ImagesRepository;

import java.util.List;
import java.util.Optional;

@Service
//@Transactional(readOnly = true)
public class AlertImagesService {

    private final AlertImagesRepository alertImagesRepository;
    private final AlertsRepository alertsRepository;
    private final ImagesRepository imagesRepository;


    public AlertImagesService(
            AlertImagesRepository alertImagesRepository,
            AlertsRepository alertsRepository,
            ImagesRepository imagesRepository
    ) {
        this.alertImagesRepository = alertImagesRepository;
        this.alertsRepository = alertsRepository;
        this.imagesRepository = imagesRepository;
    }

    //1. 모든 Alert-Image 연관 조회
    public List<AlertImages> getAll() {
        return alertImagesRepository.findAll();
    }

    //2. ID로 Alert-Image 연관 조회
    public Optional<AlertImages> getById(int id) {
        return alertImagesRepository.findById(id);
    }

    // Alert + Image 연결 (생성)
    @Transactional
    public AlertImages createAssociation(int alertId, int imageId) {
        // 입력값 검증
        if (alertId <= 0 || imageId <= 0) {
            throw new IllegalArgumentException("Invalid ID values");
        }

        Alerts alert = alertsRepository.findById(alertId)
                .orElseThrow(() -> new IllegalArgumentException("Alert not found with id: " + alertId));
        Images image = imagesRepository.findById(imageId)
                .orElseThrow(() -> new IllegalArgumentException("Image not found with id: " + imageId));

        // 중복 체크
        Optional<AlertImages> existing = alertImagesRepository.findByAlertAlertIdAndImageImageId(alertId, imageId);
        if (existing.isPresent()) {
            return existing.get();
        }

        AlertImages alertImage = new AlertImages(alert, image);
        return alertImagesRepository.save(alertImage);
    }

    // 수정된 삭제 메서드
    public boolean deleteAssociation(int alertId, int imageId) {
        List<AlertImages> alertImages = alertImagesRepository.findByAlert_AlertId(alertId);

        for (AlertImages ai : alertImages) {
            if (ai.getImage().getImageId() == imageId) {
                alertImagesRepository.delete(ai);
                return true;
            }
        }
        return false;
    }

}
