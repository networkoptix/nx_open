#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <rest/server/json_rest_handler.h>
#include "nx/utils/safe_direct_connection.h"
#include <common/common_module_aware.h>

class QnCommonModule;

class QnModuleInformationRestHandler:
    public QnJsonRestHandler,
    public QnCommonModuleAware,
    public Qn::EnableSafeDirectConnection
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
    virtual int executeGet(
        const QString& /*path*/,
        const QnRequestParams& /*params*/,
        QnJsonRestResult& /*result*/,
        const QnRestConnectionProcessor* /*owner*/);

    void updateModuleImformation();
    void sendModuleImformation(nx::network::AbstractStreamSocket* socket);
    void sendKeepAliveByTimer(nx::network::AbstractStreamSocket* socket);

private:
    using Socket = nx::network::AbstractStreamSocket;
    using SocketList = std::map<Socket*, std::unique_ptr<Socket>>;

    nx::network::aio::BasicPollable m_pollable;
    SocketList m_socketsToKeepOpen;

    QByteArray m_moduleInformatiom;
    SocketList m_socketsToUpdate;

    QnMutex m_mutex;
    bool m_aboutToStop = false;
private:
    static void clearSockets(SocketList* sockets);
};
