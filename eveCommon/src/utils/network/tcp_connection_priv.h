static const int TCP_READ_BUFFER_SIZE = 65536;
static const QString ENDL("\r\n");

#include <QHttpRequestHeader>

static const int CODE_OK = 200;
static const int CODE_NOT_FOUND = 404;
static const int CODE_INVALID_PARAMETER = 451;
static const int CODE_NOT_IMPLEMETED = 501;
static const int CODE_INTERNAL_ERROR = 500;

class QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
public:
    //enum State {State_Stopped, State_Paused, State_Playing, State_Rewind};

    QnTCPConnectionProcessorPrivate():
          socket(0)
      {
          tcpReadBuffer = new quint8[TCP_READ_BUFFER_SIZE];
      }

      ~QnTCPConnectionProcessorPrivate()
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
      QMutex sockMutex;
      quint8* tcpReadBuffer;
      QnTcpListener* owner;
};
