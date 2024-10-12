// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

struct AVDictionary;

namespace nx::media::ffmpeg {

class NX_MEDIA_CORE_API AvOptions
{
public:
    AvOptions() = default;
    AvOptions(const AvOptions&) = delete;
    ~AvOptions();
    void set(const char* key, const char* value, int flags = 0);
    void set(const char* key, int64_t value, int flags);
    AVDictionary* get() { return m_options; }
    operator AVDictionary**() { return &m_options; }


private:
    AVDictionary* m_options = nullptr;
};

} // namespace nx::media::ffmpeg
