// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <plugins/resource/desktop_camera/desktop_data_provider_base.h>
#include <ui/screen_recording/video_recorder_settings.h>

namespace nx::vms::client::desktop { class AudioDeviceChangeNotifier; }

class QnAudioDeviceInfo;

class QnDesktopDataProvider: public QnDesktopDataProviderBase
{
    Q_OBJECT

public:
    QnDesktopDataProvider(
        const QnResourcePtr& res,
        int desktopNum, // = 0,
        const QnAudioDeviceInfo* audioDevice,
        const QnAudioDeviceInfo* audioDevice2,
        Qn::CaptureMode mode,
        bool captureCursor,
        const QSize& captureResolution,
        float encodeQualuty, // in range 0.0 .. 1.0
        QWidget* glWidget, // used in application capture mode only
        const QPixmap& logo // logo over video
    );
    virtual ~QnDesktopDataProvider();

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
