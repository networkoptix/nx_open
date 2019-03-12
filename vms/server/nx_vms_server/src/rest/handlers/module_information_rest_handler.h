#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <rest/server/json_rest_handler.h>
#include "nx/utils/safe_direct_connection.h"
#include <common/common_module_aware.h>

class QnCommonModule;

class QnModuleInformationRestHandler:
    public QnJsonRestHandler,
    public /*mixin*/ QnCommonModuleAware,
    public /*mixin*/ Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    QnModuleInformationRestHandler(QnCommonModule* commonModule);
    virtual ~QnModuleInformationRestHandler() override;

    virtual JsonRestResponse executeGet(const JsonRestRequest& request) override;
    virtual void afterExecute(const RestRequest& request, const QByteArray& response) override;

private slots:
    void changeModuleInformation();

private:
    struct Connection
    {
        std::unique_ptr<nx::network::AbstractStreamSocket> socket;
        std::chrono::milliseconds updateInterval;
        nx::Buffer dataToSend;
        bool isSendInProgress = false;
    };
    using Connections = std::list<Connection>;

    static void clear(Connections* connections);

    virtual int executeGet(
        const QString& /*path*/,
        const QnRequestParams& /*params*/,
        QnJsonRestResult& /*result*/,
        const QnRestConnectionProcessor* /*owner*/);

    void updateModuleImformation();
    void send(Connections::iterator socket, nx::Buffer buffer = {});
    void sendModuleImformation(Connections::iterator connection);
    void sendKeepAliveByTimer(Connections::iterator connection, bool firstUpdate = true);

private:
    nx::network::aio::BasicPollable m_pollable;
    QByteArray m_moduleInformatiom;
    Connections m_connectionsToKeepOpened;
    Connections m_connectionsToUpdate;

    QnMutex m_mutex;
    bool m_aboutToStop = false;

private:
};
