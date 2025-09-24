# step4_infer_overlay.py
# -*- coding: utf-8 -*-

print("스크립트 시작")

import sys, time, datetime, collections
from pathlib import Path
import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

from PySide6.QtCore import Qt, QTimer, QSize, Signal, QObject
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import (
    QApplication, QWidget, QLabel, QListWidget, QPushButton, QListWidgetItem,
    QHBoxLayout, QVBoxLayout, QFileDialog, QMessageBox, QCheckBox, QComboBox
)
from PySide6.QtMultimedia import QMediaPlayer, QAudioOutput
from PySide6.QtMultimediaWidgets import QVideoWidget

# === Flask 추가 ===
from flask import Flask, request


# =========================[ 설정 ]=========================
# 여러분 모델 경로(.pt 또는 .onnx). 둘 중 하나만 써도 됩니다.
MODEL_PATH_DET = "/Users/lyw/aigub/121.물류창고 내 작업 안전 데이터/runs/detect/det.pt"   # 또는 .onnx
MODEL_PATH_SEG = "/Users/lyw/aigub/121.물류창고 내 작업 안전 데이터/runs/segment/seg.pt"   # 또는 .pt
USE_MODEL = "det"  # "det" 또는 "seg" 버튼으로 런타임 전환 가능

# 트리거: 위험 상황 라벨(28~55번)
TRIGGER_LABELS = {
    "지게차 운반 시 시야 미확보",
    "랙 적재 시 주변 장애물 존재",
    "3단 이상 평치 적재",
    "랙 보관 적재상태 불량",
    "운반장비 적재 시 개별물류 불안정",
    "운반 중 화물 붕괴",
    "지게차 이동 통로에 사람",
    "안전수칙 미준수",
    "운반 시 적재상태 불량/붕괴",
    "외부 전용작업구역 내 사람",
    "핸드파렛트카 2단 이상 적재",
    "용접구역 내 가연물 침범",
    "비흡연 구역 내 흡연",
    "입고 시 화물칸에 사람",
    "출고 시 화물칸에 사람",
    "지게차 통로 표시 미흡",
    "도크 출입문 앞 장애물",
    "후진 접차 시 후방에 사람",
    "빈 파렛트 미정돈",
    "랙 안전선 내부 기대기",
    "파렛트 비틀림/파손/부식",
    "화물 승강기에 사람 탑승",
    "과부하 차단기 없는 멀티탭",
    "소화기 미비치",
    "출입제한 구역문 개방",
    "대피로 내 적재물",
    "도크 분리 상태 하역",
    "지게차 이동영역 이탈 주행",
}

# 사람/지게차 근접 위험
PERSON_LABELS = {"사람(작업복)", "사람(작업복 미착용)"}  # ID 0,1 통합
FORKLIFT_LABEL = "지게차"                             # ID 3
PIXEL_DIST_THRESH = 60  # 중심점 간 픽셀 거리 임계값 (해상도에 맞게 조정)

# 저장/버퍼
PRE_SEC, POST_SEC = 5, 5  # 사전/사후 N초
# ========================================================

BASE_DIR = Path(__file__).resolve().parent
EVENT_DIR = BASE_DIR / "events"
EVENT_DIR.mkdir(parents=True, exist_ok=True)


# === Flask 추가 ===
app = Flask(__name__)
qt_window = None  # MainWindow 참조용

@app.route("/event", methods=["POST"])
def receive_event():
    label = request.form.get("label", "unknown")
    img_file = request.files.get("image")

    if img_file:
        save_path = EVENT_DIR / f"{label}_{img_file.filename}"
        img_file.save(save_path)
        print(f"[Frontend] Received event: {label}, saved {save_path}")

        if qt_window:  # GUI 업데이트
            qt_window.event_list.addItem(str(save_path))

        return {"status": "ok"}, 200
    return {"status": "fail"}, 400

def run_flask():
    app.run(host="0.0.0.0", port=5000, debug=False, use_reloader=False)
#



# 젯슨나노 환경 감지
def is_jetson_nano():
    """젯슨나노 환경인지 확인"""
    try:
        with open('/proc/device-tree/model', 'r') as f:
            model = f.read().strip()
            return 'jetson' in model.lower() or 'nano' in model.lower()
    except:
        return False

def get_jetson_csi_pipeline(sensor_id=0, width=1280, height=720, fps=30):
    """젯슨나노 CSI 카메라용 GStreamer 파이프라인 생성"""
    return (
        f"nvarguscamerasrc sensor-id={sensor_id} ! "
        f"video/x-raw(memory:NVMM), width={width}, height={height}, "
        f"format=NV12, framerate={fps}/1 ! "
        f"nvvidconv flip-method=0 ! "
        f"video/x-raw, width={width}, height={height}, format=BGRx ! "
        f"videoconvert ! "
        f"video/x-raw, format=BGR ! appsink"
    )

# Ultralytics 로더
try:
    from ultralytics import YOLO
except Exception as e:
    print("Ultralytics가 필요합니다. 가상환경에서 실행 후 다음을 설치하세요:")
    print("  pip install ultralytics onnxruntime")
    raise

def load_model(kind: str):
    path = MODEL_PATH_DET if kind == "det" else MODEL_PATH_SEG
    if not Path(path).exists():
        raise FileNotFoundError(f"모델 파일을 찾을 수 없습니다: {path}")
    model = YOLO(path)
    # model.fuse()  # (선택) 일부 백엔드에서 속도 개선
    return model

# -------- 이벤트 레코더(사전/사후) --------
class EventRecorder(QObject):
    saved = Signal(str)  # 저장 완료 시 파일 경로

    def __init__(self, fps: int, pre_sec: int = PRE_SEC, post_sec: int = POST_SEC):
        super().__init__()
        self.fps = max(5, min(60, int(fps) if fps and not np.isnan(fps) else 30))
        self.prebuf = collections.deque(maxlen=int(pre_sec * self.fps))
        self.post_frames = 0
        self.recording = False
        self.writer = None
        self.size = None
        self._last_out_path = None
        self.post_sec = post_sec

    def feed(self, frame):
        self.prebuf.append(frame.copy())
        if self.recording and self.writer:
            self.writer.write(frame)
            if self.post_frames > 0:
                self.post_frames -= 1
            else:
                self._stop()

    def trigger(self, size, label="event"):
        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        out = EVENT_DIR / f"{label}_{ts}.mp4"
        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        self.size = size
        self.writer = cv2.VideoWriter(str(out), fourcc, self.fps, self.size)
        for f in list(self.prebuf):
            self.writer.write(f)
        self.post_frames = int(self.post_sec * self.fps)
        self.recording = True
        self._last_out_path = str(out)
        print(f"[Event] start -> {out}")

    def extend(self):
        self.post_frames = int(self.post_sec * self.fps)

    def _stop(self):
        if self.writer:
            self.writer.release()
            self.writer = None
            self.recording = False
            path = self._last_out_path or ""
            print(f"[Event] saved -> {path}")
            self.saved.emit(path)

# -------- 간단한 재생 다이얼로그 --------
class PlayerDialog(QWidget):
    def __init__(self, path: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle(Path(path).name)
        self.resize(960, 540)
        self.video = QVideoWidget(self)
        self.player = QMediaPlayer(self)
        self.audio = QAudioOutput(self)
        self.player.setAudioOutput(self.audio)
        self.player.setVideoOutput(self.video)
        lay = QVBoxLayout(self); lay.addWidget(self.video)
        self.player.setSource(Path(path).as_uri()); self.player.play()

# -------- 카메라 소스 --------
class CameraSource:
    def __init__(self, camera_id=0, use_csi=False):
        self.camera_id = camera_id
        self.use_csi = use_csi
        
        if use_csi:
            # 젯슨나노 CSI 카메라용 GStreamer 파이프라인
            pipeline = get_jetson_csi_pipeline(camera_id, 1280, 720, 30)
            self.cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)
            print(f"[Jetson] CSI 카메라 {camera_id} 연결 시도: {pipeline}")
        else:
            # 일반 USB 카메라
            self.cap = cv2.VideoCapture(camera_id)
        
        if not self.cap.isOpened():
            camera_type = "CSI" if use_csi else "USB"
            raise RuntimeError(f"{camera_type} 카메라를 열 수 없습니다: {camera_id}")
        
        if not use_csi:
            # USB 카메라 설정 최적화 (CSI는 파이프라인에서 설정됨)
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
            self.cap.set(cv2.CAP_PROP_FPS, 30)
        
        # 실제 설정값 읽기
        self.fps = self.cap.get(cv2.CAP_PROP_FPS) or 30
        self.w = int(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH)) or 1280
        self.h = int(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT)) or 720
        
        print(f"[Camera] 해상도: {self.w}x{self.h}, FPS: {self.fps}")

    def read(self):
        ok, frame = self.cap.read()
        return ok, frame

    def release(self):
        if self.cap:
            self.cap.release()

    @staticmethod
    def get_available_cameras():
        """사용 가능한 카메라 목록 반환"""
        available = []
        
        # 젯슨나노 환경인지 확인
        is_jetson = is_jetson_nano()
        
        if is_jetson:
            # CSI 카메라 체크 (보통 0, 1번)
            for sensor_id in [0, 1]:
                try:
                    pipeline = get_jetson_csi_pipeline(sensor_id, 640, 480, 30)  # 테스트용 낮은 해상도
                    cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)
                    if cap.isOpened():
                        available.append(('csi', sensor_id))
                        cap.release()
                        print(f"[Jetson] CSI 카메라 {sensor_id} 감지됨")
                except Exception as e:
                    print(f"[Jetson] CSI 카메라 {sensor_id} 체크 실패: {e}")
        
        # USB 카메라 체크
        for i in range(5):  # USB 카메라는 적당히 5개까지만 체크
            try:
                cap = cv2.VideoCapture(i)
                if cap.isOpened():
                    available.append(('usb', i))
                    cap.release()
            except:
                pass
        
        return available

# -------- 비디오 파일 소스 (테스트용 유지) --------
class VideoFileSource:
    def __init__(self, path: str):
        self.path = path
        self.cap = cv2.VideoCapture(path)
        if not self.cap.isOpened():
            raise RuntimeError(f"비디오를 열 수 없습니다: {path}")
        fps = self.cap.get(cv2.CAP_PROP_FPS)
        self.fps = fps if fps and not np.isnan(fps) else 30
        self.w  = int(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        self.h  = int(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT))

    def read(self):
        ok, frame = self.cap.read()
        return ok, frame

    def release(self):
        if self.cap: self.cap.release()

# ---------------- 메인 UI ----------------
class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("실시간 카메라 + 모델 추론/오버레이 + 이벤트 저장")
        self.resize(1280, 720)

        # 좌측 뷰
        self.view = QLabel("카메라를 연결해주세요")
        self.view.setAlignment(Qt.AlignCenter)
        self.view.setMinimumSize(QSize(960, 540))
        self.view.setScaledContents(False)              # 강제로 라벨 크기에 꽉 채우지 않음
        self.view.setStyleSheet("background-color: black;")  # 여백은 검정
        self._last_qimage = None                        # 최신 프레임 보관용

        # 하단 컨트롤
        self.camera_combo = QComboBox()
        self.camera_combo.setMinimumWidth(120)
        self.btn_connect = QPushButton("카메라 연결")
        self.btn_disconnect = QPushButton("연결 해제")
        self.btn_disconnect.setEnabled(False)
        self.btn_open = QPushButton("테스트 영상 열기")  # 테스트용 유지
        self.btn_trigger = QPushButton("수동 트리거")
        self.btn_past = QPushButton("과거 영상 보기")
        self.btn_model = QPushButton(f"모델: {USE_MODEL} (클릭 전환)")

        # 우측 이벤트 리스트
        self.event_list = QListWidget()

        left = QVBoxLayout()
        left.addWidget(self.view)
        
        # 카메라 컨트롤 행
        camera_row = QHBoxLayout()
        camera_row.addWidget(QLabel("카메라:"))
        camera_row.addWidget(self.camera_combo)
        camera_row.addWidget(self.btn_connect)
        camera_row.addWidget(self.btn_disconnect)
        camera_row.addStretch(1)
        left.addLayout(camera_row)
        
        # 기능 컨트롤 행
        control_row = QHBoxLayout()
        control_row.addWidget(self.btn_open)
        control_row.addWidget(self.btn_trigger)
        control_row.addWidget(self.btn_past)
        control_row.addWidget(self.btn_model)
        control_row.addStretch(1)
        left.addLayout(control_row)

        root = QHBoxLayout(self)
        root.addLayout(left, 3)
        root.addWidget(self.event_list, 1)

        # 시그널 연결
        self.btn_connect.clicked.connect(self.connect_camera)
        self.btn_disconnect.clicked.connect(self.disconnect_camera)
        self.btn_open.clicked.connect(self.open_video)
        self.btn_trigger.clicked.connect(self.manual_trigger)
        self.btn_past.clicked.connect(self.open_past)
        self.btn_model.clicked.connect(self.toggle_model)
        self.event_list.itemDoubleClicked.connect(self.play_selected)

        # 상태
        self.source = None  # CameraSource 또는 VideoFileSource
        self.rec: EventRecorder | None = None
        self.timer = QTimer(self); self.timer.timeout.connect(self.update_frame)
        self.is_camera_mode = False  # 카메라 모드 여부

        # 모델
        self.use_model = USE_MODEL
        self.model = None
        self.model_names = None
        self.load_current_model()
        
        # 카메라 초기화
        self.refresh_cameras()
        
        # 젯슨나노 환경 설정
        self.setup_jetson_optimizations()

        # === Flask 추가 ===
        global qt_window
        qt_window = self
        flask_thread = threading.Thread(target=run_flask, daemon=True)
        flask_thread.start()
        

    # ---- 모델 로드/전환 ----
    def load_current_model(self):
        self.model = load_model(self.use_model)
        names = self.model.names
        # names가 list 또는 dict 모두 대응
        if isinstance(names, dict):
            self.model_names = names
        else:
            # 리스트/튜플 등인 경우 dict로 변환
            self.model_names = {int(i): str(n) for i, n in enumerate(names)}
        
        # 커스텀 클래스명 매핑 (실제 모델 클래스에 맞게 수정)
        custom_names = {
            0: "사람(작업복)", # WO-01
            1: "사람(작업복 미착용)", # WO-02
            2: "화물트럭", # WO-03
            3: "지게차", # WO-04
            4: "핸드파레트카", # WO-05
            5: "롤테이너", # WO-06
            6: "운반수레", # WO-07
            7: "흡연", # WO-08
            8: "보관랙(선반)", # SO-01
            9: "적재물류(그룹)", # SO-02
            10: "물류(개별)", # SO-03
            11: "도크", # SO-06
            12: "출입문", # SO-07
            13: "화물승강기", # SO-08
            14: "차단멀티탭", # SO-09
            15: "멀티탭", # SO-10
            16: "개인 전열기구", # SO-11
            17: "소화기", # SO-12
            18: "작업 안전구역", # SO-13
            19: "용접 작업구역", # SO-14
            20: "지게차 이동영역", # SO-15
            21: "출입제한 구역", # SO-16
            22: "화재 대피로", # SO-17
            23: "안전펜스", # SO-18
            24: "화기(용접기, 토치)", # SO-19
            25: "이물질(물, 기름)", # SO-21
            26: "가연물, 인화물(목재, 섬유, 석유통)", # SO-22
            27: "샌드위치 판넬", # SO-23
            28: "지게차 운반 시 시야 미확보", # UA-01
            29: "랙 적재 시 주변 장애물 존재", # UA-02
            30: "3단 이상 평치 적재", # UA-03
            31: "랙 보관 적재상태 불량", # UA-04
            32: "운반장비 적재 시 개별물류 불안정", # UA-05
            33: "운반 중 화물 붕괴", # UA-06
            34: "지게차 이동 통로에 사람", # UA-10
            35: "안전수칙 미준수", # UA-12
            36: "운반 시 적재상태 불량/붕괴", # UA-13
            37: "외부 전용작업구역 내 사람", # UA-14
            38: "핸드파렛트카 2단 이상 적재", # UA-16
            39: "용접구역 내 가연물 침범", # UA-17
            40: "비흡연 구역 내 흡연", # UA-20
            41: "입고 시 화물칸에 사람", # UC-02
            42: "출고 시 화물칸에 사람", # UC-06
            43: "지게차 통로 표시 미흡", # UC-08
            44: "도크 출입문 앞 장애물", # UC-09
            45: "후진 접차 시 후방에 사람", # UC-10
            46: "빈 파렛트 미정돈", # UC-13
            47: "랙 안전선 내부 기대기", # UC-14
            48: "파렛트 비틀림/파손/부식", # UC-15
            49: "화물 승강기에 사람 탑승", # UC-16
            50: "과부하 차단기 없는 멀티탭", # UC-17
            51: "소화기 미비치", # UC-18
            52: "출입제한 구역문 개방", # UC-19
            53: "대피로 내 적재물", # UC-20
            54: "도크 분리 상태 하역", # UC-21
            55: "지게차 이동영역 이탈 주행" # UC-22
            # 필요에 따라 추가
}
        
        # 커스텀 매핑이 있는 클래스는 덮어쓰기
        for cls_id, custom_name in custom_names.items():
            if cls_id in self.model_names:
                self.model_names[cls_id] = custom_name
        
        print(f"[Model] loaded ({self.use_model}) -> classes: {self.model_names}")


    def toggle_model(self):
        self.use_model = "seg" if self.use_model == "det" else "det"
        self.btn_model.setText(f"모델: {self.use_model} (클릭 전환)")
        self.load_current_model()

    def setup_jetson_optimizations(self):
        """젯슨나노 환경 최적화 설정"""
        if not is_jetson_nano():
            return
        
        print("[Jetson] 젯슨나노 환경 감지 - 최적화 설정 적용")
        
        # 윈도우 타이틀에 젯슨 표시
        current_title = self.windowTitle()
        self.setWindowTitle(f"{current_title} - Jetson Nano")
        
        # 젯슨나노에서 CSI 카메라 우선 선택
        if self.camera_combo.count() > 0:
            # CSI 카메라가 있으면 자동 선택
            for i in range(self.camera_combo.count()):
                item_data = self.camera_combo.itemData(i)
                if item_data and item_data[0] == 'csi':
                    self.camera_combo.setCurrentIndex(i)
                    print(f"[Jetson] CSI 카메라 {item_data[1]} 자동 선택됨")
                    break
        
        # 성능 권장사항 출력
        print("[Jetson] 권장사항:")
        print("  - sudo jetson_clocks 실행으로 최대 성능 모드 활성화")
        print("  - sudo nvpmodel -m 0 실행으로 최대 전력 모드 설정")
        print("  - 필요시 스왑 메모리 확장 (4GB 이상 권장)")
        
        # GPU 메모리 확인
        try:
            import subprocess
            result = subprocess.run(['free', '-h'], capture_output=True, text=True)
            print(f"[Jetson] 메모리 상태:\n{result.stdout}")
        except:
            pass

    # ---- 카메라 관련 ----
    def refresh_cameras(self):
        """사용 가능한 카메라 목록 새로고침"""
        self.camera_combo.clear()
        available_cameras = CameraSource.get_available_cameras()
        
        if not available_cameras:
            self.camera_combo.addItem("카메라 없음", None)
            self.btn_connect.setEnabled(False)
        else:
            for camera_type, cam_id in available_cameras:
                if camera_type == 'csi':
                    self.camera_combo.addItem(f"CSI 카메라 {cam_id} (젯슨)", (camera_type, cam_id))
                else:  # usb
                    self.camera_combo.addItem(f"USB 카메라 {cam_id}", (camera_type, cam_id))
            self.btn_connect.setEnabled(True)

    def connect_camera(self):
        """카메라 연결"""
        camera_info = self.camera_combo.currentData()
        if camera_info is None:
            QMessageBox.warning(self, "경고", "사용 가능한 카메라가 없습니다.")
            return
        
        camera_type, camera_id = camera_info
        use_csi = (camera_type == 'csi')
        
        # 기존 소스 해제
        if self.source:
            self.timer.stop()
            self.source.release()
        
        try:
            self.source = CameraSource(camera_id, use_csi=use_csi)
            self.is_camera_mode = True
            
            # EventRecorder 초기화 (카메라는 30fps 고정)
            self.rec = EventRecorder(fps=30, pre_sec=PRE_SEC, post_sec=POST_SEC)
            self.rec.saved.connect(self._on_saved)
            
            # 타이머 시작 (실시간이므로 빠른 업데이트)
            self.timer.start(33)  # ~30fps
            
            # UI 상태 업데이트
            self.btn_connect.setEnabled(False)
            self.btn_disconnect.setEnabled(True)
            self.camera_combo.setEnabled(False)
            self.view.setText("")
            
            camera_name = f"CSI {camera_id}" if use_csi else f"USB {camera_id}"
            self.setWindowTitle(f"실시간 {camera_name} 카메라 모니터링")
            
        except Exception as e:
            QMessageBox.critical(self, "카메라 연결 오류", f"{camera_type.upper()} 카메라 {camera_id} 연결 실패:\n{str(e)}")
            self.source = None
            self.is_camera_mode = False

    def disconnect_camera(self):
        """카메라 연결 해제"""
        if self.source:
            self.timer.stop()
            self.source.release()
            self.source = None
            self.is_camera_mode = False
        
        # UI 상태 복원
        self.btn_connect.setEnabled(True)
        self.btn_disconnect.setEnabled(False)
        self.camera_combo.setEnabled(True)
        self.view.setText("카메라를 연결해주세요")
        self.view.setPixmap(QPixmap())  # 이미지 클리어
        
        self.setWindowTitle("실시간 카메라 + 모델 추론/오버레이 + 이벤트 저장")

    # ---- 동영상 열기 (테스트용) ----
    def open_video(self):
        path, _ = QFileDialog.getOpenFileName(self, "테스트 동영상 선택", str(Path.home()), "Video (*.mp4 *.avi *.mov *.mkv)")
        if not path: return
        
        # 기존 소스 해제
        if self.source:
            self.timer.stop()
            self.source.release()
        
        try:
            self.source = VideoFileSource(path)
            self.is_camera_mode = False
            
            self.rec = EventRecorder(fps=self.source.fps, pre_sec=PRE_SEC, post_sec=POST_SEC)
            self.rec.saved.connect(self._on_saved)

            interval_ms = max(10, int(1000 / max(1, int(self.source.fps))))
            self.timer.start(interval_ms)
            
            # UI 상태 업데이트
            self.btn_connect.setEnabled(False)
            self.btn_disconnect.setEnabled(False)
            self.camera_combo.setEnabled(False)
            self.view.setText("")
            
            self.setWindowTitle(f"테스트 재생 중: {Path(path).name}")
            
        except Exception as e:
            QMessageBox.critical(self, "오류", str(e))
            self.source = None
            self.is_camera_mode = False

    # ---- 매 프레임 루프 ----
    def update_frame(self):
        if not self.source:
            return
            
        ok, frame = self.source.read()
        if not ok:
            if self.is_camera_mode:
                # 카메라 모드에서는 연결이 끊어진 것으로 간주
                print("[Camera] 프레임 읽기 실패 - 카메라 연결 확인 필요")
                self.disconnect_camera()
                return
            else:
                # 파일 모드에서는 종료 처리 (루프 기능 제거)
                self.timer.stop()
                # UI 상태 복원
                self.btn_connect.setEnabled(True)
                self.camera_combo.setEnabled(True)
                self.view.setText("카메라를 연결하거나 테스트 영상을 열어주세요")
                self.setWindowTitle("실시간 카메라 + 모델 추론/오버레이 + 이벤트 저장")
                return

        # ===== 추론 =====
        results = self.model(frame, verbose=False)  # list[Results]
        res = results[0]

        # ===== 오버레이 & 트리거 =====
        trigger, trig_label = self.check_trigger_and_overlay(frame, res)

        if self.rec and trigger:
            h, w = frame.shape[:2]
            if self.rec.recording:
                self.rec.extend()
            else:
                self.rec.trigger((w, h), label=trig_label)

        if self.rec:
            self.rec.feed(frame)

        # 표시
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        h, w, ch = rgb.shape
        qimg = QImage(rgb.data, w, h, ch*w, QImage.Format_RGB888)
        self._last_qimage = qimg
        self._render_qimage_scaled()


    def _render_qimage_scaled(self):
        if self._last_qimage is None:
            return
        scaled = QPixmap.fromImage(self._last_qimage).scaled(
            self.view.size(),
            Qt.KeepAspectRatio,
            Qt.SmoothTransformation
        )
        self.view.setPixmap(scaled)

    def resizeEvent(self, e):
        self._render_qimage_scaled()
        return super().resizeEvent(e)


    # ---- 트리거 판단 + 오버레이 ----
    def check_trigger_and_overlay(self, frame, res):
        H, W = frame.shape[:2]
        trigger = False
        trig_label = "event"

        boxes = getattr(res, "boxes", None)
        names = self.model_names

        person_c, forklift_c = [], []

        # BBoxes
        if boxes is not None and boxes.xyxy is not None:
            xyxy = boxes.xyxy.cpu().numpy().astype(int)
            cls  = boxes.cls.cpu().numpy().astype(int)
            conf = boxes.conf.cpu().numpy()

            for i, (x1,y1,x2,y2) in enumerate(xyxy):
                c = cls[i]; score = conf[i]
                label = names.get(int(c), str(c))

                # bbox + 라벨
                cv2.rectangle(frame, (x1,y1), (x2,y2), (0,255,0), 2)
                
                # 한글 텍스트 표시를 위해 PIL 사용
                text = f"{label}:{score:.2f}"
                frame_pil = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
                draw = ImageDraw.Draw(frame_pil)
                try:
                    # 시스템 한글 폰트 시도
                    font = ImageFont.truetype("/System/Library/Fonts/AppleSDGothicNeo.ttc", 20)
                except:
                    font = ImageFont.load_default()
                
                draw.text((x1, max(20,y1-25)), text, fill=(0,255,0), font=font)
                frame[:] = cv2.cvtColor(np.array(frame_pil), cv2.COLOR_RGB2BGR)

                # 위험 라벨 직접 트리거
                if label in TRIGGER_LABELS:
                    trigger = True
                    trig_label = label

                # 근접 위험 계산 준비
                cx, cy = (x1+x2)//2, (y1+y2)//2
                if label in PERSON_LABELS:
                    person_c.append((cx, cy))
                elif label == FORKLIFT_LABEL:
                    forklift_c.append((cx, cy))

        # Segmentation masks (선택)
        masks = getattr(res, "masks", None)
        if masks is not None and masks.data is not None:
            mdata = masks.data.cpu().numpy()  # (N, Hm, Wm)
            overlay = frame.copy()
            for m in mdata:
                m = cv2.resize(m, (W, H), interpolation=cv2.INTER_NEAREST)
                color = (0, 0, 255)  # 빨강
                overlay[m > 0.5] = (overlay[m > 0.5] * 0.5 + np.array(color)*0.5).astype(np.uint8)
            frame[:] = overlay

        # 사람-지게차 근접 위험
        if person_c and forklift_c:
            min_d2 = 1e12
            for (px,py) in person_c:
                for (fx,fy) in forklift_c:
                    d2 = (px-fx)**2 + (py-fy)**2
                    if d2 < min_d2: min_d2 = d2
            if min_d2 < (PIXEL_DIST_THRESH**2):
                trigger = True
                trig_label = "collision"
                cv2.putText(frame, "COLLISION RISK!", (20, H-20),
                            cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0,0,255), 3)

        return trigger, trig_label

    # ---- 수동 트리거 ----
    def manual_trigger(self):
        if not self.source or not self.rec:
            msg = "먼저 카메라를 연결하거나 테스트 동영상을 여세요."
            QMessageBox.information(self, "안내", msg)
            return
        self.rec.trigger((self.source.w, self.source.h), label="manual")

    # 저장 완료 → 리스트 추가
    def _on_saved(self, path: str):
        if path:
            self.event_list.addItem(QListWidgetItem(path))

    # 리스트 더블클릭 → 재생
    def play_selected(self, item: QListWidgetItem):
        path = item.text()
        if not Path(path).exists():
            QMessageBox.warning(self, "경고", "파일이 존재하지 않습니다.")
            return
        dlg = PlayerDialog(path, self)
        dlg.show()

    # 과거 보기
    def open_past(self):
        start_dir = str(EVENT_DIR)
        path, _ = QFileDialog.getOpenFileName(self, "과거 영상 열기", start_dir, "Video (*.mp4 *.avi *.mov *.mkv)")
        if path:
            dlg = PlayerDialog(path, self)
            dlg.show()

    def closeEvent(self, e):
        self.timer.stop()
        if self.source:
            self.source.release()
        super().closeEvent(e)

# ---------------- 엔트리포인트 ----------------
if __name__ == "__main__":
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    sys.exit(app.exec())
