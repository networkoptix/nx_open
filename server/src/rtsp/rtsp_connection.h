#include "common/base.h"
#include "network/socket.h"
#include "common/longrunnable.h"

class QnRtspConnectionProcessor: public QnLongRunnable
{
public:
    QnRtspConnectionProcessor(TCPSocket* socket);
    virtual ~QnRtspConnectionProcessor();

    qint64 getRtspTime();
protected:
    virtual void run();
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
    inline void sendData(const QByteArray& data)
    { sendData(data.constData(), data.size()); }
    void sendData(const char* data, int size);
    QString extractMediaName(const QString& path);
    int extractTrackId(const QString& path);
    int composeTeardown();
    void processRangeHeader();
    void extractNptTime(const QString& strValue, qint64* dst);
    int composeSetParameter();
    int composeGetParameter();
    QMutex& getSockMutex();
private:
    QN_DECLARE_PRIVATE(QnRtspConnectionProcessor);
    friend class QnRtspDataProcessor;
};
