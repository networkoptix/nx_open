#pragma once

#include <QtCore/QMap>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/auth/abstract_nonce_provider.h>

class TimeBasedNonceProvider:
    public nx::vms::auth::AbstractNonceProvider
{
public:
    virtual QByteArray generateNonce() override;
    virtual bool isNonceValid(const QByteArray& nonce) const override;

private:
    //map<nonce, nonce creation timestamp usec>
    mutable QMap<qint64, qint64> m_cookieNonceCache;
    mutable QnMutex m_cookieNonceCacheMutex;
};
