/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_AUTH_ABSTRACT_NONCE_PROVIDER_H
#define NX_AUTH_ABSTRACT_NONCE_PROVIDER_H

#include <QtCore/QByteArray>


class AbstractNonceProvider
{
public:
    virtual ~AbstractNonceProvider() {}

    virtual QByteArray generateNonce() = 0;
    virtual bool isNonceValid(const QByteArray& nonce) const = 0;
};

#endif  //NX_AUTH_ABSTRACT_NONCE_PROVIDER_H
