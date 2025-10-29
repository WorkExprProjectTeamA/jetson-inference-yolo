#ifndef __ALSA_SPEAKER_H__
#define __ALSA_SPEAKER_H__

#include "gstUtility.h"

#include <string>
#include <vector>
#include <chrono>
#include <future>
#include <atomic>


class gstSpeaker {
public:
    gstSpeaker(const std::string& audio_path = "./audio/");
    ~gstSpeaker();

    bool play_class_audio(int class_number);
    bool play_warning_sequence(int class_number);

    std::future<bool> play_class_audio_async(int class_number);
    std::future<bool> play_warning_sequence_async(int class_number);

    // Getters for last played warning info
    const std::string& get_last_warning() const { return mLastWarning; }
    std::chrono::system_clock::time_point get_last_play_time() const { return mLastPlayTime; }

private:
    void wait_for_completion();
    void cleanup();

    GstElement* mPipeline;
    GstBus* mBus;

    // Warning tracking
    std::string mLastWarning;
    std::chrono::system_clock::time_point mLastPlayTime;

    // Audio settings
    std::string mAudioDevice;
    std::string mAudioPath;

    // Playing state
    std::atomic<bool> mIsPlaying;
};

#endif