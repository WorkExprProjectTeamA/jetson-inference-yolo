#ifndef __ALSA_SPEAKER_H__
#define __ALSA_SPEAKER_H__

#include "gstUtility.h"

#include <string>


class gstSpeaker {
public:
    gstSpeaker();
    ~gstSpeaker();

    bool play_mp3(const std::string& filename, const std::string& audio_device = "hw:2,0");

private:
    void wait_for_completion();
    void cleanup();

    GstElement* mPipeline;
    GstBus* mBus;
};

#endif