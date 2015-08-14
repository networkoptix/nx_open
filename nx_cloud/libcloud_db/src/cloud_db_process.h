/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_PROCESS_H
#define CLOUD_DB_PROCESS_H

#include <memory>

#include <qtsinglecoreapplication.h>
#include <qtservice.h>

#include <utils/common/stoppable.h>
#include <utils/db/db_manager.h>
#include <utils/network/connection_server/multi_address_server.h>
#include <utils/network/http/server/http_stream_socket_server.h>

#include "settings.h"


class QnCommandLineParser;

namespace nx_http
{
    class MessageDispatcher;
}

namespace nx {
namespace cdb {

class AccountManager;
class SystemManager;

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
    int m_argc;
    char** m_argv;
    std::unique_ptr<MultiAddressServer<nx_http::HttpStreamSocketServer>> m_multiAddressHttpServer;

    void initializeLogging( const conf::Settings& settings );
    void registerApiHandlers(
        nx_http::MessageDispatcher* const msgDispatcher,
        AccountManager* const accountManager,
        SystemManager* const systemManager );
    bool updateDB( nx::db::DBManager* const dbManager );
};

}   //cdb
}   //nx

#endif  //HOLE_PUNCHER_SERVICE_H
