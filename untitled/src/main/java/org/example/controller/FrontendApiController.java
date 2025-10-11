package org.example.controller;

import org.example.model.Alerts;
import org.example.model.Heartbeats;
import org.example.model.Images;
import org.example.service.AlertsService;
import org.example.service.HeartbeatsService;
import org.example.service.ImagesService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.CrossOrigin;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;


@RestController
    @RequestMapping("/api")
    @CrossOrigin(origins = {"http://localhost:3000", "http://localhost:5173"})

    public class FrontendApiController {

        @Autowired
        private AlertsService alertsService;

        @Autowired
        private ImagesService imagesService;

        @Autowired
        private HeartbeatsService heartbeatsService;

        // 대시보드 데이터
        @GetMapping("/dashboard")
        public ResponseEntity<Map<String, Object>> getDashboard() {
            Map<String, Object> data = new HashMap<>();
            data.put("totalAlerts", alertsService.getTotalCount());
            data.put("totalImages", imagesService.getTotalCount());
            data.put("totalHeartbeats", heartbeatsService.getTotalCount());

            // 최근 알림 5개
            List<Alerts> recentAlerts = alertsService.getAllAlerts().stream()
                    .sorted((a, b) -> b.getTimestamp().compareTo(a.getTimestamp()))
                    .limit(5)
                    .collect(Collectors.toList());
            data.put("recentAlerts", recentAlerts);

            return ResponseEntity.ok(data);
        }

        // 알림 목록 (페이징)
        @GetMapping("/alerts")
        public ResponseEntity<List<Alerts>> getAllAlerts() {
            return ResponseEntity.ok(alertsService.getAllAlerts());
        }

        // 이미지 목록 (페이징)
        @GetMapping("/images")
        public ResponseEntity<List<Images>> getAllImages() {
            return ResponseEntity.ok(imagesService.getAllImages());
        }

        // 시스템 모니터링 데이터
        @GetMapping("/monitoring")
        public ResponseEntity<List<Heartbeats>> getMonitoring() {
            return ResponseEntity.ok(heartbeatsService.getAllHeartbeats());
        }
    }

