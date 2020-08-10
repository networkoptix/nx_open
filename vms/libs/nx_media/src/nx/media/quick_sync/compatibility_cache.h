#pragma once

#include <mutex>
#include <map>

#include <QSize>

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

class CompatibilityCache
{
public:
    static bool isCompatible(AVCodecID codec, int width, int height)
    {
        static CompatibilityCache table;
        return table.isCompatibleCached(codec, width, height);
    }

private:
    bool isCompatibleCached(AVCodecID codec, int width, int height);
    bool tryCompatibility(AVCodecID codec, int width, int height);

private:
    std::mutex m_mutex;
    std::map<AVCodecID, QSize> m_compatibleSize;
    std::map<AVCodecID, QSize> m_uncompatibleSize;
};
