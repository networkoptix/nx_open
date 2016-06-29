#include "indentation.h"

QnIndentation::QnIndentation() : m_left(0), m_right(0)
{
}

QnIndentation::QnIndentation(int indent) : m_left(indent), m_right(indent)
{
}

QnIndentation::QnIndentation(int left, int right) : m_left(left), m_right(right)
{
}

int QnIndentation::left() const
{
    return m_left;
}

void QnIndentation::setLeft(int value)
{
    m_left = value;
}

int QnIndentation::right() const
{
    return m_left;
}

void QnIndentation::setRight(int value)
{
    m_right = value;
}
