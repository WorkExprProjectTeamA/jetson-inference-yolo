package org.example.controller;

import org.example.dto.HeartbeatRequest;
import org.example.model.Heartbeats;
import org.example.service.HeartbeatsService;
import org.example.service.SseHeartbeatEmitterService;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.sql.Timestamp;
import java.util.List;

@RestController
@RequestMapping("/heartbeats")
public class HeartbeatsController {

    private final HeartbeatsService service;
    private final SseHeartbeatEmitterService sseHeartbeatEmitterService; // 추가

    public HeartbeatsController(HeartbeatsService service, SseHeartbeatEmitterService sseHeartbeatEmitterService) {

        this.service = service;
        this.sseHeartbeatEmitterService = sseHeartbeatEmitterService;
    }


    //1. 전체 조회
    @GetMapping
    public ResponseEntity<List<Heartbeats>> getAll() {
        return ResponseEntity.ok(service.getAllHeartbeats());
    }

    //SSE용 GET 메서드 추가
    @GetMapping("/heartbeat/stream")
    public SseEmitter streamHeartbeats() {
        return sseHeartbeatEmitterService.createEmitter();
    }


    //2. ID 조회
    @GetMapping("/{id}")
    public ResponseEntity<Heartbeats> getById(@PathVariable Integer id) {
        return service.getHeartbeatById(id)
                .map(ResponseEntity::ok)
                .orElse(ResponseEntity.notFound().build());
    }

    //3. 새 데이터 생성
    @PostMapping
    public ResponseEntity<Heartbeats> create(@RequestBody HeartbeatRequest request) {
        try {
            Heartbeats heartbeat = new Heartbeats();
            heartbeat.setCpuUsage(request.getCpuUsage());
            heartbeat.setGpuUsage(request.getGpuUsage());
            heartbeat.setMemoryUsage(request.getMemoryUsage());
            heartbeat.setTemperature(request.getTemperature());
            heartbeat.setInferenceFps(request.getInferenceFps());
            heartbeat.setInferenceSuccessRate(request.getInferenceSuccessRate());
            heartbeat.setTimestamp(new Timestamp(System.currentTimeMillis()));

            Heartbeats saved = service.saveHeartbeat(heartbeat);
            return ResponseEntity.status(HttpStatus.CREATED).body(saved);
        } catch (Exception e) {
            e.printStackTrace();
            return ResponseEntity.badRequest().build();
        }
    }

    //4. 삭제
    @DeleteMapping("/{id}")
    public ResponseEntity<Void> delete(@PathVariable Integer id) {
        return service.getHeartbeatById(id)
                .map(h -> {
                    service.deleteHeartbeat(id);
                    return ResponseEntity.ok().<Void>build();
                })
                .orElse(ResponseEntity.notFound().build());
    }

    // 총 개수 조회
    @GetMapping("/count")
    public ResponseEntity<Long> getTotalCount() {
        return ResponseEntity.ok(service.getTotalCount());
    }
}
