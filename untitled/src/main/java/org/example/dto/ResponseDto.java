package org.example.dto;

public class ResponseDto<T> {
    private boolean success;
    private String message;
    private T data;

    // 기본 생성자
    public ResponseDto() {}

    // getter, setter
    public boolean isSuccess() {
        return success;
    }

    public void setSuccess(boolean success) {
        this.success = success;
    }

    public String getMessage() {
        return message;
    }

    public void setMessage(String message) {
        this.message = message;
    }

    public T getData() {
        return data;
    }

    public void setData(T data) {
        this.data = data;
    }

    // 정적 메소드들
    public static <T> ResponseDto<T> success(T data) {
        ResponseDto<T> response = new ResponseDto<>();
        response.setSuccess(true);
        response.setData(data);
        return response;
    }

    public static <T> ResponseDto<T> error(String message) {
        ResponseDto<T> response = new ResponseDto<>();
        response.setSuccess(false);
        response.setMessage(message);
        return response;
    }
}