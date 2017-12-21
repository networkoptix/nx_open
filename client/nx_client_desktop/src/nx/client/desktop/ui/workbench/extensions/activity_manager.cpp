#include "activity_manager.h"

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

ActivityManager::ActivityManager(QObject* parent):
    base_type(parent)
{
}

QList<QnUuid> ActivityManager::activities() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ids;
}

QnUuid ActivityManager::add(const QString& title, const QString& description, bool cancellable)
{
    QnMutexLocker lock(&m_mutex);
    const auto activityId = QnUuid::createUuid();
    m_lookup.insert(activityId, State(title, description, cancellable, 0.0));
    m_ids.push_back(activityId);

    lock.unlock();
    emit added(activityId);

    return activityId;
}

void ActivityManager::remove(const QnUuid& activityId)
{
    QnMutexLocker lock(&m_mutex);
    auto iter = m_lookup.find(activityId);
    if (iter == m_lookup.end())
        return;

    m_lookup.erase(iter);
    m_ids.removeOne(activityId);

    lock.unlock();
    emit removed(activityId);
}

QString ActivityManager::title(const QnUuid& activityId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_lookup.value(activityId).title;
}

QString ActivityManager::description(const QnUuid& activityId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_lookup.value(activityId).description;
}

void ActivityManager::setDescription(const QnUuid& activityId, const QString& value)
{
    QnMutexLocker lock(&m_mutex);
    auto iter = m_lookup.find(activityId);
    if (iter == m_lookup.end() || iter->description == value)
        return;

    iter->description = value;

    lock.unlock();
    emit descriptionChanged(activityId, value);
}

qreal ActivityManager::progress(const QnUuid& activityId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_lookup.value(activityId).progress;
}

void ActivityManager::setProgress(const QnUuid& activityId, qreal value)
{
    QnMutexLocker lock(&m_mutex);
    auto iter = m_lookup.find(activityId);
    if (iter == m_lookup.end() || iter->progress == value)
        return;

    iter->progress = value;

    lock.unlock();
    emit progressChanged(activityId, value);
}

bool ActivityManager::isCancellable(const QnUuid& activityId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_lookup.value(activityId).cancellable;
}

void ActivityManager::setCancellable(const QnUuid& activityId, bool value)
{
    QnMutexLocker lock(&m_mutex);
    auto iter = m_lookup.find(activityId);
    if (iter == m_lookup.end() || iter->cancellable == value)
        return;

    iter->cancellable = value;

    lock.unlock();
    emit cancellableChanged(activityId, value);
}

void ActivityManager::cancel(const QnUuid& activityId)
{
    QnMutexLocker lock(&m_mutex);
    const auto iter = m_lookup.find(activityId);
    if (iter == m_lookup.end() || !iter->cancellable)
        return;

    lock.unlock();
    emit cancelRequested(activityId);
}

void ActivityManager::interact(const QnUuid& activityId)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_lookup.contains(activityId))
        return;

    lock.unlock();
    emit interactionRequested(activityId);
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
