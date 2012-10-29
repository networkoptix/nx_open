#include "exec_action_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "events/abstract_business_action.h"

QnExecActionHandler::QnExecActionHandler() {
}

QnExecActionHandler::~QnExecActionHandler() 
{

}

int QnExecActionHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType)
{
    Q_UNUSED(params)
    Q_UNUSED(path)

    resultByteArray.append("<root>\n");
    resultByteArray.append("Invalid method 'GET'. You should use 'POST' instead\n");
    resultByteArray.append("</root>\n");

    return CODE_NOT_IMPLEMETED;
}

int QnExecActionHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    QnAbstractBusinessActionPtr action = QnAbstractBusinessAction::fromByteArray(body);
    return executeGet(path, params, result, contentType);
}

QString QnExecActionHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "Execute business action. Action specified in POST request body at binary protobuf format. \n";
    return rez;
}
