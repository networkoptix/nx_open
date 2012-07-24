static const int TCP_READ_BUFFER_SIZE = 65536;
static const QByteArray ENDL("\r\n");

#include <QHttpRequestHeader>
#include "tcp_connection_processor.h"
#include "utils/common/bytearray.h"

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

static const int CODE_OK = 200;
static const int CODE_AUTH_REQUIRED = 401;
static const int CODE_NOT_FOUND = 404;
static const int CODE_INVALID_PARAMETER = 451;
static const int CODE_NOT_IMPLEMETED = 501;
static const int CODE_INTERNAL_ERROR = 500;

class QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
public:
    //enum State {State_Stopped, State_Paused, State_Playing, State_Rewind};

    QnTCPConnectionProcessorPrivate():
        socket(0),
        sendBuffer(CL_MEDIA_ALIGNMENT, 1024*256)
    {
        tcpReadBuffer = new quint8[TCP_READ_BUFFER_SIZE];
        socketTimeout = 5000 * 1000;
    }

    virtual ~QnTCPConnectionProcessorPrivate()
    {
        delete socket;
        delete [] tcpReadBuffer;
    }

public:
    TCPSocket* socket;
    QHttpRequestHeader requestHeaders;
    QHttpResponseHeader responseHeaders;

    QByteArray protocol;
    QByteArray requestBody;
    QByteArray responseBody;
    QByteArray clientRequest;
    QByteArray receiveBuffer;
    QMutex sockMutex;
    quint8* tcpReadBuffer;
    QnTcpListener* owner;
    CLByteArray sendBuffer;
    int socketTimeout;
};
