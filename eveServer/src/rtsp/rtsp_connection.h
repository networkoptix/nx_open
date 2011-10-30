#ifndef __RTSP_CONNECTION_H_
#define __RTSP_CONNECTION_H_

#include <QMutex>
#include "utils/network/ffmpeg_sdp.h"
#include "utils/network/tcp_connection_processor.h"

class QnRtspConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnRtspConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnRtspConnectionProcessor();
    qint64 getRtspTime();
    void setRtspTime(qint64 time);
protected:
    virtual void run();
private:
    void processRequest();
    void parseRequest();
    void initResponse(int code = 200, const QString& message = "OK");
    void generateSessionId();
    void sendResponse(int code);

    int numOfVideoChannels();
    int composeDescribe();
    int composeSetup();
    int composePlay();
    int composePause();
    void parseRangeHeader(const QString& rangeStr, qint64* startTime, qint64* endTime);
    int extractTrackId(const QString& path);
    int composeTeardown();
    void processRangeHeader();
    void extractNptTime(const QString& strValue, qint64* dst);
    int composeSetParameter();
    int composeGetParameter();
    void createDataProvider();
private:
    QN_DECLARE_PRIVATE(QnRtspConnectionProcessor);
    friend class QnRtspDataConsumer;
};

#endif
