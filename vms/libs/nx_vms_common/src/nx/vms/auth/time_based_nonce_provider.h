// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/vms/auth/abstract_nonce_provider.h>

class NX_VMS_COMMON_API TimeBasedNonceProvider: public nx::vms::auth::AbstractNonceProvider
{
public:
    TimeBasedNonceProvider(
        std::chrono::milliseconds maxServerTimeDifference = std::chrono::minutes(5),
        std::chrono::milliseconds steadyExpirationPeriod = std::chrono::hours(1));

    virtual nx::String generateNonce() override;
    virtual bool isNonceValid(const nx::String& nonce) const override;

    /**
     * Generate time based nonce. Function uses current VMS (synchronized) time
     * if nonce time is not provided.
     */
    static nx::String generateTimeBasedNonce(
        std::chrono::microseconds nonceTime = std::chrono::microseconds::zero());

private:
    using NonceTime = std::chrono::microseconds;
    using TimePoint = std::chrono::steady_clock::time_point;

    const std::chrono::milliseconds m_maxServerTimeDifference;
    const std::chrono::milliseconds m_steadyExpirationPeriod;

    mutable nx::Mutex m_mutex;
    mutable std::map<NonceTime, TimePoint> m_nonceCache; //< TODO: boost::multi_index?
};
