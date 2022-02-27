// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "updatable.h"

#include <nx/utils/log/log.h>

QnUpdatable::QnUpdatable()
    : m_updateCount(0)
{
}

QnUpdatable::~QnUpdatable()
{
}

void QnUpdatable::beginUpdate()
{
    if (m_updateCount == 0)
        beforeUpdate();
    ++m_updateCount;
    beginUpdateInternal();
}

void QnUpdatable::endUpdate()
{
    NX_ASSERT(m_updateCount > 0, "Invalid begin/end sequence");
    endUpdateInternal();
    --m_updateCount;
    if (m_updateCount == 0)
        afterUpdate();
}

bool QnUpdatable::isUpdating() const
{
    return m_updateCount > 0;
}

void QnUpdatable::beginUpdateInternal()
{
}

void QnUpdatable::endUpdateInternal()
{
}

void QnUpdatable::beforeUpdate()
{
}

void QnUpdatable::afterUpdate()
{
}
