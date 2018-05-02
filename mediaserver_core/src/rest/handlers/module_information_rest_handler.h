#pragma once

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <rest/server/json_rest_handler.h>

class QnCommonModule;

class QnModuleInformationRestHandler: public QnJsonRestHandler
{
    Q_OBJECT

public:
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
    void sendModuleImformation(const QSharedPointer<nx::network::AbstractStreamSocket>& socket);
    void sendKeepAliveByTimer(const QSharedPointer<nx::network::AbstractStreamSocket>& socket);

private:
    nx::network::aio::BasicPollable m_pollable;
    std::set<QSharedPointer<nx::network::AbstractStreamSocket>> m_socketsToKeepOpen;

    QnCommonModule* m_commonModule = nullptr;
    QByteArray m_moduleInformatiom;
    std::set<QSharedPointer<nx::network::AbstractStreamSocket>> m_socketsToUpdate;
};
