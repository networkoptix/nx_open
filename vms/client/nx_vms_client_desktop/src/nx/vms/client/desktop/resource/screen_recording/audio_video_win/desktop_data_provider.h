// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_data_provider_base.h>

#include "../video_recorder_settings.h"

namespace nx::vms::client::desktop {

class AudioDeviceChangeNotifier;

class DesktopDataProvider: public core::DesktopDataProviderBase
{
    Q_OBJECT

public:
    DesktopDataProvider(
        const QnResourcePtr& res,
        int desktopNum, // = 0,
        const core::AudioDeviceInfo* audioDevice,
        const core::AudioDeviceInfo* audioDevice2,
        Qn::CaptureMode mode,
        bool captureCursor,
        const QSize& captureResolution,
        float encodeQualuty, // in range 0.0 .. 1.0
        QWidget* glWidget, // used in application capture mode only
        const QPixmap& logo // logo over video
    );
    virtual ~DesktopDataProvider();

    virtual void start(Priority priority = InheritPriority) override;

    virtual void beforeDestroyDataProvider(QnAbstractMediaDataReceptor* dataProviderWrapper) override;
    void addDataProcessor(QnAbstractMediaDataReceptor* consumer);

    virtual bool isInitialized() const override;

    virtual AudioLayoutConstPtr getAudioLayout() override;

    bool readyToStop() const;

    qint64 currentTime() const;

    int frameSize() const;

protected:
    virtual void run() override;

private:
    bool initVideoCapturing();
    bool initAudioCapturing();
    virtual void closeStream();

    int calculateBitrate(const char* codecName = nullptr);
    int processData(bool flush);
    void putAudioData();
    void stopCapturing();
    bool needVideoData() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
