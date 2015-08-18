/**********************************************************
* Aug 12, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_EMAIL_MANAGER_H
#define NX_CLOUD_DB_EMAIL_MANAGER_H

#include <functional>
#include <memory>
#include <thread>

#include <QtCore/QString>

#include <nx_ec/data/api_email_data.h>
#include <utils/common/threadqueue.h>
#include <utils/thread/mutex.h>


namespace nx {
namespace cdb {


namespace conf {
    class Settings;
}


//!Responsible for sending emails
class EMailManager
{
public:
    EMailManager( const conf::Settings& settings );
    virtual ~EMailManager();

    //!Adds email to the internal queue and returns. Email will be sent as soon as possible
    /*!
        \param completionHandler If not NULL will be called to forward async operation result
        \return true If started asynchronous email send operation
    */
    bool sendEmailAsync(
        const QString& to,
        const QString& subject,
        const QString& body,
        std::function<void(bool)> completionHandler );
    //!Renders email using \a templateFileName and \a emailParams and calls \a EMailManager::sendEmailAsync
    bool renderAndSendEmailAsync(
        const QString& to,
        const QString& templateFileName,
        const QVariantHash& emailParams,
        std::function<void(bool)> completionHandler );

private:
    struct SendEmailTask
    {
        ec2::ApiEmailData email;
        std::function<void( bool )> completionHandler;
    };
    
    const conf::Settings& m_settings;
    std::unique_ptr<std::thread> m_sendEmailThread;
    bool m_terminated;
    mutable QnMutex m_mutex;
    CLThreadQueue<SendEmailTask> m_taskQueue;

    void sedMailThreadFunc();
};


}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_EMAIL_MANAGER_H
