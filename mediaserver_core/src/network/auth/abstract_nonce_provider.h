#pragma once

#include <QtCore/QByteArray>

class AbstractNonceProvider
{
public:
    virtual ~AbstractNonceProvider() = default;

    virtual QByteArray generateNonce() = 0;
    virtual bool isNonceValid(const QByteArray& nonce) const = 0;
};
