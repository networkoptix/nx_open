#ifndef axis_resource_h_2215
#define axis_resource_h_2215

#ifdef ENABLE_AXIS

#include <QtCore/QMap>
#include <QtCore/QMutex>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "utils/network/http/asynchttpclient.h"
#include "utils/network/simple_http_client.h"
#include <utils/network/http/multipartcontentparser.h>

class QnAxisPtzController;

class QnPlAxisResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const int PRIMARY_ENCODER_INDEX = 0;
    static const int SECONDARY_ENCODER_INDEX = 1;

    struct AxisResolution
    {
        AxisResolution() {}
        AxisResolution(const QSize& _size, const QByteArray& _resolutionStr): size(_size), resolutionStr(_resolutionStr ) {}

        QSize size;
        QByteArray resolutionStr;
    };

    static const QString MANUFACTURE;

    QnPlAxisResource();
    ~QnPlAxisResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool isInitialized() const;

    AxisResolution getMaxResolution() const;
    AxisResolution getNearestResolution(const QSize& resolution, float aspectRatio) const;
    float getResolutionAspectRatio(const AxisResolution& resolution) const;

    QRect getMotionWindow(int num) const;
    QMap<int, QRect>  getMotionWindows() const;
    bool readMotionInfo();

    virtual void setMotionMaskPhysical(int channel) override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;

    virtual int getChannel() const override;

    int getChannelNumAxis() const; // depracated

    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getRelayOutputList() const override;
    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getInputPortList() const override;
    //!Implementation of QnSecurityCamResource::setRelayOutputState
    virtual bool setRelayOutputState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;

    virtual QnAbstractPtzController *createPtzControllerInternal() override;

    AxisResolution getResolution( int encoderIndex ) const;

public slots:
    void onMonitorResponseReceived( nx_http::AsyncHttpClientPtr httpClient );
    void onMonitorMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient );
    void onMonitorConnectionClosed( nx_http::AsyncHttpClientPtr httpClient );

protected:
    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCroppingPhysical(QRect cropping);
    virtual bool startInputPortMonitoring() override;
    virtual void stopInputPortMonitoring() override;
    virtual bool isInputPortMonitored() const override;

private:
    void clear();
    static QRect axisRectToGridRect(const QRect& axisRect);
    static QRect gridRectToAxisRect(const QRect& gridRect);

    bool removeMotionWindow(int wndNum);
    int addMotionWindow();
    bool updateMotionWindow(int wndNum, int sensitivity, const QRect& rect);
    int toAxisMotionSensitivity(int sensitivity);

private:
    QList<AxisResolution> m_resolutionList;

    QMap<int, QRect> m_motionWindows;
    QMap<int, QRect> m_motionMask;
    qint64 m_lastMotionReadTime;
    //!map<name, index based on 1>
    std::map<QString, unsigned int> m_inputPortNameToIndex;
    std::map<QString, unsigned int> m_outputPortNameToIndex;
    mutable QMutex m_inputPortMutex;
    //!http client used to monitor input port(s) state
    std::shared_ptr<nx_http::AsyncHttpClient> m_inputPortHttpMonitor;
    nx_http::MultipartContentParserHelper m_multipartContentParser;
    nx_http::BufferType m_currentMonitorData;
    AxisResolution m_resolutions[SECONDARY_ENCODER_INDEX+1];

    //!reads axis parameter, triggering url like http://ip/axis-cgi/param.cgi?action=list&group=Input.NbrOfInputs
    CLHttpStatus readAxisParameter(
        CLSimpleHTTPClient* const httpClient,
        const QString& paramName,
        QVariant* paramValue );
    CLHttpStatus readAxisParameter(
        CLSimpleHTTPClient* const httpClient,
        const QString& paramName,
        QString* paramValue );
    CLHttpStatus readAxisParameter(
        CLSimpleHTTPClient* const httpClient,
        const QString& paramName,
        unsigned int* paramValue );
    void initializeIOPorts( CLSimpleHTTPClient* const http );
    void notificationReceived( const nx_http::ConstBufferRefType& notification );

    friend class QnAxisPtzController;
};

#endif // #ifdef ENABLE_AXIS
#endif //axis_resource_h_2215
