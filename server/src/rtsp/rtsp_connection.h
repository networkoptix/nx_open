#include <QTcpSocket>

#include "utils/common/base.h"

class QnRtspConnectionProcessor: public QObject
{
    Q_OBJECT
public:
    QnRtspConnectionProcessor(QTcpSocket* socket);
    virtual ~QnRtspConnectionProcessor();
private slots:
    void onClientReadyRead();
    void onClientConnected();
    void onClientDisconnected();
private:
    bool isFullMessage();
    void processRequest();
    void parseRequest();
    void initResponse(int code = 200, const QString& message = "OK");
    void generateSessionId();
    void sendResponse();

    int numOfVideoChannels();
    int composeDescribe();
    int composeSetup();
    int composePlay();
    void sendData(const QByteArray& data);
    void sendData(const char* data, int size);
    QString extractMediaName(const QString& path);
    int extractTrackId(const QString& path);
    int composeTeardown();
private:
    QN_DECLARE_PRIVATE(QnRtspConnectionProcessor);
    friend class QnRtspDataProcessor;
};
