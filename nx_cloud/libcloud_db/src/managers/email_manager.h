/**********************************************************
* Aug 12, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_EMAIL_MANAGER_H
#define NX_CLOUD_DB_EMAIL_MANAGER_H

#include <functional>

#include <QtCore/QString>


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

    //!Adds email to the internal queue and returns. Email will be sent as soon as possible
    void sendEmailAsync(
        const QString& emailAddress,
        const QString& emailBody,
        std::function<void( bool )> completionHandler );
    void renderAndSendEmailAsync(
        const QString& emailAddress,
        const QString& templateFileName,
        const QVariantHash& emailParams,
        std::function<void(bool)> completionHandler );

private:
    const conf::Settings& m_settings;
};


}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_EMAIL_MANAGER_H
