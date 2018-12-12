#pragma once 

#include <QObject>

namespace nx {
namespace utils {
namespace bstream {
namespace gzip {

class NX_UTILS_API Compressor
{
public:
    static QByteArray compressData(const QByteArray& data);
};

} // namespace gzip
} // namespace bstream
} // namespace utils
} // namespace nx
