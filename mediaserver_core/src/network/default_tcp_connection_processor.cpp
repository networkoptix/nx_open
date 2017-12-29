#include "default_tcp_connection_processor.h"
#include "network/tcp_connection_priv.h"
#include "network/tcp_listener.h"

QnDefaultTcpConnectionProcessor::QnDefaultTcpConnectionProcessor(
    QSharedPointer<nx::network::AbstractStreamSocket> socket, 
    QnTcpListener* owner):
    QnTCPConnectionProcessor(socket, owner)
{
}

void QnDefaultTcpConnectionProcessor::run()
{
    Q_D(QnTCPConnectionProcessor);
    initSystemThreadId();

    d->response.messageBody.clear();
    d->response.messageBody.append("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
    d->response.messageBody.append("<html lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\">\n");
    d->response.messageBody.append("<head>\n");
    d->response.messageBody.append("<b>Requested path is absent.\n");
    d->response.messageBody.append("</head>\n");
    d->response.messageBody.append("<body>\n");
    d->response.messageBody.append("</body>\n");
    d->response.messageBody.append("</html>\n");

    sendResponse(CODE_NOT_FOUND, "text/html; charset=utf-8");
}
