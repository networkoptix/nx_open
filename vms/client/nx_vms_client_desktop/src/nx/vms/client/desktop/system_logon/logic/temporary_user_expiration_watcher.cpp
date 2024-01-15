// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "temporary_user_expiration_watcher.h"

#include <chrono>

#include <core/resource/user_resource.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/text/human_readable.h>
#include <nx/vms/time/formatter.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

using namespace std::chrono_literals;
using namespace nx::vms::text;

namespace nx::vms::client::desktop {

namespace {
constexpr auto kTimerInterval = 1min;
constexpr auto kImportantNotificationTime = 1h;
constexpr auto kOneMonth = std::chrono::months(1);

QString durationToString(const std::chrono::seconds duration,
    const QString& separator,
    int suppressSecondUnitLimit = HumanReadable::kAlwaysSuppressSecondUnit)
{
    using namespace nx::vms::text;
    return HumanReadable::timeSpan(duration,
        HumanReadable::Days | HumanReadable::Hours | HumanReadable::Minutes,
        HumanReadable::SuffixFormat::Full,
        separator,
        suppressSecondUnitLimit);
}
} // namespace

TemporaryUserExpirationWatcher::TemporaryUserExpirationWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    m_notificationManager = context()->instance<workbench::LocalNotificationsManager>();
    connect(systemContext()->userWatcher(),
        &nx::vms::client::core::UserWatcher::userChanged,
        this,
        &TemporaryUserExpirationWatcher::createNotification);

    connect(&m_timer, &QTimer::timeout, this, &TemporaryUserExpirationWatcher::updateNotification);

    connect(m_notificationManager,
        &workbench::LocalNotificationsManager::cancelRequested,
        [this](QnUuid notificationId)
        {
            if (notificationId != m_notification)
                return;

            m_timer.stop();
            m_notificationManager->remove(*m_notification);
            m_notification = std::nullopt;
        });

    connect(systemContext()->serverTimeWatcher(),
        &core::ServerTimeWatcher::timeZoneChanged,
        this,
        &TemporaryUserExpirationWatcher::updateNotification);

    m_timer.setInterval(kTimerInterval);
}

void TemporaryUserExpirationWatcher::createNotification()
{
    const auto currentUser = systemContext()->userWatcher()->user();
    if (currentUser && currentUser->isTemporary())
    {
        m_timer.start();
        updateNotification();
        return;
    }

    m_timer.stop();
    if (m_notification)
    {
        m_notificationManager->remove(*m_notification);
        m_notification = std::nullopt;
    }
}

void TemporaryUserExpirationWatcher::updateNotification()
{
    const auto currentUser = systemContext()->userWatcher()->user();
    if (!currentUser || !currentUser->isTemporary())
        return;

    auto timeLeft = currentUser->temporarySessionExpiresIn().value();

    const auto expirationTimestampMs = qnSyncTime->currentMSecsSinceEpoch()
        + duration_cast<std::chrono::milliseconds>(timeLeft).count();

    static const QString separator =
        QString(" %1 ").arg(tr("and", /*comment*/ "Example: 1 month and 2 days"));

    auto timeStr = durationToString(timeLeft, separator);
    QString notificationText;

    if (const auto months = duration_cast<std::chrono::months>(timeLeft); months >= kOneMonth)
    {
        const auto timeWatcher = systemContext()->serverTimeWatcher();
        timeStr = nx::vms::time::toString(timeWatcher->displayTime(expirationTimestampMs).date());
        notificationText =
            tr("Your access to the System expires %1", /*comment*/ "%1 is a date").arg(timeStr);
    }
    else if (const auto minutes = duration_cast<std::chrono::minutes>(timeLeft); minutes < 1min)
    {
        timeStr = durationToString(1min, separator);
    }

    if (notificationText.isEmpty())
    {
        notificationText =
            tr("Your access to the System expires in %1", /*comment*/ "%1 is a duration")
                .arg(timeStr);
    }

    if (m_notification)
        m_notificationManager->setTitle(*m_notification, notificationText);
    else
        m_notification = m_notificationManager->add(notificationText, "", /*cancellable*/ true);

    auto currentLevel = m_notificationManager->level(*m_notification);

    if (duration_cast<std::chrono::hours>(timeLeft) >= kImportantNotificationTime)
    {
        if(currentLevel != QnNotificationLevel::Value::OtherNotification)
        {
            m_notificationManager->setLevel(
                *m_notification, QnNotificationLevel::Value::OtherNotification);
            m_notificationManager->setIcon(*m_notification, qnSkin->pixmap("events/stopwatch.svg"));
        }
    }
    else if (currentLevel != QnNotificationLevel::Value::ImportantNotification)
    {
        m_notificationManager->setLevel(
            *m_notification, QnNotificationLevel::Value::ImportantNotification);
        m_notificationManager->setIcon(
            *m_notification, qnSkin->pixmap("events/stopwatch_yellow.svg"));
    }
}

} // namespace nx::vms::client::desktop
