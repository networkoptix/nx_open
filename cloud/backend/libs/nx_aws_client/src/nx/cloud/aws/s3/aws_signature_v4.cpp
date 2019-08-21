#include "aws_signature_v4.h"

#include <nx/utils/cryptographic_hash.h>

namespace nx::cloud::aws::s3 {

QByteArray SignatureCalculator::encodePath(const QString& path)
{
    return QUrl::toPercentEncoding(path, "/");
}

} // namespace nx::cloud::aws::s3
