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
    static const QString dbReadOnly("ecDbReadOnly");
    static const bool dbReadOnlyDefault = false;

    //time_sync params
    static const QString internetSyncTimePeriodSec("ecInternetSyncTimePeriodSec");
    static const QString maxInternetTimeSyncRetryPeriodSec("ecMaxInternetTimeSyncRetryPeriodSec");
}


bool Settings::dbReadOnly() const
{
    //if booted from SD card, setting ecDbReadOnly to true by default
    bool defaultValue = params::dbReadOnlyDefault;
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (QnAppInfo::isBpi() || QnAppInfo::isNx1())
        defaultValue = Nx1::isBootedFromSD();
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

void Settings::loadParams( std::map<QString, QVariant> confParams )
{
    QnMutexLocker lk( &m_mutex );
    m_confParams = std::move( confParams );
}

}   //ec2
