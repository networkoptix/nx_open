#include <QTcpSocket>

#include "common/base.h"

class QnRtspConnectionProcessor: public QObject
{
    Q_OBJECT
public:
    QnRtspConnectionProcessor(QTcpSocket* socket);
    virtual ~QnRtspConnectionProcessor();

    qint64 getRtspTime();
private slots:
    void onClientReadyRead();
    void onClientConnected();
    void onClientDisconnected();
private:
    bool isFullMessage();
    void processRequest();
    QString codeToMessage(int code);
    void parseRequest();
    void initResponse(int code = 200, const QString& message = "OK");
    void generateSessionId();
    void sendResponse();

    int numOfVideoChannels();
    int composeDescribe();
    int composeSetup();
    int composePlay();
    int composePause();
    void sendData(const QByteArray& data);
    void sendData(const char* data, int size);
    void flush();
    QString extractMediaName(const QString& path);
    int extractTrackId(const QString& path);
    int composeTeardown();
    void processRangeHeader();
    void extractNptTime(const QString& strValue, qint64* dst);
    int composeSetParameter();
    int composeGetParameter();
private:
    QN_DECLARE_PRIVATE(QnRtspConnectionProcessor);
    friend class QnRtspDataProcessor;
};
