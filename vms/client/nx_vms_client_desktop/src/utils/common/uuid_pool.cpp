#include "uuid_pool.h"

#include <type_traits>
#include <nx/utils/log/assert.h>

namespace {
    /* Quite reasonable default size for all pools. */
    const size_t defaultPoolSize = 256;
}

QnUuidPool::QnUuidPool(const QnUuid &baseId, offset_type size /* = std::numeric_limits<uint>::max()*/):
    m_baseid(baseId.getQUuid()),
    m_size(size)
{
    static_assert(std::is_unsigned<offset_type>::value, "Offset type must be unsigned");
    static_assert(sizeof(offset_type) <= sizeof(decltype(QUuid::data1)), "Offset type must be not greater than storage field size.");

    m_usageData.resize(defaultPoolSize, false);
}

void QnUuidPool::markAsUsed(const QnUuid &id) {
    if (!belongsToPool(id))
        return;
    
    offset_type i = offset(id);
    if (m_usageData.size() < i)
        m_usageData.resize(i, false);
    NX_ASSERT(m_usageData[i] == false, Q_FUNC_INFO, "Uuids must not be used twice");
    m_usageData[i] = true;
}

void QnUuidPool::markAsFree(const QnUuid &id) {
    if (!belongsToPool(id))
        return;

    offset_type i = offset(id);
    NX_ASSERT(m_usageData.size() >= i && m_usageData[i] == true, Q_FUNC_INFO, "Uuids must not be freed twice");
    if (m_usageData.size() < i)
        return;
    m_usageData[i] = false;
}


QnUuid QnUuidPool::getFreeId() const {
    auto iter = m_usageData.cbegin();
    while (iter != m_usageData.cend() && *iter)
        ++iter;

    offset_type i = std::distance(m_usageData.cbegin(), iter);
    QnUuid result(QnUuid::createUuidFromPool(m_baseid, i));
    NX_ASSERT(belongsToPool(result), Q_FUNC_INFO, "Make sure returned id is valid");

    return result;
}

QnUuidPool::offset_type QnUuidPool::offset(const QnUuid &id) const {
    return static_cast<offset_type>(id.getQUuid().data1 - m_baseid.data1);
}


bool QnUuidPool::belongsToPool(const QnUuid &id) const {
    const QUuid uuid(id.getQUuid());
    
    if (offset(id) > m_size)
        return false;

    if (uuid.data2 != m_baseid.data2)
        return false;

    if (uuid.data3 != m_baseid.data3)
        return false;

    return std::equal(uuid.data4, uuid.data4 + sizeof(uuid.data4), m_baseid.data4);
}

