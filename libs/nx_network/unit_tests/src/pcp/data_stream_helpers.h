#pragma once

#include <QDataStream>

namespace dsh {

/** Reverse byte order in @param array */
void reverse(QByteArray& array);

/** Returns reversed copy of @param array */
QByteArray reversed(const QByteArray& array);

/** Writes binary @param data to array according to the byte order of @param stream.
 *  NOTE: @param size is used for assertion if specified */
int write(QDataStream& stream, const QByteArray& data, int size = 0);

/** Writes binary @param data to array according to the byte order of @param stream */
int read(QDataStream& stream, QByteArray& data, int size);

template <typename T>
QByteArray toBytes(const T& data, bool networkOrder = true)
{
    QByteArray a;
    QDataStream s(&a, QIODevice::WriteOnly);
    if (networkOrder)
        s.setByteOrder(QDataStream::LittleEndian);

    s << data;
    return a;
}

template <typename T>
T fromBytes(const QByteArray& bytes, bool networkOrder = true)
{
    T d;
    QDataStream s(bytes);
    if (networkOrder)
        s.setByteOrder(QDataStream::LittleEndian);

    s >> d;
    return d;
}

template <typename T>
QByteArray rawBytes(const T& data)
{
    return QByteArray((char*)&data, sizeof(T));
}

} // namespace dsh
