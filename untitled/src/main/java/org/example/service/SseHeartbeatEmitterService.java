package org.example.service;

import org.springframework.stereotype.Service;
import org.springframework.web.servlet.mvc.method.annotation.SseEmitter;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

@Service
public class SseHeartbeatEmitterService {

    // 동시 접속 클라이언트를 위한 Thread-safe 리스트
    private final List<SseEmitter> emitters = new CopyOnWriteArrayList<>();

    public SseEmitter createEmitter() {
        SseEmitter emitter = new SseEmitter(Long.MAX_VALUE); // 타임아웃 거의 무한
        emitters.add(emitter);

        // 연결 종료 시 리스트에서 제거
        emitter.onCompletion(() -> emitters.remove(emitter));
        emitter.onTimeout(() -> emitters.remove(emitter));
        emitter.onError((e) -> emitters.remove(emitter));

        return emitter;
    }

    public void sendHeartbeat(Object data) {
        for (SseEmitter emitter : emitters) {
            try {
                emitter.send(data);
            } catch (IOException e) {
                emitters.remove(emitter);
            }
        }
    }
}
