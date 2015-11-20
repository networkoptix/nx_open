/**********************************************************
* Aug 10, 2015
* a.kolesnikov
***********************************************************/

#include "settings.h"

#include <nx1/info.h>
#include <utils/common/app_info.h>


namespace ec2 {

namespace params
{
    static const QString dbReadOnly(lit("ecDbReadOnly"));
    static const bool dbReadOnlyDefault = false;

    //time_sync params
    static const QString internetSyncTimePeriodSec(lit("ecInternetSyncTimePeriodSec"));
    static const QString maxInternetTimeSyncRetryPeriodSec(lit("ecMaxInternetTimeSyncRetryPeriodSec"));

    //transaction connection params
    static const QString connectionKeepAliveTimeoutMs(lit("ecConnectionKeepAliveTimeoutMs"));
    static const unsigned int connectionKeepAliveTimeoutMsDefault = 5*1000;

    static const QString keepAliveProbeCount(lit("ecKeepAliveProbeCount"));
    static const unsigned int keepAliveProbeCountDefault = 3;
}


bool Settings::dbReadOnly() const
{
    //if booted from SD card, setting ecDbReadOnly to true by default
    bool defaultValue = params::dbReadOnlyDefault;
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (QnAppInfo::armBox() == "bpi" || QnAppInfo::armBox() == "nx1")
    {
        defaultValue = Nx1::isBootedFromSD();
    }
#endif

    return value<bool>( params::dbReadOnly, defaultValue );
}

size_t Settings::internetSyncTimePeriodSec(size_t defaultValue) const
{
    return value<size_t>(params::internetSyncTimePeriodSec, defaultValue);
}

size_t Settings::maxInternetTimeSyncRetryPeriodSec(size_t defaultValue) const
{
    return value<size_t>(params::maxInternetTimeSyncRetryPeriodSec, defaultValue);
}

std::chrono::milliseconds Settings::connectionKeepAliveTimeout() const
{
    return std::chrono::milliseconds(value<unsigned int>(
        params::connectionKeepAliveTimeoutMs,
        params::connectionKeepAliveTimeoutMsDefault));
}

int Settings::keepAliveProbeCount() const
{
    return value<unsigned int>(
        params::keepAliveProbeCount,
        params::keepAliveProbeCountDefault);
}

void Settings::loadParams( std::map<QString, QVariant> confParams )
{
    QMutexLocker lk( &m_mutex );
    m_confParams = std::move( confParams );
}

}   //ec2
