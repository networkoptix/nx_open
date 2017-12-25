#include "workbench_progress_manager.h"

namespace nx {
namespace client {
namespace desktop {

WorkbenchProgressManager::WorkbenchProgressManager(QObject* parent):
    base_type(parent)
{
}

QList<QnUuid> WorkbenchProgressManager::activities() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ids;
}

QnUuid WorkbenchProgressManager::add(const QString& title, const QString& description, bool cancellable)
{
    QnMutexLocker lock(&m_mutex);
    const auto activityId = QnUuid::createUuid();
    m_lookup.insert(activityId, State(title, description, cancellable, 0.0));
    m_ids.push_back(activityId);

    lock.unlock();
    emit added(activityId);

    return activityId;
}

void WorkbenchProgressManager::remove(const QnUuid& activityId)
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

QString WorkbenchProgressManager::title(const QnUuid& activityId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_lookup.value(activityId).title;
}

QString WorkbenchProgressManager::description(const QnUuid& activityId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_lookup.value(activityId).description;
}

void WorkbenchProgressManager::setDescription(const QnUuid& activityId, const QString& value)
{
    QnMutexLocker lock(&m_mutex);
    auto iter = m_lookup.find(activityId);
    if (iter == m_lookup.end() || iter->description == value)
        return;

    iter->description = value;

    lock.unlock();
    emit descriptionChanged(activityId, value);
}

qreal WorkbenchProgressManager::progress(const QnUuid& activityId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_lookup.value(activityId).progress;
}

void WorkbenchProgressManager::setProgress(const QnUuid& activityId, qreal value)
{
    QnMutexLocker lock(&m_mutex);
    auto iter = m_lookup.find(activityId);
    if (iter == m_lookup.end() || iter->progress == value)
        return;

    iter->progress = value;

    lock.unlock();
    emit progressChanged(activityId, value);
}

bool WorkbenchProgressManager::isCancellable(const QnUuid& activityId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_lookup.value(activityId).cancellable;
}

void WorkbenchProgressManager::setCancellable(const QnUuid& activityId, bool value)
{
    QnMutexLocker lock(&m_mutex);
    auto iter = m_lookup.find(activityId);
    if (iter == m_lookup.end() || iter->cancellable == value)
        return;

    iter->cancellable = value;

    lock.unlock();
    emit cancellableChanged(activityId, value);
}

void WorkbenchProgressManager::cancel(const QnUuid& activityId)
{
    QnMutexLocker lock(&m_mutex);
    const auto iter = m_lookup.find(activityId);
    if (iter == m_lookup.end() || !iter->cancellable)
        return;

    lock.unlock();
    emit cancelRequested(activityId);
}

void WorkbenchProgressManager::interact(const QnUuid& activityId)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_lookup.contains(activityId))
        return;

    lock.unlock();
    emit interactionRequested(activityId);
}

} // namespace desktop
} // namespace client
} // namespace nx
