package org.example.controller;

import org.example.dto.AlertImagesRequest;
import org.example.model.AlertImages;
import org.example.service.AlertImagesService;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/alert-images")
public class AlertImagesController {

    private final AlertImagesService service;

    public AlertImagesController(AlertImagesService service) {
        this.service = service;
    }

    @GetMapping
    public ResponseEntity<List<AlertImages>> getAll() {
        return ResponseEntity.ok(service.getAll());
    }

    @GetMapping("/{id}")
    public ResponseEntity<AlertImages> getById(@PathVariable int id) {
        return service.getById(id)
                .map(ResponseEntity::ok)
                .orElse(ResponseEntity.notFound().build());
    }

    @GetMapping("/test/{alertId}/{imageId}")
    public ResponseEntity<AlertImages> testCreate(
            @PathVariable(value = "alertId") int alertId,
            @PathVariable(value = "imageId") int imageId) {
        try {
            System.out.println("Path 변수: alertId=" + alertId + ", imageId=" + imageId);
            AlertImages saved = service.createAssociation(alertId, imageId);
            return ResponseEntity.ok(saved);
        } catch (Exception e) {
            e.printStackTrace();
            return ResponseEntity.badRequest().build();
        }
    }

    @PostMapping("/json-test")
    public ResponseEntity<String> testJson(@RequestBody Map<String, Object> data) {
        return ResponseEntity.ok("JSON 수신됨: " + data.toString());
    }


    @PostMapping
    public ResponseEntity<AlertImages> create(@RequestBody Map<String, Object> data) {
        try {
            int alertId = (Integer) data.get("alertId");
            int imageId = (Integer) data.get("imageId");

            System.out.println("받은 데이터: alertId=" + alertId + ", imageId=" + imageId);

            if (alertId <= 0 || imageId <= 0) {
                return ResponseEntity.badRequest().build();
            }

            AlertImages saved = service.createAssociation(alertId, imageId);
            return ResponseEntity.status(HttpStatus.CREATED).body(saved);
        } catch (Exception e) {
            e.printStackTrace();
            return ResponseEntity.badRequest().build();
        }
    }

    @DeleteMapping
    public ResponseEntity<Void> delete(@RequestBody AlertImagesRequest request) {
        try {
            boolean deleted = service.deleteAssociation(request.getAlertId(), request.getImageId());
            if (deleted) {
                return ResponseEntity.ok().build();
            } else {
                return ResponseEntity.notFound().build();
            }
        } catch (Exception e) {
            return ResponseEntity.badRequest().build();
        }
    }
}
