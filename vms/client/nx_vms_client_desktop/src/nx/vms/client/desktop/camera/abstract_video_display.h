// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <nx/media/media_data_packet.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop
{

class AbstractVideoDisplay
{
public:
    virtual ~AbstractVideoDisplay() = default;

    typedef nx::Uuid CameraID;
    virtual CameraID getCameraID() const = 0;

    virtual bool isFullScreen() const = 0;
    virtual bool isZoomWindow() const = 0;
    virtual bool isFisheyeEnabled() const = 0;
    virtual float getSpeed() const = 0;
    virtual bool isRadassSupported() const = 0;
    /** @returns largest width and heights if there are several renderers for one video stream. */
    virtual QSize getMaxScreenSize() const = 0;
    virtual QSize getVideoSize() const = 0;
    virtual QString getName() const = 0;

    virtual MediaQuality getQuality() const = 0;
    virtual void setQuality(MediaQuality quality, bool fastSwitch) = 0;

    virtual bool isRealTimeSource() const = 0;

    virtual int dataQueueSize() const = 0;
    virtual int maxDataQueueSize() const = 0;

    virtual bool isPaused() const = 0;
    virtual bool isMediaPaused() const = 0;

    virtual bool isBuffering() const = 0;

// Cannot derive from QObject so cannot use signals.
    virtual void setCallbackForStreamChanges(std::function<void()> callback) = 0;
};

} // namespace vms::client::desktop
