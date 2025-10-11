package org.example.exception;

import org.example.dto.ResponseDto;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.MethodArgumentNotValidException;
import org.springframework.http.converter.HttpMessageNotReadableException;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;
import org.springframework.dao.DataIntegrityViolationException;

@RestControllerAdvice
public class GlobalExceptionHandler {

    @ExceptionHandler(Exception.class)
    public ResponseEntity<ResponseDto<String>> handleException(Exception e) {
        System.err.println("서버 오류: " + e.getClass().getSimpleName());
        System.err.println("오류 메시지: " + e.getMessage());
        e.printStackTrace();
        return ResponseEntity.status(500)
                .body(ResponseDto.error("서버 오류: " + e.getMessage()));
    }
}