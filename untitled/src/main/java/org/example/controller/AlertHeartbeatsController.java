package org.example.controller;

import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.example.dto.AlertHeartbeatRequest;
import org.example.model.AlertHeartbeats;
import org.example.service.AlertHeartbeatsService;

import java.util.List;

@RestController
@RequestMapping("/alert-heartbeats")
public class AlertHeartbeatsController {

    private final AlertHeartbeatsService service;

    public AlertHeartbeatsController(AlertHeartbeatsService service) {
        this.service = service;
    }

    @GetMapping
    public ResponseEntity<List<AlertHeartbeats>> getAll() {
        return ResponseEntity.ok(service.getAll());
    }

    @PostMapping
    public ResponseEntity<AlertHeartbeats> create(@RequestBody AlertHeartbeatRequest request) {
        try {
            AlertHeartbeats saved = service.createAssociation(request.getAlertId(), request.getHeartbeatId());
            return ResponseEntity.status(HttpStatus.CREATED).body(saved);
        } catch (IllegalArgumentException e) {
            // 실패 시 바디 없이 400 반환
            return ResponseEntity.badRequest().build();
        }
    }
}
