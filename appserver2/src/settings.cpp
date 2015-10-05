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
    static const QString DB_READ_ONLY( lit("ecDbReadOnly") );
    static const bool DB_READ_ONLY_DEFAULT = false;
}


bool Settings::dbReadOnly() const
{
    //if booted from SD card, setting ecDbReadOnly to true by default
    bool defaultValue = params::DB_READ_ONLY_DEFAULT;
#ifdef __linux__
    if (QnAppInfo::armBox() == "bpi" || QnAppInfo::armBox() == "nx1")
    {
        defaultValue = Nx1::isBootedFromSD();
    }
#endif

    return value<bool>( params::DB_READ_ONLY, defaultValue );
}

void Settings::loadParams( std::map<QString, QVariant> confParams )
{
    QMutexLocker lk( &m_mutex );
    m_confParams = std::move( confParams );
}

}   //ec2
