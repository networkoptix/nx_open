// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_notifications_manager.h"

namespace nx::vms::client::desktop::workbench {

LocalNotificationsManager::Progress::Progress(State state):
    m_state(state)
{
}

LocalNotificationsManager::Progress::Progress(qreal value):
    m_state(value)
{
}

bool LocalNotificationsManager::Progress::isCompleted() const
{
    if (auto state = std::get_if<State>(&m_state))
        return *state == completed;

    return false;
}

bool LocalNotificationsManager::Progress::isFailed() const
{
    if (auto state = std::get_if<State>(&m_state))
        return *state == failed;

    return false;
}

bool LocalNotificationsManager::Progress::isIndefinite() const
{
    if (auto state = std::get_if<State>(&m_state))
        return *state == indefinite;

    return false;
}

std::optional<qreal> LocalNotificationsManager::Progress::value() const
{
    if (auto value = std::get_if<qreal>(&m_state))
        return *value;

    return {};
}

LocalNotificationsManager::LocalNotificationsManager(QObject* parent):
    base_type(parent)
{
}

QList<QnUuid> LocalNotificationsManager::notifications() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_ids;
}

QnUuid LocalNotificationsManager::add(
    const QString& title, const QString& description, bool cancellable)
{
    return add(State(title, description, cancellable, /*progress*/ std::nullopt));
}

QnUuid LocalNotificationsManager::addProgress(
    const QString& title, const QString& description, bool cancellable)
{
    return add(State(title, description, cancellable, /*progress*/ 0));
}

QnUuid LocalNotificationsManager::add(State state)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto notificationId = QnUuid::createUuid();
    m_lookup.insert(notificationId, std::move(state));
    m_ids.push_back(notificationId);
    lock.unlock();

    emit added(notificationId);
    return notificationId;
}

void LocalNotificationsManager::remove(const QnUuid& notificationId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end())
        return;

    m_lookup.erase(iter);
    m_ids.removeOne(notificationId);

    lock.unlock();
    emit removed(notificationId);
}

QString LocalNotificationsManager::title(const QnUuid& notificationId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lookup.value(notificationId).title;
}

void LocalNotificationsManager::setTitle(const QnUuid& notificationId, const QString& value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end() || iter->title == value)
        return;

    iter->title = value;

    lock.unlock();
    emit titleChanged(notificationId, value);
}

QString LocalNotificationsManager::description(const QnUuid& notificationId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lookup.value(notificationId).description;
}

void LocalNotificationsManager::setDescription(const QnUuid& notificationId, const QString& value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end() || iter->description == value)
        return;

    iter->description = value;

    lock.unlock();
    emit descriptionChanged(notificationId, value);
}

QPixmap LocalNotificationsManager::icon(const QnUuid& notificationId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lookup.value(notificationId).icon;
}

void LocalNotificationsManager::setIcon(const QnUuid& notificationId, const QPixmap& value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end())
        return;

    iter->icon = value;

    lock.unlock();
    emit iconChanged(notificationId, value);
}

std::optional<LocalNotificationsManager::Progress> LocalNotificationsManager::progress(
    const QnUuid& notificationId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lookup.value(notificationId).progress;
}

void LocalNotificationsManager::setProgress(
    const QnUuid& notificationId, std::optional<LocalNotificationsManager::Progress> value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end() || iter->progress == value)
        return;

    iter->progress = value;

    lock.unlock();
    emit progressChanged(notificationId, value);
}

QString LocalNotificationsManager::progressFormat(const QnUuid& notificationId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lookup.value(notificationId).format;
}

void LocalNotificationsManager::setProgressFormat(
    const QnUuid& notificationId, const QString& value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end() || iter->format == value)
        return;

    iter->format = value;

    lock.unlock();
    emit progressFormatChanged(notificationId, value);
}

bool LocalNotificationsManager::isCancellable(const QnUuid& notificationId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lookup.value(notificationId).cancellable;
}

void LocalNotificationsManager::setCancellable(const QnUuid& notificationId, bool value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end() || iter->cancellable == value)
        return;

    iter->cancellable = value;

    lock.unlock();
    emit cancellableChanged(notificationId, value);
}

CommandActionPtr LocalNotificationsManager::action(const QnUuid& notificationId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lookup.value(notificationId).action;
}

void LocalNotificationsManager::setAction(const QnUuid& notificationId, CommandActionPtr value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end() || iter->action == value)
        return;

    iter->action = value;

    lock.unlock();
    emit actionChanged(notificationId, value);
}

QnNotificationLevel::Value LocalNotificationsManager::level(const QnUuid& notificationId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lookup.value(notificationId).level;
}

void LocalNotificationsManager::setLevel(
    const QnUuid& notificationId, QnNotificationLevel::Value level)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end() || iter->level == level)
        return;

    iter->level = level;

    lock.unlock();
    emit levelChanged(notificationId, level);
}

QString LocalNotificationsManager::additionalText(const QnUuid& notificationId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lookup[notificationId].additionalText;
}

void LocalNotificationsManager::setAdditionalText(
    const QnUuid& notificationId, QString additionalText)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end() || iter->additionalText == additionalText)
        return;

    iter->additionalText = additionalText;

    lock.unlock();
    emit additionalTextChanged(notificationId, additionalText);
}

void LocalNotificationsManager::cancel(const QnUuid& notificationId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto iter = m_lookup.find(notificationId);
    if (iter == m_lookup.end() || !iter->cancellable)
        return;

    lock.unlock();
    emit cancelRequested(notificationId);
}

void LocalNotificationsManager::interact(const QnUuid& notificationId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_lookup.contains(notificationId))
        return;

    lock.unlock();
    emit interactionRequested(notificationId);
}

} // namespace nx::vms::client::desktop::workbench
