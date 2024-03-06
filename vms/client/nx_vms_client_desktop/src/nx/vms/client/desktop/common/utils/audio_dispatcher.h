// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("QtGui/QWindow")

class QWindow;

namespace nx::vms::client::desktop {

/**
 * This singleton dispatches exclusive rights to use an audio device between several windows.
 * It only provides information and doesn't actually control audio playback.
 * The rights are given to the active subscribed window. If neither of them is active,
 * the rights are given to the primary audio source, if set.
 */
class AudioDispatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QWindow* currentAudioSource READ currentAudioSource
        NOTIFY currentAudioSourceChanged)
    Q_PROPERTY(bool audioEnabled READ audioEnabled NOTIFY audioEnabledChanged)

    explicit AudioDispatcher(QObject* parent = nullptr);

public:
    static AudioDispatcher* instance();
    virtual ~AudioDispatcher() override;

    /** The window that currently possesses the rights to play audio, or null. */
    QWindow* currentAudioSource() const;

    /** Indicate that the specified window intends to play audio. */
    Q_INVOKABLE void requestAudio(QWindow* window);

    /** Indicate that the specified window does no longer intend to play audio. */
    Q_INVOKABLE void releaseAudio(QWindow* window);

    /**
     * Specify a window that will acquire audio playback rights in case that none of windows
     * that requested audio playback are active (but only if it requested audio playback as well).
     */
    void setPrimaryAudioSource(QWindow* value);
    QWindow* primaryAudioSource() const;

    /** Whether audio playback is enabled on the global audio device. */
    bool audioEnabled() const;

    static void registerQmlType();

signals:
    void currentAudioSourceChanged();
    void audioEnabledChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
