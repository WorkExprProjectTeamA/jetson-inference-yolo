package org.example.service;

import org.springframework.transaction.annotation.Transactional;
import org.example.model.Heartbeats;
import org.example.repository.HeartbeatsRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.sql.Timestamp;
import java.util.List;
import java.util.Optional;

@Service
@Transactional(readOnly = true)
public class HeartbeatsService {

    private final HeartbeatsRepository heartbeatsRepository;

    public HeartbeatsService(HeartbeatsRepository heartbeatsRepository) {
        this.heartbeatsRepository = heartbeatsRepository;
    }

    //1. 데이터 저장
    @Transactional
    public Heartbeats saveHeartbeat(Heartbeats heartbeat) {
        return heartbeatsRepository.save(heartbeat);
    }

    //2. 모든 하트비트 조회
    public List<Heartbeats> getAllHeartbeats() {
        return heartbeatsRepository.findAll();
    }

    //3. ID로 조회
    public Optional<Heartbeats> getHeartbeatById(Integer id) {
        return heartbeatsRepository.findById(id);
    }

    //4. 삭제
    @Transactional
    public void deleteHeartbeat(Integer id) {
        heartbeatsRepository.deleteById(id);
    }

    //5. 특정 날짜 범위의 하트비트 조회
    public List<Heartbeats> getHeartbeatsBetween(Timestamp startTime, Timestamp endTime) {
        return heartbeatsRepository.findByTimestampBetween(startTime, endTime);
    }

    //6. Alert와 연결된 Heartbeat만 조회
    public List<Heartbeats> getHeartbeatsWithAlerts() {
        return heartbeatsRepository.findHeartbeatsWithAlerts();
    }

    //7. 특정 Heartbeat의 Alert 개수 조회
    public long getAlertCountByHeartbeatId(int heartbeatId) {
        return heartbeatsRepository.countAlertsByHeartbeatId(heartbeatId);
    }

    // 총 개수 조회
    public long getTotalCount() {
        return heartbeatsRepository.count();
    }

}
