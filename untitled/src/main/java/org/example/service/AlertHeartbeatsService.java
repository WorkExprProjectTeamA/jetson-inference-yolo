package org.example.service;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.example.model.AlertHeartbeats;
import org.example.model.Alerts;
import org.example.model.Heartbeats;
import org.example.repository.AlertHeartbeatsRepository;
import org.example.repository.AlertsRepository;
import org.example.repository.HeartbeatsRepository;

import java.util.List;
import java.util.Optional;

@Service
@Transactional(readOnly = true)
public class AlertHeartbeatsService {

    private final AlertHeartbeatsRepository repository;
    private final AlertsRepository alertsRepository;
    private final HeartbeatsRepository heartbeatsRepository;

    public AlertHeartbeatsService(AlertHeartbeatsRepository repository,
                                  AlertsRepository alertsRepository,
                                  HeartbeatsRepository heartbeatsRepository) {
        this.repository = repository;
        this.alertsRepository = alertsRepository;
        this.heartbeatsRepository = heartbeatsRepository;
    }

    //1. 모든 AlertHeartbeats 조회
    public List<AlertHeartbeats> getAll() {
        return repository.findAll();
    }

    //2. ID로 조회
    public Optional<AlertHeartbeats> getById(int id) {
        return repository.findById(id);
    }

    //3. Alert와 Heartbeat 연결
    @Transactional
    public AlertHeartbeats createAssociation(int alertId, int heartbeatId) {
        Alerts alert = alertsRepository.findById(alertId)
                .orElseThrow(() -> new IllegalArgumentException("Alert가 존재하지 않습니다."));
        Heartbeats heartbeat = heartbeatsRepository.findById(heartbeatId)
                .orElseThrow(() -> new IllegalArgumentException("Heartbeat가 존재하지 않습니다."));

        return repository.findByAlertAndHeartbeat(alert, heartbeat)
                .orElseGet(() -> repository.save(new AlertHeartbeats(alert, heartbeat)));

    }
}
