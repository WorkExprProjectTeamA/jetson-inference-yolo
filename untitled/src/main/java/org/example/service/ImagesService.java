package org.example.service;

import org.springframework.transaction.annotation.Transactional;
import org.example.model.Images;
import org.example.repository.ImagesRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.sql.Timestamp;
import java.util.List;
import java.util.Optional;

@Service
@Transactional(readOnly = true)
public class ImagesService {

    private final ImagesRepository imagesRepository;

    public ImagesService(ImagesRepository imagesRepository) {
        this.imagesRepository = imagesRepository;
    }

    //1. 이미지 저장
    @Transactional
    public Images saveImage(Images image) {
        return imagesRepository.save(image);
    }

    //2. 모든 이미지 조회
    public List<Images> getAllImages() {
        return imagesRepository.findAll();
    }

    //3. ID로 조회
    public Optional<Images> getImageById(Integer id) {
        return imagesRepository.findById(id);
    }

    //4. 삭제
    @Transactional
    public void deleteImage(Integer id) {
        imagesRepository.deleteById(id);
    }

    //5. 특정 날짜 범위의 이미지 조회
    public List<Images> getImagesBetween(Timestamp startTime, Timestamp endTime) {
        return imagesRepository.findByTimestampBetween(startTime, endTime);
    }

    //6. Alert와 연결된 Image만 조회
    public List<Images> getImagesWithAlerts() {
        return imagesRepository.findImagesWithAlerts();
    }

    //7. 특정 Image의 Alert 개수 조회
    public long getAlertCountByImageId(int imageId) {
        return imagesRepository.countAlertsByImageId(imageId);
    }

    //8. 총 개수 조회
    public Long getTotalCount() {
        return imagesRepository.count();
    }
}