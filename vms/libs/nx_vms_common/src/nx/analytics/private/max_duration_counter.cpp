// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "max_duration_counter.h"

#include <nx/utils/log/assert.h>

namespace nx::analytics {

qint64 MaxDurationCounter::max() const
{
    NX_ASSERT(size(), "Cache should have elements to count them.");

    qint64 max = 0;
    if (m_pushStack.size())
        max = std::max(m_pushStack.top().m_max, max);
    if (m_popStack.size())
        max = std::max(m_popStack.top().m_max, max);
    return max;
}

void MaxDurationCounter::push(const qint64 value)
{
    const qint64 max = m_pushStack.empty() ? value : std::max(m_pushStack.top().m_max, value);
    m_pushStack.push(Element(value, max));
}



void MaxDurationCounter::pop(size_t count)
{
    NX_ASSERT(size() >= count, "Cache should have elements to release them.");

    for (size_t i = 0; i < count; ++i)
    {
        if (m_popStack.empty())
        {
            // move from push
            qint64 max = m_pushStack.top().m_value;
            while (!m_pushStack.empty())
            {
                max = std::max(max, m_pushStack.top().m_value);

                m_popStack.push({m_pushStack.top().m_value, max});
                m_pushStack.pop();
            }
        }

        m_popStack.pop();
    }
}

} // namespace nx::analytics
