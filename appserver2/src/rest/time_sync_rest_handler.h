/**********************************************************
* Aug 14, 2015
* a.kolesnikov
***********************************************************/

#ifndef TIME_SYNC_REST_HANDLER_H
#define TIME_SYNC_REST_HANDLER_H

#include "rest/server/request_handler.h"


namespace ec2 {

class Ec2DirectConnectionFactory;

class QnTimeSyncRestHandler: public QnRestRequestHandler
{
public:
    static const QString PATH;
    //!used to pass time sync information between peers
    static const QByteArray TIME_SYNC_HEADER_NAME;

    QnTimeSyncRestHandler(Ec2DirectConnectionFactory* connection);

    //!Implementation of \a QnRestRequestHandler::executeGet
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* ) override;
    //!Implementation of \a QnRestRequestHandler::executeGet
    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* ) override;
private:
    Ec2DirectConnectionFactory* m_appServerConnection;
};

}

#endif  //TIME_SYNC_REST_HANDLER_H
