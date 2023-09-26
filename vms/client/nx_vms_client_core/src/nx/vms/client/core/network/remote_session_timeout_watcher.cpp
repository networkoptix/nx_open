// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_session_timeout_watcher.h"

#include <QtCore/QTimer>

#include <core/resource/user_resource.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/synctime.h>

#include "remote_connection.h"
#include "remote_session.h"

using namespace std::chrono;
using namespace nx::vms::common;

namespace nx::vms::client::core {

namespace {

/** One minute precision is quite enough for session control. */
constexpr auto kTimerInterval = 1min;

/** First notification will be displayed a hour before session end. */
constexpr auto kFirstNotificationTime = 1h;

/** User will be forcefully disconnected when token estimated time is less than 3 seconds. */
constexpr auto kForceDisconnectTime = 3s;

/**
 * Second (and last) notification will be displayed 10 minutes before session end even if first one
 * was cancelled.
 */
constexpr auto kLastNotificationTime = 10min;

} // namespace

struct RemoteSessionTimeoutWatcher::Private
{
    std::weak_ptr<RemoteSession> session;
    std::optional<std::chrono::seconds> passwordSessionTimeoutLimit;
    std::optional<std::chrono::microseconds> passwordEnteredAt;

    std::optional<std::chrono::seconds> timeLeftWhenCancelled;
    bool notificationVisible = false;

    QTimer timer;

    std::optional<seconds> calculateTokenRemainingTime(RemoteConnectionPtr connection) const
    {
        if (const auto tokenExpirationTime = connection->sessionTokenExpirationTime())
            return duration_cast<seconds>(*tokenExpirationTime - qnSyncTime->currentTimePoint());

        return std::nullopt;
    }

    std::optional<seconds> temporaryTokenRemainingTime(
        std::shared_ptr<RemoteSession> session) const
    {
        const QnUserResourcePtr user = session->systemContext()->userWatcher()->user();
        return user ? user->temporarySessionExpiresIn() : std::nullopt;
    }

    std::optional<seconds> calculatePasswordRemainingTime() const
    {
        // Check if password is unlimited.
        if (!passwordSessionTimeoutLimit)
            return std::nullopt;

        if (!NX_ASSERT(passwordEnteredAt, "Workflow error, session is not established"))
            return std::nullopt;

        const auto timePassed = qnSyncTime->currentTimePoint() - *passwordEnteredAt;
        return duration_cast<seconds>(*passwordSessionTimeoutLimit - timePassed);
    }

    static bool isExpired(std::optional<seconds> remainingTime)
    {
        return remainingTime && remainingTime <= seconds::zero();
    }

    std::optional<seconds> calculateSessionRemainingTime(RemoteConnectionPtr connection) const
    {
        return connection->credentials().authToken.isBearerToken()
            ? calculateTokenRemainingTime(connection)
            : calculatePasswordRemainingTime();
    }

    bool sessionExpired(RemoteConnectionPtr connection) const
    {
        return isExpired(calculateSessionRemainingTime(connection));
    }
};

//--------------------------------------------------------------------------------------------------

RemoteSessionTimeoutWatcher::RemoteSessionTimeoutWatcher(
    SystemSettings* settings,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    d->passwordSessionTimeoutLimit = settings->sessionTimeoutLimit();
    connect(settings, &SystemSettings::sessionTimeoutChanged, this,
        [this, settings]()
        {
            d->passwordSessionTimeoutLimit = settings->sessionTimeoutLimit();
        });

    d->timer.setInterval(kTimerInterval);
    connect(&d->timer, &QTimer::timeout, this, &RemoteSessionTimeoutWatcher::tick);
    d->timer.start();
}

RemoteSessionTimeoutWatcher::~RemoteSessionTimeoutWatcher()
{
}

void RemoteSessionTimeoutWatcher::tick()
{
    auto session = d->session.lock();
    if (!session)
        return;

    auto connection = session->connection();
    if (!NX_ASSERT(connection))
        return;

    if (connection->connectionInfo().isCloud())
        return;

    const std::optional<seconds> timeLeft = d->calculateSessionRemainingTime(connection);
    if (timeLeft)
        NX_VERBOSE(this, "Left %1", *timeLeft);

    if (d->sessionExpired(connection)
        || (timeLeft && *timeLeft <= kForceDisconnectTime))
    {
        NX_VERBOSE(this, "Session expired");
        if (d->notificationVisible)
        {
            d->timeLeftWhenCancelled.reset();
            d->notificationVisible = false;
            emit hideNotification();
        }

        const auto temporaryTokenRemainingTime = d->temporaryTokenRemainingTime(session);

        // Time calculation error caused by duration cast from microseconds to seconds.
        const auto kTimeCalculationError = 5s;
        const auto reason = temporaryTokenRemainingTime
            && *temporaryTokenRemainingTime <= kForceDisconnectTime + kTimeCalculationError
                ? SessionExpirationReason::temporaryTokenExpired
                : SessionExpirationReason::sessionExpired;

        if (temporaryTokenRemainingTime)
            NX_VERBOSE(this, "Temporary token remaining time: %1", *temporaryTokenRemainingTime);

        emit sessionExpired(reason);
        return;
    }

    NX_VERBOSE(this, "Session still active");

    // Try to disconnect user gracefully before session ends.
    if (timeLeft && (*timeLeft - kForceDisconnectTime < kTimerInterval))
        QTimer::singleShot(*timeLeft - kForceDisconnectTime, [this]() { tick(); });

    const bool isTimeToNotify = timeLeft && *timeLeft <= kFirstNotificationTime;
    const bool isTimeToLastNotify = timeLeft && *timeLeft <= kLastNotificationTime;
    const bool notificationWasCancelled = d->timeLeftWhenCancelled.has_value();
    bool notificationShouldBeVisible =
        ((isTimeToNotify && !notificationWasCancelled)
            || (isTimeToLastNotify && (*d->timeLeftWhenCancelled > kLastNotificationTime)));

    if (const auto temporaryTokenRemainingTime = d->temporaryTokenRemainingTime(session);
        temporaryTokenRemainingTime && timeLeft)
    {
        const auto deltaTime = *temporaryTokenRemainingTime - timeLeft.value();
        notificationShouldBeVisible &= deltaTime > kForceDisconnectTime;
    }

    if (notificationShouldBeVisible)
    {
        emit showNotification(duration_cast<minutes>(*timeLeft));
    }
    else if (d->notificationVisible)
    {
        emit hideNotification();
        d->timeLeftWhenCancelled.reset();
    }

    d->notificationVisible = notificationShouldBeVisible;

    // If user cancelled the first notification
    if (d->timeLeftWhenCancelled && (!isTimeToNotify || !timeLeft))
        d->timeLeftWhenCancelled.reset();
}

void RemoteSessionTimeoutWatcher::sessionStarted(std::shared_ptr<RemoteSession> session)
{
    NX_VERBOSE(this, "Session started");
    d->session = session;
    d->passwordEnteredAt = qnSyncTime->currentTimePoint();

    // Update password expiration time if user enters password manually.
    if (NX_ASSERT(session))
    {
        connect(session.get(),
            &RemoteSession::credentialsChanged,
            this,
            [this]()
            {
                d->passwordEnteredAt = qnSyncTime->currentTimePoint();
                tick();
            });

        connect(session->systemContext()->userWatcher(),
            &nx::vms::client::core::UserWatcher::userChanged,
            this,
            &RemoteSessionTimeoutWatcher::tick);

        tick();
    }
}

void RemoteSessionTimeoutWatcher::sessionStopped()
{
    NX_VERBOSE(this, "Session stopped");
    d->passwordEnteredAt.reset();
    d->timeLeftWhenCancelled.reset();
    d->session.reset();
    if (d->notificationVisible)
    {
        d->notificationVisible = false;
        emit hideNotification();
    }
}

void RemoteSessionTimeoutWatcher::notificationHiddenByUser()
{
    auto session = d->session.lock();
    if (!session)
        return;

    auto connection = session->connection();
    if (!NX_ASSERT(connection))
        return;

    d->timeLeftWhenCancelled = d->calculateSessionRemainingTime(connection);
    d->notificationVisible = false;
    emit hideNotification();
}

} // namespace nx::vms::client::core
