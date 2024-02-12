// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uuid_pool.h"

#include <type_traits>
#include <nx/utils/log/assert.h>

namespace {
    /* Quite reasonable default size for all pools. */
    const size_t defaultPoolSize = 256;
}

UuidPool::UuidPool(const nx::Uuid &baseId, offset_type size /* = std::numeric_limits<uint>::max()*/):
    m_baseid(baseId.getQUuid()),
    m_size(size)
{
    static_assert(std::is_unsigned<offset_type>::value, "Offset type must be unsigned");
    static_assert(sizeof(offset_type) <= sizeof(decltype(QUuid::data1)), "Offset type must be not greater than storage field size.");

    m_usageData.resize(defaultPoolSize, false);
}

void UuidPool::markAsUsed(const nx::Uuid &id) {
    if (!belongsToPool(id))
        return;

    offset_type i = offset(id);
    if (m_usageData.size() < i)
        m_usageData.resize(i, false);
    NX_ASSERT(m_usageData[i] == false, "Uuids must not be used twice");
    m_usageData[i] = true;
}

void UuidPool::markAsFree(const nx::Uuid &id) {
    if (!belongsToPool(id))
        return;

    offset_type i = offset(id);
    NX_ASSERT(m_usageData.size() >= i && m_usageData[i] == true, "Uuids must not be freed twice");
    if (m_usageData.size() < i)
        return;
    m_usageData[i] = false;
}


nx::Uuid UuidPool::getFreeId() const {
    auto iter = m_usageData.cbegin();
    while (iter != m_usageData.cend() && *iter)
        ++iter;

    offset_type i = std::distance(m_usageData.cbegin(), iter);
    nx::Uuid result(nx::Uuid::createUuidFromPool(m_baseid, i));
    NX_ASSERT(belongsToPool(result), "Make sure returned id is valid");

    return result;
}

UuidPool::offset_type UuidPool::offset(const nx::Uuid &id) const {
    return static_cast<offset_type>(id.getQUuid().data1 - m_baseid.data1);
}


bool UuidPool::belongsToPool(const nx::Uuid &id) const {
    const QUuid uuid(id.getQUuid());

    if (offset(id) > m_size)
        return false;

    if (uuid.data2 != m_baseid.data2)
        return false;

    if (uuid.data3 != m_baseid.data3)
        return false;

    return std::equal(uuid.data4, uuid.data4 + sizeof(uuid.data4), m_baseid.data4);
}
