/**********************************************************
* Sep 21, 2015
* a.kolesnikov
***********************************************************/

#ifndef SAVE_CLOUD_SYSTEM_CREDENTIALS_H
#define SAVE_CLOUD_SYSTEM_CREDENTIALS_H

#include "rest/server/json_rest_handler.h"


class QnSaveCloudSystemCredentialsHandler
:
    public QnJsonRestHandler
{
    Q_OBJECT

public:
    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*);
    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor*);
};

#endif  //SAVE_CLOUD_SYSTEM_CREDENTIALS_H
