// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QMultiHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>

#include <nx/vms/client/desktop/window_context_aware.h>
#include <nx/utils/uuid.h>

class AudioPlayer;

namespace nx::vms::client::desktop {

class SoundController:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT

public:
    explicit SoundController(WindowContextAware* windowContextAware);
    virtual ~SoundController() override;

    void play(const QString& soundUrl);

    void addLoopedPlay(const QString& soundUrl, const Uuid& eventId);
    void removeLoopedPlay(const Uuid& eventId);
    void resetLoopedPlayers();

private:
    void createSingleShotPlayer(const QString& filePath);

private:
    QSet<QString> m_soundsToLoad;
    QSet<QString> m_loadedSounds;

    QMultiHash<QString, nx::Uuid> m_eventsByLoadingSound;
    QHash<nx::Uuid /*event id*/, QSharedPointer<AudioPlayer>> m_loopedPlayers;
    QHash<AudioPlayer*, QSharedPointer<AudioPlayer>> m_singleShotPlayers;
};

} // namespace nx::vms::client::desktop
