/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_AUTH_TIME_BASED_NONCE_PROVIDER_H
#define NX_AUTH_TIME_BASED_NONCE_PROVIDER_H

#include "abstract_nonce_provider.h"

#include <QtCore/QMap>


class TimeBasedNonceProvider
:
    public AbstractNonceProvider
{
public:
    virtual QByteArray generateNonce() override;
    virtual bool isNonceValid(const QByteArray& nonce) const override;

private:
    //map<nonce, nonce creation timestamp usec>
    mutable QMap<qint64, qint64> m_cookieNonceCache;
    mutable QnMutex m_cookieNonceCacheMutex;
};

#endif  //NX_AUTH_TIME_BASED_NONCE_PROVIDER_H
