#ifndef __TCP_CONNECTION_PRIV_H__
#define __TCP_CONNECTION_PRIV_H__

#include <QDateTime>
#include <QtCore/QByteArray>


static const int TCP_READ_BUFFER_SIZE = 65536;

#include "tcp_connection_processor.h"

#include "utils/common/byte_array.h"
#include "utils/network/http/httptypes.h"


static const QByteArray STATIC_UNAUTHORIZED_HTML("\
    <!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\"http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd\">\
    <HTML>\
    <HEAD>\
    <TITLE>Error</TITLE>\
    <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=ISO-8859-1\">\
    </HEAD>\
    <BODY><H1>401 Unauthorized.</H1></BODY>\
    </HTML>"
);

static const QByteArray STATIC_PROXY_UNAUTHORIZED_HTML("\
    <!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\"http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd\">\
    <HTML>\
    <HEAD>\
    <TITLE>Error</TITLE>\
    <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=ISO-8859-1\">\
    </HEAD>\
    <BODY><H1>407 Proxy Unauthorized.</H1></BODY>\
    </HTML>"
);


// TODO: #vasilenko these are part of a public interface and are used throughout the codebase.
// Why they are in a private header???
static const int CODE_OK = 200;
static const int CODE_MOVED_PERMANENTLY = 301;
static const int CODE_NOT_MODIFIED = 304;
static const int CODE_AUTH_REQUIRED = 401;
static const int CODE_NOT_FOUND = 404;
static const int CODE_PROXY_AUTH_REQUIRED = 407;
static const int CODE_INVALID_PARAMETER = 451;
static const int CODE_UNSPOORTED_TRANSPORT = 461;
static const int CODE_NOT_IMPLEMETED = 501;
static const int CODE_INTERNAL_ERROR = 500;

class QnTCPConnectionProcessorPrivate
{
public:
    //enum State {State_Stopped, State_Paused, State_Playing, State_Rewind};

    QnTCPConnectionProcessorPrivate():
        socket(0),
        clientRequestOffset(0)
    {
        tcpReadBuffer = new quint8[TCP_READ_BUFFER_SIZE];
        socketTimeout = 5 * 1000;
    }

    virtual ~QnTCPConnectionProcessorPrivate()
    {
        delete [] tcpReadBuffer;
    }

public:
    QSharedPointer<AbstractStreamSocket> socket;
    nx_http::Request request;
    nx_http::Response response;

    QByteArray protocol;
    QByteArray requestBody;
    QByteArray responseBody;
    QByteArray clientRequest;
    QByteArray receiveBuffer;
    QMutex sockMutex;
    quint8* tcpReadBuffer;
    int socketTimeout;
    bool chunkedMode;
    int clientRequestOffset;
    QDateTime lastModified;
};

#endif // __TCP_CONNECTION_PRIV_H__
