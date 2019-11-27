#pragma once

#include <QtCore/QtEndian>

// Make qToBigEndian() and qFromBigEndian() work on Linux with int64_t and uint64_t.
//
// This is a workaround for a Qt deficiency: on GCC/Clang, qbswap() is not defined for int64_t
// because int64_t is `long`, and qint64 `long long`, which are considered distinct types. The same
// takes place with the unsigned version. On Windows there is no such problem because qint64 is
// defined as __int64, and int64_t is defined as `long long`, which are considered synonymous.
#if !defined(_WIN32)

template<>
inline Q_DECL_CONSTEXPR int64_t qbswap<int64_t>(int64_t value)
{
    return qbswap<qint64>(value);
}

template<>
inline Q_DECL_CONSTEXPR uint64_t qbswap<uint64_t>(uint64_t value)
{
    return qbswap<quint64>(value);
}

#endif // defined(LINUX)

namespace nx {
namespace utils {

/**
 * Wrapper for integer values: internally stores the number as Big Endian.
 */
template<typename Value>
class BigEndian
{
private:
    Value m_bigEndian;

public:
    BigEndian() = default;

    BigEndian(Value value)
    {
        m_bigEndian = qToBigEndian(value);
    }

    operator Value() const
    {
        return qFromBigEndian(m_bigEndian);
    }
};

/**
 * Tool which helps filling the buffers with structures having the required byte layout, e.g. for
 * sending over the network.
 *
 * Constructs a new Value using a constructor, being appended to the given buffer.
 *
 * Usage:
 * ```
 *     auto header = makeAppended<Header>(&buffer); //< Append the buffer and default-initialize.
 *     header->id = id; //< Fill the new structure.
 * ```
 * or
 * ```
 *     makeAppended<Id>(&buffer, id); //< Append the buffer and initialize with the given value.
 * ```
 *
 * @return Pointer to the newly constructed value.
 * @param args Arguments to the Value constructor, possibly omitted.
 */
template<typename Value, typename... Args>
Value* makeAppended(QByteArray* buffer, Args... args)
{
    buffer->resize(buffer->size() + sizeof(Value));
    // "Placement new" initializes the memory at the end of the buffer with the Value constructor.
    return new (&buffer[buffer->size() - sizeof(Value)]) Value(args...);
}

} // namespace utils
} // namespace nx
