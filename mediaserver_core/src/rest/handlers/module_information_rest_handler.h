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

    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner) override;

    virtual void afterExecute(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QnRestConnectionProcessor* owner);

private slots:
    void changeModuleInformation();

private:
    void updateModuleImformation();
    void sendModuleImformation(const QSharedPointer<AbstractStreamSocket>& socket);
    void sendKeepAliveByTimer(const QSharedPointer<AbstractStreamSocket>& socket);

private:
    nx::network::aio::BasicPollable m_pollable;
    std::set<QSharedPointer<AbstractStreamSocket>> m_socketsToKeepOpen;

    QnCommonModule* m_commonModule = nullptr;
    QByteArray m_moduleInformatiom;
    std::set<QSharedPointer<AbstractStreamSocket>> m_socketsToUpdate;
};
