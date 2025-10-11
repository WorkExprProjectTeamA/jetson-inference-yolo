package org.example.repository;

import org.example.model.AlertImages;
import org.example.model.Alerts;
import org.example.model.Images;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface AlertImagesRepository extends JpaRepository<AlertImages, Integer> {

    Optional<AlertImages> findByAlertAlertIdAndImageImageId(int alertId, int imageId);

    long countByAlertAlertId(int alertId);

    List<AlertImages> findByAlert_AlertId(int alertId);

    AlertImages findByAlert_AlertIdAndImage_ImageId(int alertId, int imageId);

    //1. Alert 기준으로 연관된 AlertImages 조회
    List<AlertImages> findByAlert(Alerts alert);

    //2. Image 기준으로 연관된 AlertImages 조회
    List<AlertImages> findByImage(Images image);

    //3. 특정 Alert에 연관된 모든 Image 조회
    @Query("SELECT ai.image FROM AlertImages ai WHERE ai.alert.alertId = :alertId")
    List<Images> findImagesByAlertId(@Param("alertId") int alertId);

    //4. 특정 Image에 연관된 모든 Alert 조회
    @Query("SELECT ai.alert FROM AlertImages ai WHERE ai.image.imageId = :imageId")
    List<Alerts> findAlertsByImageId(@Param("imageId") int imageId);

    //5. 특정 Alert + Image 조합 조회
    AlertImages findByAlertAndImage(Alerts alert, Images image);
}
