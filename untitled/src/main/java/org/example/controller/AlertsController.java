package org.example.controller;

import org.example.dto.AlertRequest;
import org.example.model.Alerts;
import org.example.service.AlertsService;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.sql.Timestamp;
import java.util.List;


@RestController
@RequestMapping("/alerts")
public class AlertsController {

    private final AlertsService service;

    public AlertsController(AlertsService service) {
        this.service = service;
    }

    //1. 전체 조회
    @GetMapping
    public ResponseEntity<List<Alerts>> getAllAlerts() {
        return ResponseEntity.ok(service.getAllAlerts());
    }

    //2. ID 조회
    @GetMapping("/{id}")
    public ResponseEntity<Alerts> getById(@PathVariable Integer id) {
        return service.getAlertById(id)
                .map(ResponseEntity::ok)
                .orElse(ResponseEntity.notFound().build());
    }

    //3. alert_type 조회
    @GetMapping("/type/{alertType}")
    public ResponseEntity<List<Alerts>> getByType(@PathVariable int alertType) {
        return ResponseEntity.ok(service.getAlertsByType(alertType));
    }

    // 새 데이터 생성
    // 기존 createAlert 메소드 수정
    @PostMapping
    public ResponseEntity<Alerts> createAlert(@RequestBody AlertRequest request) {
        try {
            Alerts alert = new Alerts();
            alert.setAlertType(request.getAlertType());
            alert.setTimestamp(new Timestamp(System.currentTimeMillis()));

            Alerts saved = service.saveAlert(alert);
            return ResponseEntity.status(HttpStatus.CREATED).body(saved);
        } catch (Exception e) {
            e.printStackTrace();
            return ResponseEntity.badRequest().build();
        }
    }

    // createSimpleAlert 메소드도 수정
    @PostMapping("/create-simple")
    public ResponseEntity<Alerts> createSimpleAlert(@RequestParam("alertType") int alertType) {
        try {
            Alerts saved = service.createAlert(alertType);
            return ResponseEntity.status(HttpStatus.CREATED).body(saved);
        } catch (IllegalArgumentException e) {
            return ResponseEntity.badRequest().build();
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).build();
        }
    }

    // 삭제
    @DeleteMapping("/{id}")
    public ResponseEntity<Void> delete(@PathVariable Integer id) {
        return service.getAlertById(id)
                .map(alert -> {
                    service.deleteAlert(id);
                    return ResponseEntity.ok().<Void>build();
                })
                .orElse(ResponseEntity.notFound().build());
    }


    // 특정 Alert의 이미지 개수 조회
    @GetMapping("/{id}/image-count")
    public ResponseEntity<Long> getImageCount(@PathVariable int id) {
        return ResponseEntity.ok(service.getImageCountByAlertId(id));
    }

    // 특정 Alert의 heartbeat 개수 조회
    @GetMapping("/{id}/heartbeat-count")
    public ResponseEntity<Long> getHeartbeatCount(@PathVariable int id) {
        return ResponseEntity.ok(service.getHeartbeatCountByAlertId(id));
    }


    // 이미지가 있는 Alert만 조회
    @GetMapping("/with-images")
    public ResponseEntity<List<Alerts>> getAlertsWithImages() {
        return ResponseEntity.ok(service.getAlertsWithImages());
    }

    // Heartbeat가 있는 Alert만 조회
    @GetMapping("/with-heartbeats")
    public ResponseEntity<List<Alerts>> getAlertsWithHeartbeats() {
        return ResponseEntity.ok(service.getAlertsWithHeartbeats());
    }

    // 총 개수 조회
    @GetMapping("/count")
    public ResponseEntity<Long> getTotalCount() {
        return ResponseEntity.ok(service.getTotalCount());
    }

    // Timestamp 변환 메소드
    private Timestamp toTimestamp(long millis) {
        return new Timestamp(millis);
    }



}
