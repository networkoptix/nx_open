#ifndef CONFIGURE_REST_HANDLER_H
#define CONFIGURE_REST_HANDLER_H

#include <rest/server/json_rest_handler.h>
#include <core/resource_access/user_access_data.h>

namespace ec2 {
    class QnTransactionMessageBusBase;
}

struct ConfigureSystemData;
class QnConfigureRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    QnConfigureRestHandler(ec2::QnTransactionMessageBusBase* messageBus);

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    int execute(const ConfigureSystemData& data, QnJsonRestResult &result, const QnRestConnectionProcessor* owner);
    int changePort(const QnRestConnectionProcessor* owner, int port);
private:
    ec2::QnTransactionMessageBusBase* m_messageBus;
};

#endif // CONFIGURE_REST_HANDLER_H
