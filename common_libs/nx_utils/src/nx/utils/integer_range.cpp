#include "integer_range.h"

#include <utility>

namespace nx {
namespace utils {

IntegerRange::IntegerRange(qint64 first, qint64 count):
    m_first(first),
    m_last(first + count - 1)
{
}

qint64 IntegerRange::first() const
{
    return m_first;
}

qint64 IntegerRange::last() const
{
    return m_last;
}

qint64 IntegerRange::next() const
{
    return m_last + 1;
}

qint64 IntegerRange::count() const
{
    return next() - m_first;
}

bool IntegerRange::isNull() const
{
    return m_last < m_first;
}

IntegerRange::operator bool() const
{
    return !isNull();
}

bool IntegerRange::intersects(const IntegerRange& other) const
{
    return !(*this & other).isNull();
}

bool IntegerRange::contains(qint64 value) const
{
    return value >= m_first && value <= m_last;
}

IntegerRange IntegerRange::shifted(qint64 delta) const
{
    return IntegerRange(m_first + delta, count());
}

IntegerRange IntegerRange::operator+(qint64 delta) const
{
    return shifted(delta);
}

IntegerRange IntegerRange::operator-(qint64 delta) const
{
    return shifted(-delta);
}

IntegerRange IntegerRange::intersected(const IntegerRange& other) const
{
    const int first = qMax(m_first, other.m_first);
    const int last = qMin(m_last, other.m_last);
    return IntegerRange(first, qMax(last - first + 1, 0));
}

IntegerRange IntegerRange::operator&(const IntegerRange& other) const
{
    return intersected(other);
}

IntegerRange::Pair IntegerRange::united(const IntegerRange& other) const
{
    Pair result{*this, other};
    if (result.left.m_first > result.right.m_first)
        std::swap(result.left, result.right);

    if (result.right.m_first <= result.left.next())
    {
        result.left.m_last = qMax(result.left.m_last, result.right.m_last);
        result.right = IntegerRange();
    }

    if (result.left.isNull() && !result.right.isNull())
        std::swap(result.left, result.right);

    return result;
}

IntegerRange::Pair IntegerRange::operator|(const IntegerRange& other) const
{
    return united(other);
}

IntegerRange::Pair IntegerRange::subtracted(const IntegerRange& other) const
{
    Pair result;
    if (other.m_first > m_first)
    {
        result.left.m_first = m_first;
        result.left.m_last = qMin(m_last, other.m_first - 1);
    }

    if (other.m_last < m_last)
    {
        result.right.m_first = qMax(m_first, other.m_last + 1);
        result.right.m_last = m_last;
    }

    return result;
}

IntegerRange::Pair IntegerRange::operator-(const IntegerRange& other) const
{
    return subtracted(other);
}

} // namespace utils
} // namespace nx
