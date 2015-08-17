/**********************************************************
* Aug 12, 2015
* a.kolesnikov
***********************************************************/

#include "email_manager.h"

#include <mustache/mustache_helper.h>
#include <utils/common/log.h>


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

void EMailManager::renderAndSendEmailAsync(
    const QString& emailAddress,
    const QString& templateFileName,
    const QVariantHash& emailParams )
{
    //TODO #ak rendering should also happen in different thread

    QString emailToSend;
    if( !renderTemplateFromFile(
            templateFileName,
            emailParams,
            &emailToSend ) )
    {
        NX_LOG( lit("EMailManager. Failed to render confirmation email to send to %1").
            arg( emailAddress ), cl_logWARNING );
        return;
    }
    sendEmailAsync(
        emailAddress,
        emailToSend );
}


}   //cdb
}   //nx
