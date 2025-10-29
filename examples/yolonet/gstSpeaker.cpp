#include "gstSpeaker.h"

#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>


gstSpeaker::gstSpeaker(const std::string& audio_path)
    : mPipeline(nullptr), mBus(nullptr), mAudioDevice("hw:0,0"), mAudioPath(audio_path), mIsPlaying(false) {
    if (!gstreamerInit()) {
        throw std::runtime_error("GStreamer initialization failed");
    }
}

gstSpeaker::~gstSpeaker() {
    cleanup();
}


bool gstSpeaker::play_class_audio(int class_number) {
    // Check 10-second cooldown
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - mLastPlayTime);

    if (elapsed.count() < 10) {
        std::cout << LOG_GSTREAMER "Cooldown active. " << (10 - elapsed.count()) << " seconds remaining." << std::endl;
        return false;
    }

    // Generate filename from class number
    std::string filename = mAudioPath + std::to_string(class_number) + ".mp3";
    mLastWarning = filename;

    // 파이프라인 문자열 생성
    std::ostringstream pipeline_str;
    pipeline_str << "filesrc location=" << filename
                << " ! decodebin ! audioconvert ! audioresample ! volume volume=4.0 ! "
                << "alsasink device=" << mAudioDevice;

    // GStreamer 파이프라인 생성
    GError* error = nullptr;
    mPipeline = gst_parse_launch(pipeline_str.str().c_str(), &error);

    if (!mPipeline || error) {
        std::cerr << LOG_GSTREAMER "Failed to create pipeline: "
                << (error ? error->message : "Unknown error") << std::endl;
        if (error) g_error_free(error);
        return false;
    }

    // 버스 설정 - jetson-inference의 메시지 핸들러 활용
    mBus = gst_element_get_bus(mPipeline);
    gst_bus_add_watch(mBus, gst_message_print, nullptr);

    // 재생 시작
    GstStateChangeReturn ret = gst_element_set_state(mPipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << LOG_GSTREAMER "Failed to start playing" << std::endl;
        return false;
    }

    // 재생 완료 대기
    wait_for_completion();

    // 재생 완료 후 시간 업데이트
    mLastPlayTime = std::chrono::system_clock::now();

    return true;
}

bool gstSpeaker::play_warning_sequence(int class_number) {
    // Generate filenames
    std::string warning_file = mAudioPath + "warning.mp3";
    std::string class_file = mAudioPath + std::to_string(class_number) + ".mp3";
    mLastWarning = class_file;

    // 순차 재생: warning.mp3 먼저 재생
    std::ostringstream pipeline_str;
    pipeline_str << "filesrc location=" << warning_file << " ! decodebin ! audioconvert ! audioresample ! volume volume=2.0 ! alsasink device=" << mAudioDevice;

    // GStreamer 파이프라인 생성 (warning 파일)
    GError* error = nullptr;
    mPipeline = gst_parse_launch(pipeline_str.str().c_str(), &error);

    if (!mPipeline || error) {
        std::cerr << LOG_GSTREAMER "Failed to create pipeline for warning: "
                  << (error ? error->message : "Unknown error") << std::endl;
        if (error) g_error_free(error);
        return false;
    }

    // 버스 설정
    mBus = gst_element_get_bus(mPipeline);
    gst_bus_add_watch(mBus, gst_message_print, nullptr);

    // warning.mp3 재생 시작
    GstStateChangeReturn ret = gst_element_set_state(mPipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << LOG_GSTREAMER "Failed to start playing warning" << std::endl;
        return false;
    }

    // warning.mp3 재생 완료 대기
    wait_for_completion();

    // 파이프라인 정리
    gst_element_set_state(mPipeline, GST_STATE_NULL);

    // 상태 변경 완료 대기
    GstStateChangeReturn state_ret = gst_element_get_state(mPipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << LOG_GSTREAMER "Failed to change state to NULL" << std::endl;
    }

    gst_object_unref(mPipeline);
    gst_object_unref(mBus);

    // 짧은 지연으로 오디오 디바이스 해제 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // class 파일 재생
    pipeline_str.str("");
    pipeline_str.clear();
    pipeline_str << "filesrc location=" << class_file << " ! decodebin ! audioconvert ! audioresample ! volume volume=2.0 ! alsasink device=" << mAudioDevice;

    // GStreamer 파이프라인 생성 (class 파일)
    error = nullptr;
    mPipeline = gst_parse_launch(pipeline_str.str().c_str(), &error);

    if (!mPipeline || error) {
        std::cerr << LOG_GSTREAMER "Failed to create pipeline for class: "
                  << (error ? error->message : "Unknown error") << std::endl;
        if (error) g_error_free(error);
        return false;
    }

    // 버스 설정
    mBus = gst_element_get_bus(mPipeline);
    gst_bus_add_watch(mBus, gst_message_print, nullptr);

    // class 파일 재생 시작
    ret = gst_element_set_state(mPipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << LOG_GSTREAMER "Failed to start playing class file" << std::endl;
        return false;
    }

    // class 파일 재생 완료 대기
    wait_for_completion();

    // 두 번째 파이프라인도 정리
    gst_element_set_state(mPipeline, GST_STATE_NULL);

    // 상태 변경 완료 대기
    state_ret = gst_element_get_state(mPipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << LOG_GSTREAMER "Failed to change class pipeline state to NULL" << std::endl;
    }

    gst_object_unref(mPipeline);
    gst_object_unref(mBus);

    // 재생 완료 후 시간 업데이트
    mLastPlayTime = std::chrono::system_clock::now();

    return true;
}

std::future<bool> gstSpeaker::play_class_audio_async(int class_number) {
    return std::async(std::launch::async, [this, class_number]() {
        return this->play_class_audio(class_number);
    });
}

std::future<bool> gstSpeaker::play_warning_sequence_async(int class_number) {
    // 이미 재생 중이면 바로 false 반환
    if (mIsPlaying.load()) {
        return std::async(std::launch::deferred, []() { return false; });
    }

    // Check 10-second cooldown
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - mLastPlayTime);

    if (elapsed.count() < 3) {
        std::cout << LOG_GSTREAMER "Cooldown active. " << (10 - elapsed.count()) << " seconds remaining." << std::endl;
        return std::async(std::launch::deferred, []() { return false; });;
    }
    

    return std::async(std::launch::async, [this, class_number]() {
        mIsPlaying.store(true);
        bool result = this->play_warning_sequence(class_number);
        mIsPlaying.store(false);
        return result;
    });
}

void gstSpeaker::wait_for_completion() {
    GstMessage* msg;

    // EOS 또는 에러 대기
    msg = gst_bus_timed_pop_filtered(mBus, GST_CLOCK_TIME_NONE, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg) {
        // jetson-inference의 메시지 처리 활용
        gst_message_print(mBus, msg, nullptr);
        gst_message_unref(msg);
    }
}

void gstSpeaker::cleanup() {
    if (mPipeline) {
        gst_element_set_state(mPipeline, GST_STATE_NULL);
        gst_object_unref(mPipeline);
        mPipeline = nullptr;
    }

    if (mBus) {
        gst_object_unref(mBus);
        mBus = nullptr;
    }
}