#ifndef __RTSP_CONNECTION_H_
#define __RTSP_CONNECTION_H_

#include <QHostAddress>
#include <QMutex>
#include "utils/network/ffmpeg_sdp.h"
#include "utils/network/tcp_connection_processor.h"
#include "core/resource/media_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "rtsp_encoder.h"

class QnAbstractStreamDataProvider;

struct RtspServerTrackInfo
{
    RtspServerTrackInfo(): clientPort(-1), clientRtcpPort(0), sequence(0), firstRtpTime(-1), mediaSocket(0), rtcpSocket(0) 
    {

    }
    ~RtspServerTrackInfo()
    {
        delete mediaSocket;
        delete rtcpSocket;
    }

    bool openServerSocket(const QString& peerAddress);

    quint32 getSSRC() const {
        return encoder ? encoder->getSSRC() : 0;
    }

    int clientPort;
    int clientRtcpPort;
    quint16 sequence;
    qint64 firstRtpTime;
    UDPSocket* mediaSocket;
    UDPSocket* rtcpSocket;
    QnRtspEncoderPtr encoder;
    static QMutex m_createSocketMutex;
};
typedef QSharedPointer<RtspServerTrackInfo> RtspServerTrackInfoPtr;
typedef QMap<int, RtspServerTrackInfoPtr> ServerTrackInfoMap;

class QnRtspConnectionProcessorPrivate;

class QnRtspConnectionProcessor: public QnTCPConnectionProcessor
{
    Q_OBJECT
public:
    QnRtspConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnRtspConnectionProcessor();
    qint64 getRtspTime();
    void setRtspTime(qint64 time);
    void resetTrackTiming();
    bool isTcpMode() const;
    QnMediaResourcePtr getResource() const;
    bool isLiveDP(QnAbstractStreamDataProvider* dp);

    void setQuality(MediaQuality quality);
    bool isSecondaryLiveDPSupported() const;
    QHostAddress getPeerAddress() const;
    QString getRangeHeaderIfChanged();
    int getMetadataChannelNum() const;
    int getAVTcpChannel(int trackNum) const;
    //QnRtspEncoderPtr getCodecEncoder(int trackNum) const;
    //UDPSocket* getMediaSocket(int trackNum) const;
    RtspServerTrackInfoPtr getTrackInfo(int trackNum) const;
    int getTracksCount() const;

protected:
    virtual void run();
    void addResponseRangeHeader();
    QString getRangeStr();

private slots:
    void at_camera_disabledChanged();
    void at_camera_resourceChanged();

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
    QnRtspEncoderPtr createEncoderByMediaData(QnAbstractMediaDataPtr media, QSize resolution, const QnResourceVideoLayout* vLayout);
    QnAbstractMediaDataPtr getCameraData(QnAbstractMediaData::DataType dataType);
    static int isFullBinaryMessage(const QByteArray& data);
    void processBinaryRequest();
    void createPredefinedTracks(const QnResourceVideoLayout* videoLayout);

private:
    Q_DECLARE_PRIVATE(QnRtspConnectionProcessor);
    friend class QnRtspDataConsumer;
};

#endif
