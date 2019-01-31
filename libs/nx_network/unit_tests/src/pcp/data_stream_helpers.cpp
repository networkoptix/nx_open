#include "data_stream_helpers.h"

#include <nx/utils/log/assert.h>

namespace dsh {

void reverse(QByteArray& array)
{
    if (array.isNull())
        return;

    for (auto i = array.data(), j = array.data() + array.size() - 1; i < j; ++i, --j)
        std::swap(*i, *j);
}

QByteArray reversed(const QByteArray& array)
{
    QByteArray rev(array);
    reverse(rev);
    return rev;
}

int write(QDataStream& stream, const QByteArray& data, int size)
{
    NX_ASSERT(data.size() == size);
    if (stream.byteOrder() == QDataStream::LittleEndian)
    {
        QByteArray rev(data);
        reverse(rev);
        return stream.writeRawData(rev.data(), size);
    }
    return stream.writeRawData(data.data(), size);
}

int read(QDataStream& stream, QByteArray& data, int size)
{
    data.resize(size);
    int n = stream.readRawData(data.data(), size);
    if (n != size) return n;

    if (stream.byteOrder() == QDataStream::LittleEndian)
        reverse(data);
    return size;
}

} // namespace dsh
