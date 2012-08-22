#ifndef __RTSP_CONNECTION_H_
#define __RTSP_CONNECTION_H_

#include <QHostAddress>
#include <QMutex>
#include "utils/network/ffmpeg_sdp.h"
#include "utils/network/tcp_connection_processor.h"
#include "core/resource/media_resource.h"
#include "core/datapacket/mediadatapacket.h"
#include "rtsp_encoder.h"

class QnAbstractStreamDataProvider;

class QnRtspConnectionProcessor: public QnTCPConnectionProcessor
{
    Q_OBJECT
public:
    QnRtspConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnRtspConnectionProcessor();
    qint64 getRtspTime();
    void setRtspTime(qint64 time);
    void switchToLive();
    QnMediaResourcePtr getResource() const;
    bool isLiveDP(QnAbstractStreamDataProvider* dp);

    void setQuality(MediaQuality quality);
    bool isSecondaryLiveDP(QnAbstractStreamDataProvider* provider) const;
    bool isPrimaryLiveDP(QnAbstractStreamDataProvider* dp) const;
    bool isSecondaryLiveDPSupported() const;
    QHostAddress getPeerAddress() const;
    QString getRangeHeaderIfChanged();
    int getMetadataTcpChannel() const;
    int getAVTcpChannel(int trackNum) const;
    QnRtspEncoderPtr getCodecEncoder(int trackNum) const;
    UDPSocket* getMediaSocket(int trackNum) const;
protected:
    virtual void run();
    void addResponseRangeHeader();
    QString getRangeStr();
private slots:
    void at_cameraDisabledChanged(bool oldValue, bool newValue);
    void at_cameraUpdated();
private:
    void checkQuality();
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
    void putLastIFrameToQueue();
    void connectToLiveDataProviders();
    //QnAbstractMediaStreamDataProvider* getLiveDp();
    void setQualityInternal(MediaQuality quality);
    QnRtspEncoderPtr createEncoderByMediaData(QnAbstractMediaDataPtr media);
    QnAbstractMediaDataPtr getCameraData(QnAbstractMediaData::DataType dataType);
private:
    QN_DECLARE_PRIVATE_DERIVED(QnRtspConnectionProcessor);
    friend class QnRtspDataConsumer;
};

#endif
