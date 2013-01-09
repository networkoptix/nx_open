#ifndef axis_resource_h_2215
#define axis_resource_h_2215

#include <QMap>
#include <QMutex>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "utils/network/http/asynchttpclient.h"
#include "utils/network/simple_http_client.h"
#include <utils/network/http/multipartcontentparser.h>


class QnPlAxisResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const char* MANUFACTURE;

    QnPlAxisResource();
    ~QnPlAxisResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool isInitialized() const;

    virtual bool shoudResolveConflicts() const override;

    QByteArray getMaxResolution() const;
    QString getNearestResolution(const QByteArray& resolution, float aspectRatio) const;
    float getResolutionAspectRatio(const QByteArray& resolution) const;

    QRect getMotionWindow(int num) const;
    QMap<int, QRect>  getMotionWindows() const;
    bool readMotionInfo();

    virtual void setMotionMaskPhysical(int channel) override;
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider) override;

    int getChannelNum() const;

    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getRelayOutputList() const override;
    //!Implementation of QnSecurityCamResource::getRelayOutputList
    virtual QStringList getInputPortList() const override;
    //!Implementation of QnSecurityCamResource::setRelayOutputState
    virtual bool setRelayOutputState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;

public slots:
    void onMonitorResponseReceived( nx_http::AsyncHttpClient* httpClient );
    void onMonitorMessageBodyAvailable( nx_http::AsyncHttpClient* httpClient );
    void onMonitorConnectionClosed( nx_http::AsyncHttpClient* httpClient );

protected:
    bool initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
    virtual bool startInputPortMonitoring() override;
    virtual void stopInputPortMonitoring() override;

private:
    void clear();
    static QRect axisRectToGridRect(const QRect& axisRect);
    static QRect gridRectToAxisRect(const QRect& gridRect);

    bool removeMotionWindow(int wndNum);
    int addMotionWindow();
    bool updateMotionWindow(int wndNum, int sensitivity, const QRect& rect);
    int toAxisMotionSensitivity(int sensitivity);

private:
    QList<QByteArray> m_resolutionList;
    bool m_palntscRes;

    QMap<int, QRect> m_motionWindows;
    QMap<int, QRect> m_motionMask;
    qint64 m_lastMotionReadTime;
    //!map<name, index based on 1>
    std::map<QString, unsigned int> m_inputPortNameToIndex;
    std::map<QString, unsigned int> m_outputPortNameToIndex;
    mutable QMutex m_inputPortMutex;
    std::map<unsigned int, nx_http::AsyncHttpClient*> m_inputPortHttpMonitor;
    nx_http::MultipartContentParser m_multipartContentParser;
    nx_http::BufferType m_currentMonitorData;

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
    void forgetHttpClient( nx_http::AsyncHttpClient* const httpClient );
};

#endif //axis_resource_h_2215
