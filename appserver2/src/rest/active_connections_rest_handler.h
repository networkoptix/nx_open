#pragma once

#include "rest/server/json_rest_handler.h"

namespace ec2
{
    class QnTransactionMessageBusBase;
}

class QnActiveConnectionsRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnActiveConnectionsRestHandler(const ec2::QnTransactionMessageBusBase* messageBus);

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    const ec2::QnTransactionMessageBusBase* m_messageBus;
};
