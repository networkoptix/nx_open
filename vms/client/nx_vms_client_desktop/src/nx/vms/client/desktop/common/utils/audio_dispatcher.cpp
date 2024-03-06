// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_dispatcher.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtQml/QtQml>

#include <nx/audio/audiodevice.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

struct AudioDispatcher::Private
{
    AudioDispatcher* const q;
    QSet<QWindow*> audioSources;
    QPointer<QWindow> primaryAudioSource;
    QPointer<QWindow> currentAudioSource;
    std::atomic_bool audioInitialized = false;
    bool audioEnabled = false;

    void initializeAudio();
    bool updateCurrentAudioSource();
    QWindow* calculateCurrentAudioSource() const;
};

// -----------------------------------------------------------------------------------------------
// AudioDispatcher

AudioDispatcher::AudioDispatcher(QObject* parent):
    QObject(parent),
    d(new Private{.q = this})
{
    connect(qGuiApp, &QGuiApplication::focusWindowChanged, this,
        [this]()
        {
            if (d->updateCurrentAudioSource())
                emit currentAudioSourceChanged();
        });
}

AudioDispatcher::~AudioDispatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

QWindow* AudioDispatcher::currentAudioSource() const
{
    return d->currentAudioSource;
}

void AudioDispatcher::requestAudio(QWindow* window)
{
    if (d->audioSources.contains(window) || !NX_ASSERT(window))
        return;

    d->initializeAudio();
    d->audioSources.insert(window);

    connect(window, &QObject::destroyed, this,
        [this, window]()
        {
            d->audioSources.remove(window);

            if (d->currentAudioSource == window && d->updateCurrentAudioSource())
                emit currentAudioSourceChanged();
        });

    if (d->updateCurrentAudioSource())
        emit currentAudioSourceChanged();
}

void AudioDispatcher::releaseAudio(QWindow* window)
{
    if (!NX_ASSERT(window) || !d->audioSources.contains(window))
        return;

    d->audioSources.remove(window);
    window->disconnect(this);

    if (d->currentAudioSource == window && d->updateCurrentAudioSource())
        emit currentAudioSourceChanged();
}

QWindow* AudioDispatcher::primaryAudioSource() const
{
    return d->primaryAudioSource;
}

void AudioDispatcher::setPrimaryAudioSource(QWindow* value)
{
    if (d->primaryAudioSource == value)
        return;

    d->primaryAudioSource = value;

    if (d->updateCurrentAudioSource())
        emit currentAudioSourceChanged();
}

bool AudioDispatcher::audioEnabled() const
{
    return d->audioEnabled;
}

AudioDispatcher* AudioDispatcher::instance()
{
    static AudioDispatcher audioDispatcher;
    QQmlEngine::setObjectOwnership(&audioDispatcher, QQmlEngine::CppOwnership);
    return &audioDispatcher;
}

void AudioDispatcher::registerQmlType()
{
    qmlRegisterSingletonInstance<AudioDispatcher>(
        "nx.vms.client.desktop", 1, 0, "AudioDispatcher", instance());
}

// -----------------------------------------------------------------------------------------------
// AudioDispatcher::Private

void AudioDispatcher::Private::initializeAudio()
{
    if (audioInitialized.exchange(true))
        return;

    using nx::audio::AudioDevice;

    audioEnabled = !AudioDevice::instance()->isMute();

    QObject::connect(AudioDevice::instance(), &AudioDevice::volumeChanged, q,
        [this]()
        {
            const bool isAudioEnabled = !AudioDevice::instance()->isMute();
            if (isAudioEnabled == audioEnabled)
                return;

            audioEnabled = isAudioEnabled;
            emit q->audioEnabledChanged();
        });
}

bool AudioDispatcher::Private::updateCurrentAudioSource()
{
    const auto source = calculateCurrentAudioSource();
    if (source == currentAudioSource)
        return false;

    NX_VERBOSE(this, "Current audio source is changed to %1", source);
    currentAudioSource = source;

    return true;
}

QWindow* AudioDispatcher::Private::calculateCurrentAudioSource() const
{
    for (auto window = QGuiApplication::focusWindow();
        window != nullptr;
        window = window->parent(QWindow::AncestorMode::IncludeTransients))
    {
        if (audioSources.contains(window))
            return window;
    }

    return primaryAudioSource && audioSources.contains(primaryAudioSource)
        ? primaryAudioSource
        : nullptr;
}

} // namespace nx::vms::client::desktop
