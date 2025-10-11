package org.example.service;

import io.minio.MinioClient;
import io.minio.PutObjectArgs;
import org.springframework.stereotype.Service;
import org.springframework.web.multipart.MultipartFile;

import java.io.InputStream;
import java.util.UUID;

@Service
public class CloudStorageService {

    private final MinioClient minioClient;
    private final String bucketName = "cloude"; // iwinv 버킷 이름

    public CloudStorageService() {
        minioClient = MinioClient.builder()
                .endpoint("https://kr.object.iwinv.kr")
                .credentials("UZL05GGK57XOO3NMZZBS", "BRDON8RneJ2xJtGGCZpWhITJy65nT2xpOMglkAxk")
                .build();
    }

    public String uploadFile(MultipartFile file) throws Exception {
        String fileName = System.currentTimeMillis() + "_" + file.getOriginalFilename();

        minioClient.putObject(
                PutObjectArgs.builder()
                        .bucket(bucketName)
                        .object(fileName)
                        .stream(file.getInputStream(), file.getSize(), -1)
                        .contentType(file.getContentType())
                        .build()
        );

        // 업로드 URL 생성
        return "https://kr.object.iwinv.kr/" + bucketName + "/" + fileName;
    }
}
