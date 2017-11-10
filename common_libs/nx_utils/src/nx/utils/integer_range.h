#pragma once

namespace nx {
namespace utils {

// TODO: #vkutin Write unit tests.

// A class representing integer ranges and basic operations with them.
class NX_UTILS_API IntegerRange
{
public:
    IntegerRange() = default;
    IntegerRange(const IntegerRange& other) = default;
    IntegerRange(qint64 first, qint64 count);

    qint64 first() const; //< First element in range.
    qint64 last() const;  //< Last element in range.
    qint64 next() const;  //< Next element past range.
    qint64 count() const; //< Count of elements in range.

    bool contains(qint64 value) const; //< Whether range contains specified value.

    bool isNull() const; //< Whether range is empty.
    explicit operator bool() const; //< Whether range is non-empty.

    // Shift entire range.
    IntegerRange shifted(qint64 delta) const;
    IntegerRange operator+(qint64 delta) const;
    IntegerRange operator-(qint64 delta) const;

    // Two range intersection.
    bool intersects(const IntegerRange& other) const;
    IntegerRange intersected(const IntegerRange& other) const;
    IntegerRange operator&(const IntegerRange& other) const;

    // Struct that keeps two ranges, left and right.
    struct Pair;

    // Two range union, produces one or two resulting ranges.
    Pair united(const IntegerRange& other) const;
    Pair operator|(const IntegerRange& other) const;

    // Two range subtraction, produces one or two resulting ranges.
    Pair subtracted(const IntegerRange& other) const;
    Pair operator-(const IntegerRange& other) const;

private:
    qint64 m_first = 0;
    qint64 m_last = -1;
};

struct IntegerRange::Pair
{
    IntegerRange left;
    IntegerRange right;
};

} // namespace utils
} // namespace nx
