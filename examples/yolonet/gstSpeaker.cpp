#include "gstSpeaker.h"

#include <iostream>
#include <sstream>

gstSpeaker::gstSpeaker() : mPipeline(nullptr), mBus(nullptr) {
    if (!gstreamerInit()) {
        throw std::runtime_error("GStreamer initialization failed");
    }
}

gstSpeaker::~gstSpeaker() {
    cleanup();
}


bool gstSpeaker::play_mp3(const std::string& filename, const std::string& audio_device) {
    // 파이프라인 문자열 생성
    std::ostringstream pipeline_str;
    pipeline_str << "filesrc location=" << filename
                << " ! decodebin ! audioconvert ! audioresample ! volume volume=2.0 ! "
                << "alsasink device=" << audio_device;

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
    return true;
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