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
    ++m_updateCount;
    if (m_updateCount == 1)
        updateStarted();
}

void QnUpdatable::endUpdate()
{
    Q_ASSERT_X(m_updateCount > 0, Q_FUNC_INFO, "Invalid begin/end sequence");
    --m_updateCount;
    if (m_updateCount == 0)
        updateFinished();
}

bool QnUpdatable::inUpdate() const
{
    return m_updateCount > 0;
}

void QnUpdatable::beginUpdateInternal()
{
}

void QnUpdatable::endUpdateInternal()
{
}

void QnUpdatable::updateStarted()
{
}

void QnUpdatable::updateFinished()
{
}


