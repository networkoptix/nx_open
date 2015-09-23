/**********************************************************
* Aug 10, 2015
* a.kolesnikov
***********************************************************/

#include "settings.h"


namespace ec2 {

namespace params
{
    static const QString DB_READ_ONLY( lit("ecDbReadOnly") );
    static const bool DB_READ_ONLY_DEFAULT = false;
}


bool Settings::dbReadOnly() const
{
    return value<bool>( params::DB_READ_ONLY, params::DB_READ_ONLY_DEFAULT );
}

void Settings::loadParams( std::map<QString, QVariant> confParams )
{
    QMutexLocker lk( &m_mutex );
    m_confParams = std::move( confParams );
}

}   //ec2
