// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

extern "C" {
#include <libavdevice/avdevice.h>
}

namespace nx::media::test {

TEST(ffmpeg, build_configuration)
{
#ifdef __linux__
    AVInputFormat* device = nullptr;
    bool alsaPresent = false;
    while ((device = av_input_audio_device_next(device)))
    {
        if (strcmp(device->name, "alsa") == 0)
            alsaPresent = true;
    }
    ASSERT_TRUE(alsaPresent);
#endif // __linux__
}


} // namespace nx::media::test
