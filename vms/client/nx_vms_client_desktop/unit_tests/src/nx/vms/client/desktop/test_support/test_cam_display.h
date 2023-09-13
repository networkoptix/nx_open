// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/camera/abstract_video_display.h>

namespace nx::vms::client::desktop::test {

class TestCamDisplay: public AbstractVideoDisplay
{
public:
    TestCamDisplay(const QString& name = {}, CameraID cameraID = {});

    virtual CameraID getCameraID() const override { return cameraID; }

    virtual bool isFullScreen() const override { return fullScreen; }
    virtual bool isZoomWindow() const override { return zoomWindow; }
    virtual bool isFisheyeEnabled() const override { return fisheye; }
    virtual float getSpeed() const override { return speed; }
    virtual bool isRadassSupported() const override { return radassSupported; }
    virtual QSize getMaxScreenSize() const override { return maxScreenSize; }
    virtual QSize getVideoSize() const override { return videoSize; }
    virtual QString getName() const override { return name; }

    virtual MediaQuality getQuality() const override { return qualityValue; }
    virtual void setQuality(MediaQuality quality, bool fastSwitch) override;

    virtual bool isRealTimeSource() const override { return realtimeSource; }

    virtual int dataQueueSize() const override { return dataQueueSizeValue; }
    virtual int maxDataQueueSize() const override { return maxDataQueueSizeValue; }

    virtual bool isPaused() const override { return paused; }
    virtual bool isMediaPaused() const override { return mediaPaused; }

    virtual bool isBuffering() const override { return buffering; }

    virtual void setCallbackForStreamChanges(std::function<void()> callback) override;

// Data members to access from test environment.
public:
    QString name;
    CameraID cameraID;
    bool fullScreen = false;
    bool zoomWindow = false;
    bool fisheye = false;
    float speed = 1.0f;
    bool radassSupported = true;
    QSize maxScreenSize = { 1920, 1080};
    QSize videoSize = { 1920, 1080 };
    MediaQuality qualityValue = MEDIA_Quality_High;
    bool fastSwitchValue = false;
    bool realtimeSource = true;
    int dataQueueSizeValue = 0;
    int maxDataQueueSizeValue = 0;
    bool paused = false;
    bool mediaPaused = false;
    bool buffering = false;
    void executeStreamChangedCallback();

private:
    std::function<void()> m_streamCallback;
};

using TestCamDisplayPtr = QSharedPointer<TestCamDisplay>;

} // namespace nx::vms::client::desktop::test
