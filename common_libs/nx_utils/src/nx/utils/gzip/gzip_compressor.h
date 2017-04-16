#pragma once 

#include <QObject>

namespace nx {
namespace utils {
namespace bsf {
namespace gzip {

class NX_UTILS_API Compressor
{
public:
    static QByteArray compressData(const QByteArray& data);
};

} // namespace gzip
} // namespace bsf
} // namespace utils
} // namespace nx
