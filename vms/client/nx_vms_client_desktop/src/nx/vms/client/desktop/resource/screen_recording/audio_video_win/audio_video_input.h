// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_data_provider_base.h>

class QOpenGLWidget;

namespace nx::vms::client::desktop {

class AudioVideoInput
{
public:
    AudioVideoInput(QOpenGLWidget* mainWidget);
    std::unique_ptr<QnAbstractStreamDataProvider> createAudioInputProvider() const;

private:
    void createSharedDataProvider() const;

private:
    QOpenGLWidget* const m_mainWidget;
    mutable nx::Mutex m_mutex;
    mutable std::unique_ptr<nx::vms::client::core::DesktopDataProviderBase> m_provider;
};

} // namespace nx::vms::client::desktop
