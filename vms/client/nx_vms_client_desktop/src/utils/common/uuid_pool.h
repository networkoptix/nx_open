// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUuid>

#include <nx/utils/uuid.h>

/** Class for generating uuid's from the fixed pool. */
class UuidPool {
    typedef uint offset_type;
public:
    UuidPool(const nx::Uuid &baseId, offset_type size = std::numeric_limits<offset_type>::max());

    /** Mark id as used. */
    void markAsUsed(const nx::Uuid &id);

    /** Mark id as free. */
    void markAsFree(const nx::Uuid &id);

    /** Get next free id. */
    nx::Uuid getFreeId() const;

private:
    offset_type offset(const nx::Uuid &id) const;

    /** Check if id belongs to given pool. */
    bool belongsToPool(const nx::Uuid &id) const;

private:
    const QUuid m_baseid;
    const offset_type m_size;
    std::vector<bool> m_usageData;
};
