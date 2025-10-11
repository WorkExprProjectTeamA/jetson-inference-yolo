package org.example.repository;

import org.example.model.AlertHeartbeats;
import org.example.model.Alerts;
import org.example.model.Heartbeats;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

import java.util.List;
import java.util.Optional;

public interface AlertHeartbeatsRepository extends JpaRepository<AlertHeartbeats, Integer> {

    long countByAlertAlertId(int alertId);

    //1. 특정 Alert에 연결된 Heartbeats 조회
    List<AlertHeartbeats> findByAlert(Alerts alert);

    //2. 특정 Heartbeat에 연결된 Alerts 조회
    List<AlertHeartbeats> findByHeartbeat(Heartbeats heartbeat);

    //3. Alert와 Heartbeat로 특정 연결 조회
    Optional<AlertHeartbeats> findByAlertAndHeartbeat(Alerts alert, Heartbeats heartbeat);

    //4. Alert ID로 Heartbeat 리스트 조회
    @Query("SELECT a.heartbeat FROM AlertHeartbeats a WHERE a.alert.alertId = :alertId")
    List<Heartbeats> findHeartbeatsByAlertId(int alertId);

    //5. Heartbeat ID로 Alert 리스트 조회
    @Query("SELECT a.alert FROM AlertHeartbeats a WHERE a.heartbeat.heartbeatId = :heartbeatId")
    List<Alerts> findAlertsByHeartbeatId(int heartbeatId);


}
