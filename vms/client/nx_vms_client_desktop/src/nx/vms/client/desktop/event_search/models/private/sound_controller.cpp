// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sound_controller.h"

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/file_cache/server_notification_cache.h>
#include <nx/vms/client/desktop/window_context.h>
#include <utils/media/audio_player.h>

namespace nx::vms::client::desktop {

namespace {

void deletePlayer(AudioPlayer* player)
{
    // Due to AudioPlayer strange architecture simple calling pleaseStop doesn't work well:
    // it makes the calling thread wait until current playback finishes playing.

    if (player->isRunning())
        QObject::connect(player, &AudioPlayer::done, player, &AudioPlayer::deleteLater);
    else
        player->deleteLater();
}

QSharedPointer<AudioPlayer> loopSound(const QString& filePath)
{
    auto player = std::make_unique<AudioPlayer>();
    if (!player->open(filePath))
        return {};

    const auto restart =
        [filePath, player = player.get()]()
        {
            player->close();
            if (player->open(filePath))
                player->playAsync();
        };

    const auto loopConnection = QObject::connect(player.get(), &AudioPlayer::done,
        player.get(), restart, Qt::QueuedConnection);

    if (!player->playAsync())
        return {};

    return QSharedPointer<AudioPlayer>(player.release(),
        [loopConnection](AudioPlayer* player)
        {
            QObject::disconnect(loopConnection);
            deletePlayer(player);
        });
}

} // namespace

SoundController::SoundController(WindowContextAware* windowContextAware):
    WindowContextAware(windowContextAware)
{
    connect(system()->serverNotificationCache(),
        &ServerFileCache::fileDownloaded, this,
        [this](
            const QString& soundUrl,
            ServerFileCache::OperationResult status,
            const QString& absolutePath)
        {
            const bool wasRequestedAsSingleShot = m_soundsToLoad.remove(soundUrl);

            if (status == ServerFileCache::OperationResult::ok)
            {
                if (wasRequestedAsSingleShot)
                {
                    m_loadedSounds.insert(soundUrl);
                    createSingleShotPlayer(absolutePath);
                }

                for (const auto& id: m_eventsByLoadingSound.values(soundUrl))
                    m_loopedPlayers[id] = loopSound(absolutePath);
            }

            m_eventsByLoadingSound.remove(soundUrl);
        });
}

SoundController::~SoundController()
{
}

void SoundController::play(const QString& soundUrl)
{
    if (m_loadedSounds.contains(soundUrl))
    {
        const auto path = system()->serverNotificationCache()->absoluteFilePath(soundUrl);
        if (NX_ASSERT(!path.isEmpty()))
            createSingleShotPlayer(path);
        return;
    }

    m_soundsToLoad.insert(soundUrl);
    system()->serverNotificationCache()->downloadFile(soundUrl);
}

void SoundController::addLoopedPlay(const QString& soundUrl, const Uuid& eventId)
{
    if (!m_eventsByLoadingSound.contains(soundUrl))
        system()->serverNotificationCache()->downloadFile(soundUrl);

    m_eventsByLoadingSound.insert(soundUrl, eventId);
}

void SoundController::removeLoopedPlay(const Uuid& eventId)
{
    m_loopedPlayers.remove(eventId);

    for (auto it = m_eventsByLoadingSound.begin(); it != m_eventsByLoadingSound.end(); ++it)
    {
        if (it.value() == eventId)
        {
            m_eventsByLoadingSound.erase(it);
            break;
        }
    }
}

void SoundController::resetLoopedPlayers()
{
    m_eventsByLoadingSound.clear();
    m_loopedPlayers.clear();
}

void SoundController::createSingleShotPlayer(const QString& soundUrl)
{
    auto player = std::make_unique<AudioPlayer>();
    if (!player->open(soundUrl))
        return;

    auto playerRawPtr = player.get();

    m_singleShotPlayers[playerRawPtr] = QSharedPointer<AudioPlayer>(player.release(),
        [](AudioPlayer* player)
        {
            deletePlayer(player);
        });

    connect(playerRawPtr, &AudioPlayer::done, this,
        [this, playerRawPtr]
        {
            m_singleShotPlayers.remove(playerRawPtr);
        });

    if (!playerRawPtr->playAsync())
        m_singleShotPlayers.remove(playerRawPtr);
}

} // namespace nx::vms::client::desktop
