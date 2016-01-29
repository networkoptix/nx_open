#include "updatable.h"

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
}

void QnUpdatable::endUpdate()
{
    Q_ASSERT_X(m_updateCount > 0, Q_FUNC_INFO, "Invalid begin/end sequence");
    --m_updateCount;
    if (m_updateCount == 0)
        afterUpdate();
}

bool QnUpdatable::isUpdating() const
{
    return m_updateCount > 0;
}

void QnUpdatable::beginUpdateInternal()
{}

void QnUpdatable::endUpdateInternal()
{}

void QnUpdatable::beforeUpdate()
{}

void QnUpdatable::afterUpdate()
{}


