// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QnIndents
{
public:
    constexpr QnIndents() noexcept {};

    constexpr QnIndents(int left, int right) noexcept:
        m_left(left),
        m_right(right)
    {
    };

    constexpr QnIndents(int indent) noexcept:
        m_left(indent),
        m_right(indent)
    {
    };

    constexpr int left() const noexcept
    {
        return m_left;
    };

    inline void setLeft(int value) noexcept
    {
        m_left = value;
    };

    constexpr int right() const noexcept
    {
        return m_right;
    };

    inline void setRight(int value) noexcept
    {
        m_right = value;
    };

private:
    int m_left = 0;
    int m_right = 0;
};
