# 📢 안전 관리 온디바이스 AI 시스템
실시간 위험 상황 감지가 가능한 온디바이스 AI 안전 관리 시스템

<img src="https://github.com/user-attachments/assets/9316ff1b-5e52-418b-ba72-6943961dc55f" width="720" height="405"/> 
<img src="https://github.com/user-attachments/assets/a81bb56b-60ba-4a10-b888-405068ff918b" width="720" height="405"/>

## ⚙️ Features
|기능|기능 상세|
|------|------------|
|위험 상황 감지 기능|시스템은 위험 상황을 실시간으로 감지한다.<br>위험 상황은 다음과 같다:<br>1. 파레트가 잘 쌓여있는지 (박스 적재 상태)<br>2. 지게차와 사람, 박스와 사람 간 거리가 위험 정도인지<br>3. 작업자가 쓰러졌는지|
|위험 상황 자동 알림 기능|위험 상황 발생 시, 사전에 설정된 방식에 따라 즉시 알림을 생성하고 전송해야 한다.<br>알림 방식 :<br>- 스피커/경광등을 통한 현장 알림<br>- 관리자용 웹 대시보드로 푸시 알림|
|시스템 상태 모니터링|- 정해진 시간마다 주기적으로 모니터링 데이터를 전송해 시스템의 정상 작동을 확인한다.<br>- 저장 공간 부족, 네트워크 연결 끊김 등 시스템 장애 발생 시 관리자에게 알림을 보낸다.<br>|
|위험 이벤트 기록|모든 위험 이벤트는 발생 시간, 카메라 위치, 위험 유형, 증거 사진과 함께 데이터베이스에 자동으로 기록되어야 한다.|

## 💠 Environments
### Hardware
|||
|------|------------|
|Jetson Orin Nano|<img width="100" height="100" alt="Image" src="https://github.com/user-attachments/assets/4a96a09d-80be-4e88-bb5d-e4dfeab84760" />|
|Arducam IMX519|<img width="100" height="100" alt="Image" src="https://github.com/user-attachments/assets/af22e744-5b67-40a8-9ffa-33e2d5aa2804" />|
|Adafruit Mini External USB Stereo Speaker [ada-3369]|<img width="100" height="100" alt="Image" src="https://github.com/user-attachments/assets/e01facfb-0d46-4b23-8f42-bbdf0f57fc91" />|

### Software
- JetPack 6.2.1
- CUDA 12.6
- TensorRT 10.3.0

## 🚀 Getting Started
```bash
git clone https://github.com/choibujang/jetson-inference-yolo.git
cd jetson-inference-yolo
mkdir build
cd build
cmake ..
make -j$(nproc)
cd aarch64/bin/
./yolonet images/test.jpg output.jpg
```
