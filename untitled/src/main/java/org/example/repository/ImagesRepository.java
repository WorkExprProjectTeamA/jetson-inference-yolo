package org.example.repository;

import org.example.model.Images;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;
import java.sql.Timestamp;
import java.util.List;

@Repository
public interface ImagesRepository extends JpaRepository<Images, Integer> {

    //1. 특정 날짜 범위의 이미지 조회
    List<Images> findByTimestampBetween(Timestamp startTime, Timestamp endTime);

    //2. Alert와 연결된 Image만 조회
    @Query("SELECT DISTINCT i FROM Images i JOIN i.alertImages ai")
    List<Images> findImagesWithAlerts();

    //3. 특정 Image의 Alert 개수 조회
    @Query("SELECT COUNT(ai) FROM AlertImages ai WHERE ai.image.imageId = :imageId")
    long countAlertsByImageId(@Param("imageId") int imageId);
}