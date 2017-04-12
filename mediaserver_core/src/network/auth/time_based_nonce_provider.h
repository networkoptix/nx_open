#pragma once

#include "abstract_nonce_provider.h"

#include <QtCore/QMap>

#include <nx/utils/thread/mutex.h>

class TimeBasedNonceProvider:
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
