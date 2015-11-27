#pragma once

#include <QtCore/QUuid>

#include <nx/utils/uuid.h>

/** Class for generating uuid's from the fixed pool. */
class QnUuidPool {
    typedef uint offset_type;
public:
    QnUuidPool(const QnUuid &baseId, offset_type size = std::numeric_limits<offset_type>::max());

    /** Mark id as used. */
    void markAsUsed(const QnUuid &id);

    /** Mark id as free. */
    void markAsFree(const QnUuid &id);

    /** Get next free id. */
    QnUuid getFreeId() const;

private:
    offset_type offset(const QnUuid &id) const;

    /** Check if id belongs to given pool. */
    bool belongsToPool(const QnUuid &id) const;

private:
    const QUuid m_baseid;
    const offset_type m_size;
    std::vector<bool> m_usageData;
};