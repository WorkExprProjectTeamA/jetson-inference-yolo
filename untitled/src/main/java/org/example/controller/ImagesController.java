package org.example.controller;

import org.example.dto.ImageRequest;
import org.example.model.Images;
import org.example.service.CloudStorageService;
import org.example.service.ImagesService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.ResponseEntity;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

import java.sql.Timestamp;
import java.util.List;

@RestController
@RequestMapping("/images")
public class ImagesController {

    @Autowired
    private ImagesService imagesService;

    @Autowired
    private CloudStorageService cloudStorageService;

    //1. 전체 이미지 조회
    @GetMapping
    public ResponseEntity<List<Images>> getAllImages() {
        return ResponseEntity.ok(imagesService.getAllImages());
    }

    //2. ID로 조회
    @GetMapping("/{id}")
    public ResponseEntity<Images> getById(@PathVariable int id) {
        return imagesService.getImageById(id)
                .map(ResponseEntity::ok)
                .orElse(ResponseEntity.notFound().build());
    }

    //3. 파일 업로드로 이미지 생성 (클라우드 추가!!)
    @PostMapping
    public ResponseEntity<Images> createImage(@RequestParam("file") MultipartFile file) {
        try {
            // 클라우드에 파일 업로드
            String imageUrl = cloudStorageService.uploadFile(file);

            // 이미지 정보 데이터베이스에 저장
            Images image = new Images();
            image.setImageUrl(imageUrl);
            image.setFileSize((int) file.getSize());
            image.setFormat(getFileExtension(file.getOriginalFilename()));
            image.setTimestamp(new Timestamp(System.currentTimeMillis()));

            // width, height는 기본값 설정 (실제로는 이미지 분석 라이브러리 필요)
            image.setWidth(800);  // 기본값
            image.setHeight(600); // 기본값

            Images saved = imagesService.saveImage(image);
            return ResponseEntity.status(HttpStatus.CREATED).body(saved);

        } catch (Exception e) {
            e.printStackTrace();
            return ResponseEntity.badRequest().build();
        }
    }

    // 기존 JSON 방식 유지 (테스트용)
    @PostMapping("/json")
    public ResponseEntity<Images> createWithDto(@RequestBody ImageRequest request) {
        try {
            Images image = new Images();
            image.setWidth(request.getWidth());
            image.setHeight(request.getHeight());
            image.setFormat(request.getFormat());
            image.setTimestamp(new Timestamp(System.currentTimeMillis()));
            image.setImageUrl(""); // 빈 URL
            image.setFileSize(0);

            Images saved = imagesService.saveImage(image);
            return ResponseEntity.ok(saved);
        } catch (Exception e) {
            e.printStackTrace();
            return ResponseEntity.badRequest().build();
        }
    }



    // 4. 삭제
    @DeleteMapping("/{id}")
    public ResponseEntity<Void> delete(@PathVariable int id) {
        if (imagesService.getImageById(id).isPresent()) {
            imagesService.deleteImage(id);
            return ResponseEntity.ok().build();
        }
        return ResponseEntity.notFound().build();
    }

    // 5. 총 개수 조회
    @GetMapping("/count")
    public ResponseEntity<Long> getTotalCount() {
        Long count = imagesService.getTotalCount();
        return ResponseEntity.ok(count != null ? count : 0L);
    }

    // 파일 확장자 추출 헬퍼 메소드
    private String getFileExtension(String filename) {
        if (filename == null || filename.lastIndexOf(".") == -1) {
            return "unknown";
        }
        return filename.substring(filename.lastIndexOf(".") + 1);
    }
}
