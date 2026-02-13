// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stack>

#include <QtCore/qtypes.h>

namespace nx::analytics {

class NX_VMS_COMMON_API MaxDurationCounter
{
    struct Element
    {
        qint64 m_value;
        qint64 m_max;
        Element(qint64 value, qint64 max):
            m_value(value),
            m_max(max)
        {
        }
    };

    std::stack<Element> m_pushStack;
    std::stack<Element> m_popStack;

public:
    inline size_t size() const { return m_popStack.size() + m_pushStack.size(); }
    void push(const qint64 value);
    void pop(size_t count = 1);
    qint64 max() const;
};

} // namespace nx::analytics
