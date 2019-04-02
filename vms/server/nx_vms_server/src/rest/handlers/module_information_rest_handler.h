#pragma once

#include <common/common_module_aware.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/rest/handler.h>
#include <nx/utils/safe_direct_connection.h>

class QnCommonModule;

class QnModuleInformationRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ QnCommonModuleAware,
    public /*mixin*/ Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    QnModuleInformationRestHandler(QnCommonModule* commonModule);
    virtual ~QnModuleInformationRestHandler() override;

    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual void afterExecute(
        const nx::network::rest::Request& request,
        const nx::network::rest::Response& response) override;

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
