#pragma once

#include <QtCore/QByteArray>

namespace nx {
namespace vms {
namespace auth {

class AbstractNonceProvider
{
public:
    virtual ~AbstractNonceProvider() = default;

    virtual QByteArray generateNonce() = 0;
    virtual bool isNonceValid(const QByteArray& nonce) const = 0;
};

} // namespace auth
} // namespace vms
} // namespace nx
