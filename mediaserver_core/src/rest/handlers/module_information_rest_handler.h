#pragma once

#include <nx/network/abstract_socket.h>
#include <rest/server/json_rest_handler.h>

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
    void closeAllSockets();
    void removeSocket(const QSharedPointer<AbstractStreamSocket>& socket);

private:
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    std::set<QSharedPointer<AbstractStreamSocket>> m_savedSockets;
};
