package org.example.controller;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

@RestController
@RequestMapping("/test")
public class TestController {

    @PostMapping
    public ResponseEntity<String> testJson(@RequestBody Map<String, Object> data) {
        return ResponseEntity.ok("JSON 수신 성공: " + data.toString());
    }
}