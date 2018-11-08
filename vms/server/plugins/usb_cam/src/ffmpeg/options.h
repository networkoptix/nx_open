#pragma once

#include <stdint.h>

struct AVDictionary;
struct AVDictionaryEntry;

namespace nx {
namespace usb_cam {
namespace ffmpeg {

class Options
{
public:
    Options() = default;
    ~Options();
    int setEntry(const char * key, const char * value, int flags = 0);
    int setEntry(const char * key, int64_t value, int flags = 0);
    AVDictionaryEntry* getEntry(
        const char * key,
        const AVDictionaryEntry * prev = nullptr,
        int flags = 0) const;
    int count() const;

protected:
    AVDictionary * m_options;
};


} // namespace ffmpeg
} // namespace usb_cam
} // namespace nx