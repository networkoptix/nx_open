#include "default_tcp_connection_processor.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"

QnDefaultTcpConnectionProcessor::QnDefaultTcpConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner): 
    QnTCPConnectionProcessor(socket)
{

}

void QnDefaultTcpConnectionProcessor::run()
{
    Q_D(QnTCPConnectionProcessor);
    initSystemThreadId();

    d->responseBody.clear();
    d->responseBody.append("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
    d->responseBody.append("<html lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\">\n");
    d->responseBody.append("<head>\n");
    d->responseBody.append("<b>Requested path is absent.\n");
    d->responseBody.append("</head>\n");
    d->responseBody.append("<body>\n");
    d->responseBody.append("</body>\n");
    d->responseBody.append("</html>\n");

    sendResponse(CODE_NOT_FOUND, "text/html");
}
