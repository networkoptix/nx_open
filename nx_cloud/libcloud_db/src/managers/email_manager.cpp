/**********************************************************
* Aug 12, 2015
* a.kolesnikov
***********************************************************/

#include "email_manager.h"

#include <api/global_settings.h>
#include <mustache/mustache_helper.h>
#include <utils/common/cpp14.h>
#include <utils/common/log.h>
#include <utils/email/email_manager_impl.h>

#include "settings.h"


namespace nx {
namespace cdb {


EMailManager::EMailManager( const conf::Settings& settings )
:
    m_settings( settings ),
    m_terminated( false )
{
    m_sendEmailThread = std::make_unique<std::thread>(
        std::bind( &EMailManager::sedMailThreadFunc, this ) );
}

EMailManager::~EMailManager()
{
    {
        QnMutexLocker lk( &m_mutex );
        m_terminated = true;
    }
    m_taskQueue.push( SendEmailTask() );
    m_sendEmailThread->join();
}

bool EMailManager::sendEmailAsync(
    const QString& to,
    const QString& subject,
    const QString& body,
    std::function<void( bool )> completionHandler )
{
    Q_ASSERT( !to.isEmpty() );
    if( to.isEmpty() )
        return false;   //bad email address

    SendEmailTask task;
    task.email.to.push_back( to );
    task.email.subject = subject;
    task.email.body = body;
    task.completionHandler = std::move( completionHandler );
    m_taskQueue.push( std::move( task ) );
    return true;
}

bool EMailManager::renderAndSendEmailAsync(
    const QString& to,
    const QString& templateFileName,
    const QVariantHash& emailParams,
    std::function<void( bool )> completionHandler )
{
    QString emailToSend;
    if( !renderTemplateFromFile(
            templateFileName,
            emailParams,
            &emailToSend ) )
    {
        NX_LOG( lit("EMailManager. Failed to render confirmation email to send to %1").
            arg( to ), cl_logWARNING );
        return false;
    }
    return sendEmailAsync(
        std::move(to),
        templateFileName, //TODO #ak correct (and customizable) subject
        std::move( emailToSend ),
        std::move( completionHandler ) );
}

void EMailManager::sedMailThreadFunc()
{
    EmailManagerImpl emailSender;

    for( ;; )
    {
        SendEmailTask task;
        if( !m_taskQueue.pop( task ) )
            continue;
        if( m_terminated )
            break;

        //sending email
        const bool result = emailSender.sendEmail(
            m_settings.email(),
            task.email );
        if( task.completionHandler )
            task.completionHandler( result );
    }
}


}   //cdb
}   //nx
