/**********************************************************
* 12 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SOAPSERVER_H
#define SOAPSERVER_H

#include <string>

#include <plugins/resources/onvif/onvif_notification_consumer.h>
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
    //!One MUST call \a initialize after instanciation with this constructor
    QnSoapServer();
    /*!
        \param port Port to listen on
    */
    QnSoapServer( unsigned int port, const char* path = "/services" );
    virtual ~QnSoapServer();

    //!Implementation of QnSoapServer::pleaseStop
    virtual void pleaseStop();

    void initialize( unsigned int port, const char* path = "/services" );
    bool initialized() const;
    //!Port being listened
    unsigned int port() const;
    //!Base path for incoming requests (e.g. /services)
    QString path() const;
    OnvifNotificationConsumer* getService();
    const OnvifNotificationConsumer* getService() const;

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

#endif  //SOAPSERVER_H
