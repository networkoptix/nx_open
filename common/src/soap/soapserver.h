/**********************************************************
* 12 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SOAPSERVER_H
#define SOAPSERVER_H

#ifdef ENABLE_ONVIF

#include <string>

#include <plugins/resource/onvif/onvif_notification_consumer.h>
#include <utils/common/long_runnable.h>


//!Listenes for incoming soap requests. Implemented using gsoap
/*!
    \todo make class service-independent
*/
class QnSoapServer
:
    public QnLongRunnable
{
public:
    /*!
        \param port Port to listen on. If zero, random avalable port is chosen
    */
    QnSoapServer( unsigned int port = 0, const char* path = "/services" );
    virtual ~QnSoapServer();

    //!Implementation of QnSoapServer::pleaseStop
    virtual void pleaseStop();

    bool bind();
    bool initialized() const;
    //!Port being listened
    unsigned int port() const;
    //!Base path for incoming requests (e.g. /services)
    QString path() const;
    OnvifNotificationConsumer* getService();
    const OnvifNotificationConsumer* getService() const;

    static void initStaticInstance( QnSoapServer* inst );
    static QnSoapServer* instance();

protected:
    virtual void run();

private:
    unsigned int m_port;
    std::string m_path;
    bool m_terminated;
    OnvifNotificationConsumer m_service;
    bool m_initialized;
};

#endif //ENABLE_ONVIF

#endif  //SOAPSERVER_H
