package org.example.repository;

import org.example.model.Heartbeats;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;
import java.sql.Timestamp;
import java.util.List;

@Repository
public interface HeartbeatsRepository extends JpaRepository<Heartbeats, Integer> {

    //1. 특정 날짜 범위의 하트비트 조회
    List<Heartbeats> findByTimestampBetween(Timestamp startTime, Timestamp endTime);

    //2. Alert와 연결된 Heartbeat만 조회
    @Query("SELECT DISTINCT h FROM Heartbeats h JOIN h.alertHeartbeats ah")
    List<Heartbeats> findHeartbeatsWithAlerts();

    //3. 특정 Heartbeat의 Alert 개수 조회
    @Query("SELECT COUNT(ah) FROM AlertHeartbeats ah WHERE ah.heartbeat.heartbeatId = :heartbeatId")
    long countAlertsByHeartbeatId(@Param("heartbeatId") int heartbeatId);
}