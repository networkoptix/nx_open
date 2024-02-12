// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_manager.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

using namespace nx::vms::api;

namespace nx::vms::common {

ShowreelManager::ShowreelManager(QObject* parent):
    base_type(parent)
{
}

ShowreelManager::~ShowreelManager()
{
}

const ShowreelDataList& ShowreelManager::showreels() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_showreels;
}

ShowreelDataList ShowreelManager::showreels(const QList<nx::Uuid>& ids) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto matching = QSet(ids.begin(), ids.end());
    ShowreelDataList result;
    for (const auto& showreel: m_showreels)
    {
        if (matching.contains(showreel.id))
            result.push_back(showreel);
    }
    return result;
}

void ShowreelManager::resetShowreels(const ShowreelDataList& showreels)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QHash<nx::Uuid, ShowreelData> backup;
    for (auto showreel: m_showreels)
        backup.insert(showreel.id, std::move(showreel));
    m_showreels = showreels;
    lock.unlock();

    for (auto showreel: showreels)
    {
        auto old = backup.find(showreel.id);
        if (old == backup.end())
        {
            emit showreelAdded(showreel);
        }
        else
        {
            if ((*old) != showreel)
                emit showreelChanged(showreel);
            backup.erase(old);
        }
    }

    for (auto old: backup)
        emit showreelRemoved(old.id);
}

ShowreelData ShowreelManager::showreel(const nx::Uuid& id) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = std::find_if(m_showreels.cbegin(), m_showreels.cend(),
        [&id](const ShowreelData& data)
        {
            return data.id == id;
        });
    return iter != m_showreels.cend() ? *iter : ShowreelData();
}

void ShowreelManager::addOrUpdateShowreel(const ShowreelData& showreel)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (auto& existing: m_showreels)
    {
        if (existing.id == showreel.id)
        {
            if (existing == showreel)
                return;

            existing = showreel;
            lock.unlock();

            emit showreelChanged(showreel);
            return;
        }
    }
    m_showreels.push_back(showreel);
    lock.unlock();

    emit showreelAdded(showreel);
}

void ShowreelManager::removeShowreel(const nx::Uuid& showreelId)
{
    if (showreelId.isNull())
        NX_WARNING(this, "Removing showreel with empty Id will not take effect");

    NX_MUTEX_LOCKER lock(&m_mutex);
    auto iter = std::find_if(m_showreels.begin(), m_showreels.end(),
        [showreelId](const ShowreelData& data)
        {
            return data.id == showreelId;
        });

    if (iter == m_showreels.end())
        return;

    m_showreels.erase(iter);
    lock.unlock();

    emit showreelRemoved(showreelId);
}

} // namespace nx::vms::common
