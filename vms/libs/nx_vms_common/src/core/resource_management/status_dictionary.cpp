// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "status_dictionary.h"

#include <nx/utils/log/assert.h>

QnResourceStatusDictionary::QnResourceStatusDictionary(QObject* parent):
    QObject(parent),
    QnCommonModuleAware(parent)
{

}

nx::vms::api::ResourceStatus QnResourceStatusDictionary::value(const QnUuid& resourceId) const
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    auto itr = m_items.find(resourceId);
    return itr != m_items.end() ? itr.value() : nx::vms::api::ResourceStatus::offline;
}

void QnResourceStatusDictionary::setValue(const QnUuid& resourceId, nx::vms::api::ResourceStatus status)
{
    NX_ASSERT(!resourceId.isNull());
    NX_MUTEX_LOCKER lock( &m_mutex );
    m_items[resourceId] = status;
}

void QnResourceStatusDictionary::clear()
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    m_items.clear();
}

void QnResourceStatusDictionary::clear(const QVector<QnUuid>& idList)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    for(const QnUuid& id: idList)
        m_items.remove(id);
}

void QnResourceStatusDictionary::remove(const QnUuid& id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_items.remove(id);
}

QMap<QnUuid, nx::vms::api::ResourceStatus> QnResourceStatusDictionary::values() const
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    return m_items;
}
