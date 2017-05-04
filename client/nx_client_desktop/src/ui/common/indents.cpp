#include "indents.h"

QnIndents::QnIndents() :
    QnIndents(0, 0)
{
}

QnIndents::QnIndents(int left, int right) :
    m_left(left),
    m_right(right)
{
}

QnIndents::QnIndents(int indent) :
    QnIndents(indent, indent)
{
}

int QnIndents::left() const
{
    return m_left;
}

void QnIndents::setLeft(int value)
{
    m_left = value;
}

int QnIndents::right() const
{
    return m_right;
}

void QnIndents::setRight(int value)
{
    m_right = value;
}
