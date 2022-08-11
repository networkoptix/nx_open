// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSettings>

#include <nx/vms/client/core/resource/screen_recording/audio_recorder_settings.h>

namespace Qn {

enum CaptureMode
{
    FullScreenMode,
    FullScreenNoAeroMode,
    WindowMode
};

enum DecoderQuality
{
    BestQuality,
    BalancedQuality,
    PerformanceQuality
};

enum Resolution
{
    NativeResolution,
    QuaterNativeResolution,
    Exact1920x1080Resolution,
    Exact1280x720Resolution,
    Exact640x480Resolution,
    Exact320x240Resolution
};

} // namespace Qn

Q_DECLARE_METATYPE(Qn::CaptureMode)
Q_DECLARE_METATYPE(Qn::DecoderQuality)
Q_DECLARE_METATYPE(Qn::Resolution)

namespace nx::vms::client::desktop {

class VideoRecorderSettings: public core::AudioRecorderSettings
{
    Q_OBJECT
    using base_type = core::AudioRecorderSettings;

public:
    explicit VideoRecorderSettings(QObject* parent = nullptr);
    virtual ~VideoRecorderSettings() override;

    bool captureCursor() const;
    void setCaptureCursor(bool yes);

    Qn::CaptureMode captureMode() const;
    void setCaptureMode(Qn::CaptureMode c);

    Qn::DecoderQuality decoderQuality() const;
    void setDecoderQuality(Qn::DecoderQuality q);

    Qn::Resolution resolution() const;
    void setResolution(Qn::Resolution r);

    int screen() const;
    void setScreen(int screen);

    static int screenToAdapter(int screen);
    static QSize resolutionToSize(Qn::Resolution resolution);
    static float qualityToNumeric(Qn::DecoderQuality quality);

private:
    QSettings settings;
};

} // namespace nx::vms::client::desktop
