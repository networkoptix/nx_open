/**********************************************************
* Sep 21, 2015
* a.kolesnikov
***********************************************************/

#ifndef SAVE_CLOUD_SYSTEM_CREDENTIALS_H
#define SAVE_CLOUD_SYSTEM_CREDENTIALS_H

#include "rest/server/json_rest_handler.h"


struct CloudCredentialsData;
class CloudConnectionManager;

class QnSaveCloudSystemCredentialsHandler
:
    public QnJsonRestHandler
{
    Q_OBJECT

public:
    QnSaveCloudSystemCredentialsHandler(const CloudConnectionManager& cloudConnectionManager);

    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor*);

    int execute(const CloudCredentialsData& data, QnJsonRestResult& result);

private:
    const CloudConnectionManager& m_cloudConnectionManager;
};

#endif  //SAVE_CLOUD_SYSTEM_CREDENTIALS_H
