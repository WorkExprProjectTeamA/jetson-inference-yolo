package org.example.service;

import org.example.repository.AlertHeartbeatsRepository;
import org.example.repository.AlertImagesRepository;
import org.springframework.transaction.annotation.Transactional;
import org.example.model.Alerts;
import org.example.repository.AlertsRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.sql.Timestamp;
import java.util.List;
import java.util.Optional;

@Service
@Transactional(readOnly = true)
public class AlertsService {

    @Autowired
    private AlertImagesRepository alertImagesRepository;

    @Autowired
    private AlertHeartbeatsRepository alertHeartbeatsRepository;

    private final AlertsRepository alertsRepository;

    public AlertsService(AlertsRepository alertsRepository) {
        this.alertsRepository = alertsRepository;
    }

    //1. 데이터 저장
    @Transactional
    public Alerts saveAlert(Alerts alert) {
        return alertsRepository.save(alert);
    }

    //2. 모든 경고 조회
    public List<Alerts> getAllAlerts() {
        return alertsRepository.findAll();
    }

    //3. ID로 조회
    public Optional<Alerts> getAlertById(Integer id) {
        return alertsRepository.findById(id);
    }

    public long getImageCountByAlertId(int alertId) {
        return alertImagesRepository.countByAlertAlertId(alertId);
    }

    public long getHeartbeatCountByAlertId(int alertId) {
        return alertHeartbeatsRepository.countByAlertAlertId(alertId);
    }

    public List<Alerts> getAlertsWithImages() {
        return alertsRepository.findDistinctByAlertImagesIsNotNull();
    }

    public List<Alerts> getAlertsWithHeartbeats() {
        return alertsRepository.findDistinctByAlertHeartbeatsIsNotNull();
    }

    public long getTotalCount() {
        return alertsRepository.count();
    }




    //4. 삭제
    @Transactional
    public void deleteAlert(Integer id) {
        alertsRepository.deleteById(id);
    }

    //5. alert_type으로 조회
    public List<Alerts> getAlertsByType(int alertType) {
        return alertsRepository.findByAlertType(alertType);
    }

    //6. 특정 날짜 범위의 경고 조회
    public List<Alerts> getAlertsBetween(Timestamp startTime, Timestamp endTime) {
        return alertsRepository.findByTimestampBetween(startTime, endTime);
    }

    @Transactional
    public Alerts createAlert(int alertType) {
        Alerts alert = new Alerts();
        alert.setAlertType(alertType);
        alert.setTimestamp(new Timestamp(System.currentTimeMillis()));
        return alertsRepository.save(alert);
    }


}
