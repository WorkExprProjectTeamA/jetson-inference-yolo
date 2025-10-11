package org.example.repository;

import org.example.model.Alerts;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;

import java.sql.Timestamp;
import java.util.List;

@Repository
public interface AlertsRepository extends JpaRepository<Alerts, Integer> {

    //1. 특정 Alert 조회
    List<Alerts> findByAlertType(int alertType);

    // 특정 날짜 범위의 경고 조회
    List<Alerts> findByTimestampBetween(Timestamp startTime, Timestamp endTime);

    // 이미지가 있는 Alert만 조회
    List<Alerts> findDistinctByAlertImagesIsNotNull();

    // Heartbeat가 있는 Alert만 조회
    List<Alerts> findDistinctByAlertHeartbeatsIsNotNull();

    // 연관관계 활용 - 특정 Alert의 heartbeat 개수 조회
    @Query("SELECT COUNT(ah) FROM AlertHeartbeats ah WHERE ah.alert.alertId = :alertId")
    long countHeartbeatsByAlertId(@Param("alertId") int alertId);
}


