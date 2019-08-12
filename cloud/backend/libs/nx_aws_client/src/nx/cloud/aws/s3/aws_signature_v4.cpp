#include "aws_signature_v4.h"

#include <nx/utils/cryptographic_hash.h>

namespace nx::cloud::aws::s3 {

void SignatureCalculator::hashPath(
    nx::utils::QnCryptographicHash* hash, const QString& path)
{
    const auto encodedPath = QUrl::toPercentEncoding(path, "/");
    if (!encodedPath.startsWith('/'))
        hash->addData("/");
    hash->addData(encodedPath);
}

} // namespace nx::cloud::aws::s3
