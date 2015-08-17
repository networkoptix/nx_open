/**********************************************************
* Aug 12, 2015
* a.kolesnikov
***********************************************************/

#include "email_manager.h"


namespace nx {
namespace cdb {


EMailManager::EMailManager( const conf::Settings& settings )
:
    m_settings( settings )
{
}

void EMailManager::sendEmailAsync(
    const QString& /*emailAddress*/,
    const QString& /*emailBody*/ )
{
    //TODO #ak
}


}   //cdb
}   //nx
