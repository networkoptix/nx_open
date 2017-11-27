#pragma once

#include "abstract_nonce_provider.h"

#include <nx/utils/thread/mutex.h>

class TimeBasedNonceProvider:
    public AbstractNonceProvider
{
public:
    TimeBasedNonceProvider(
        std::chrono::milliseconds maxServerTimeDifference = std::chrono::minutes(5),
        std::chrono::milliseconds steadyExpirationPeriod = std::chrono::hours(1));

    virtual QByteArray generateNonce() override;
    virtual bool isNonceValid(const QByteArray& nonce) const override;

private:
    using NonceTime = std::chrono::microseconds;
    using TimePoint = std::chrono::steady_clock::time_point;

    const std::chrono::milliseconds m_maxServerTimeDifference;
    const std::chrono::milliseconds m_steadyExpirationPeriod;

    mutable QnMutex m_mutex;
    mutable std::map<NonceTime, TimePoint> m_nonceCache; //< TODO: boost::multi_index?
};
