#pragma once

#include <QtCore/QMetaType>

namespace nx {
namespace client {
namespace desktop {

struct CameraExtraStatus
{
    bool recording = false;
    bool scheduled = false;
    bool buggy =  false;

    CameraExtraStatus& operator|=(const CameraExtraStatus& other)
    {
        recording |= other.recording;
        scheduled |= other.scheduled;
        buggy |= other.buggy;
        return *this;
    }

    bool isEmpty() const
    {
        return !recording && !scheduled && !buggy;
    }
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::CameraExtraStatus)
