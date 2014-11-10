#ifndef __ACTI_RESOURCE_H__
#define __ACTI_RESOURCE_H__

#ifdef ENABLE_ACTI

#include <QtCore/QMap>
#include <QtCore/QMutex>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "utils/common/request_param.h"
#include "utils/common/timermanager.h"
#include "utils/network/http/asynchttpclient.h"
#include "utils/network/simple_http_client.h"
#include <utils/network/http/multipartcontentparser.h>


class QnActiPtzController;

class QnActiResource
:
    public QnPhysicalCameraResource,
    public TimerEventHandler
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    static const int MAX_STREAMS = 2;

    QnActiResource();
    ~QnActiResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool isInitialized() const;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;
    virtual bool hasDualStreaming() const override;
    virtual int getMaxFps() const override;

    QString getRtspUrl(int actiChannelNum) const; // in range 1..N

    /*!
        \param localAddress If not NULL, filled with local ip address, used to connect to camera
    */
    QByteArray makeActiRequest(const QString& group, const QString& command, CLHttpStatus& status, bool keepAllData = false, QString* const localAddress = NULL) const;
    QSize getResolution(Qn::ConnectionRole role) const;
    int roundFps(int srcFps, Qn::ConnectionRole role) const;
    int roundBitrate(int srcBitrateKbps) const;

    bool isAudioSupported() const;
    virtual QnAbstractPtzController *createPtzControllerInternal() override;

    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getRelayOutputList() const override;
    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getInputPortList() const override;
    //!Implementation of QnSecurityCamResource::setRelayOutputState
    /*!
        Actual request is performed asynchronously. This method only posts task to the queue
    */
    virtual bool setRelayOutputState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;
    //!Implementation of TimerEventHandler::onTimer
    virtual void onTimer( const quint64& timerID );

    static QByteArray unquoteStr(const QByteArray& value);
    QMap<QByteArray, QByteArray> parseSystemInfo(const QByteArray& report) const;

    //!Called by http server on receiving message from camera
    void cameraMessageReceived( const QString& path, const QnRequestParamList& message );

    static void setEventPort(int eventPort);
    static int eventPort();

protected:
    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    //!Implementation of QnSecurityCamResource::startInputPortMonitoring
    virtual bool startInputPortMonitoring() override;
    //!Implementation of QnSecurityCamResource::stopInputPortMonitoring
    virtual void stopInputPortMonitoring() override;
    //!Implementation of QnSecurityCamResource::isInputPortMonitored
    virtual bool isInputPortMonitored() const override;

private:
    QSize extractResolution(const QByteArray& resolutionStr) const;
    QList<QSize> parseResolutionStr(const QByteArray& resolutions);
    QList<int> parseVideoBitrateCap(const QByteArray& bitrateCap) const;
    void initializePtz();
    void initializeIO( const QMap<QByteArray, QByteArray>& systemInfo );
    bool isRtspAudioSupported(const QByteArray& platform, const QByteArray& firmware) const;

private:
    class TriggerOutputTask
    {
    public:
        //!Starts with 1. 0 - invalid
        int outputID;
        bool active;
        unsigned int autoResetTimeoutMS;

        TriggerOutputTask()
        :
            outputID( 0 ),
            active( false ),
            autoResetTimeoutMS( 0 )
        {
        }

        TriggerOutputTask(
            const int _outputID,
            const bool _active,
            const unsigned int _autoResetTimeoutMS )
        :
            outputID( _outputID ),
            active( _active ),
            autoResetTimeoutMS( _autoResetTimeoutMS )
        {
        }
    };

    class ActiResponse
    {
    public:
        //!map<parameter name, param fields including empty ones>
        std::map<QByteArray, std::vector<QByteArray> > params;
    };


    QSize m_resolution[MAX_STREAMS]; // index 0 for primary, index 1 for secondary
    QList<int> m_availFps[MAX_STREAMS];
    QList<int> m_availBitrate;
    int m_rtspPort;
    bool m_hasAudio;
    QByteArray m_platform;
    QMutex m_dioMutex;
    std::map<quint64, TriggerOutputTask> m_triggerOutputTasks;
    int m_outputCount;
    int m_inputCount;
    bool m_inputMonitored;
};

#endif // #ifdef ENABLE_ACTI
#endif // __ACTI_RESOURCE_H__
