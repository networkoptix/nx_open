/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_PROCESS_H
#define CLOUD_DB_PROCESS_H

#include <memory>

#include <QtCore/QSettings>

#include <qtsinglecoreapplication.h>
#include <qtservice.h>

#include <utils/common/stoppable.h>
#include <utils/network/connection_server/multi_address_server.h>
#include <utils/network/http/server/http_stream_socket_server.h>


class CloudDBProcess
:
    public QtService<QtSingleCoreApplication>,
    public QnStoppable
{
public:
    CloudDBProcess( int argc, char **argv );

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

protected:
    virtual int executeApplication() override;
    virtual void start() override;
    virtual void stop() override;

private:
    std::unique_ptr<QSettings> m_settings;
    int m_argc;
    char** m_argv;
    std::unique_ptr<MultiAddressServer<nx_http::HttpStreamSocketServer>> m_multiAddressHttpServer;

    void initializeLogging(
        const QString& dataLocation,
        const QString& logLevel );
    QString getDataDirectory();
    int printHelp();
};

#endif  //HOLE_PUNCHER_SERVICE_H
