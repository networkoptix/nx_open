#ifndef axis_resource_h_2215
#define axis_resource_h_2215

#ifdef ENABLE_AXIS

#include <QtCore/QMap>
#include <set>
#include <nx/utils/thread/mutex.h>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "nx/streaming/media_data_packet.h"
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/simple_http_client.h>
#include <api/model/api_ioport_data.h>
#include <nx/network/http/multipart_content_parser.h>
#include <core/resource/camera_advanced_param.h>

class QnAxisPtzController;
typedef std::shared_ptr<QnAbstractAudioTransmitter> QnAudioTransmitterPtr;

class QnPlAxisResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    typedef QnPhysicalCameraResource base_type;

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

    //!Implementation of QnNetworkResource::checkIfOnlineAsync
    virtual void checkIfOnlineAsync( std::function<void(bool)> completionHandler ) override;

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

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

    virtual bool setRelayOutputState(
        const QString& ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;

    virtual QnAbstractPtzController *createPtzControllerInternal() override;

    virtual bool getParamPhysical(const QString &id, QString &value) override;
    virtual bool getParamsPhysical(const QSet<QString> &idList, QnCameraAdvancedParamValueList& result) override;
    virtual bool setParamPhysical(const QString &id, const QString& value) override;
    virtual bool setParamsPhysical(const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result) override;

    AxisResolution getResolution( int encoderIndex ) const;
    virtual QnIOStateDataList ioStates() const override;

    virtual QnAudioTransmitterPtr getAudioTransmitter() override;
public slots:
    void onMonitorResponseReceived( nx_http::AsyncHttpClientPtr httpClient );
    void onMonitorMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient );
    void onMonitorConnectionClosed( nx_http::AsyncHttpClientPtr httpClient );

    void onCurrentIOStateResponseReceived( nx_http::AsyncHttpClientPtr httpClient );

    void at_propertyChanged(const QnResourcePtr & /*res*/, const QString & key);
protected:
    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCroppingPhysical(QRect cropping);
    virtual bool startInputPortMonitoringAsync( std::function<void(bool)>&& completionHandler ) override;
    virtual void stopInputPortMonitoringAsync() override;
    virtual bool isInputPortMonitored() const override;

private:
    void clear();
    static QRect axisRectToGridRect(const QRect& axisRect);
    static QRect gridRectToAxisRect(const QRect& gridRect);

    bool removeMotionWindow(int wndNum);
    int addMotionWindow();
    bool updateMotionWindow(int wndNum, int sensitivity, const QRect& rect);
    int toAxisMotionSensitivity(int sensitivity);
    void asyncUpdateIOSettings();
    bool readCurrentIOStateAsync();
private:
    QList<AxisResolution> m_resolutionList;

    QMap<int, QRect> m_motionWindows;
    QMap<int, QRect> m_motionMask;
    qint64 m_lastMotionReadTime;
    //!map<name, index based on 1>
    //std::map<QString, unsigned int> m_inputPortNameToIndex;
    //std::map<QString, unsigned int> m_outputPortNameToIndex;
    QnIOPortDataList m_ioPorts;
    QnIOStateDataList m_ioStates;
    mutable QnMutex m_inputPortMutex;
    //!http client used to monitor input port(s) state

    struct IOMonitor {
        nx_http::AsyncHttpClientPtr httpClient;
        std::shared_ptr<nx_http::MultipartContentParser> contentParser;
    };

    IOMonitor m_ioHttpMonitor[2];
    nx_http::AsyncHttpClientPtr m_inputPortStateReader;
    QVector<QString> m_ioPortIdList;


    nx_http::AsyncHttpClientPtr m_inputPortHttpMonitor;
    nx_http::BufferType m_currentMonitorData;
    AxisResolution m_resolutions[SECONDARY_ENCODER_INDEX+1];
    QnAudioTransmitterPtr m_audioTransmitter;

    std::set<nx_http::AsyncHttpClientPtr> m_stoppingHttpClients;
    QnWaitCondition m_stopInputMonitoringWaitCondition;

    QnCameraAdvancedParams m_advancedParameters;

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
    CLHttpStatus readAxisParameters(const QString& rootPath, CLSimpleHTTPClient* const httpClient, QList<QPair<QByteArray,QByteArray>>& params);
    CLHttpStatus readAxisParameters(const QString& rootPath, CLSimpleHTTPClient* const httpClient, QMap<QString, QString>& params);

    bool enableDuplexMode() const;

    bool initialize2WayAudio(CLSimpleHTTPClient* const http);
    bool initializeIOPorts( CLSimpleHTTPClient* const http );
    void notificationReceived( const nx_http::ConstBufferRefType& notification );
    bool readPortSettings( CLSimpleHTTPClient* const http, QnIOPortDataList& ioPorts);
    bool savePortSettings(const QnIOPortDataList& newPorts, const QnIOPortDataList& oldPorts);
    QnIOPortDataList mergeIOSettings(const QnIOPortDataList& cameraIO, const QnIOPortDataList& savedIO);
    bool ioPortErrorOccured();
    void updateIOState(const QString& portId, bool isActive, qint64 timestamp, bool overrideIfExist);
    bool startIOMonitor(Qn::IOPortType portType, IOMonitor& result);
    void resetHttpClient(nx_http::AsyncHttpClientPtr& value);

    /*!
        Convert port number to ID
    */
    QString portIndexToId(int num) const;

    /*!
        Convert port ID to relative num in range [0..N)
    */
    int portIdToIndex(const QString& id) const;

    /*!
        Convert strings like port1, IO1, I1, O1 to port num in range [0..)
    */
    int portDisplayNameToIndex(const QString& id) const;

    /*!
        Convert port number to request param. Request params are numbers in range [1..N]
    */
    QString portIndexToReqParam(int number) const;

    void readPortIdLIst();

    QList<QnCameraAdvancedParameter> getParamsByIds(const QSet<QString>& idList) const;

    /**
     * Example of advanced parameter ranges overload via resource_data.json
     *
     *    {
     *       "keys": ["Axis|AXISM1031W"],
     *       "advancedParameterOverloads": [
     *           {
     *               "paramId": "root.AudioSource.A0.SampleRate",
     *               "dependencyId" : null,
     *               "internalRange": "1000,2000,3000,4000",
     *               "range": "1,2,3,4"
     *           },
     *           {
     *               "paramId": "root.AudioSource.A0.SampleRate",
     *               "dependencyId" : "pcmRange",
     *               "internalRange": "1000,2000",
     *               "range": "1,2"
     *           },
     *           {
     *               "paramId": "root.AudioSource.A0.BitRate",
     *               "dependencyId": "g726Range",
     *               "range": "1,2,3",
     *               "internalRange": "100,200,300"
     *           }
     *       ]
     *   }
     */
    void fetchAndSetAdvancedParameters();

    QString getAdvancedParametersTemplate() const;
    bool loadAdvancedParametersTemplateFromFile(QnCameraAdvancedParams& params, const QString& filename);
    QSet<QString> calculateSupportedAdvancedParameters(const QnCameraAdvancedParams& allParams);


    QString getParamCmd(const QnCameraAdvancedParameter& param) const;
    QSet<QString> buildGetParamsQueries(const QList<QnCameraAdvancedParameter>& params) const;
    QString buildSetParamsQuery(const QnCameraAdvancedParamValueList& params) const;
    QString buildMaintenanceQuery(const QnCameraAdvancedParamValueList& params) const;

    QMap<QString, QString> executeParamsQueries(const QSet<QString>& queries, bool& isSuccessful) const;
    QMap<QString, QString> executeParamsQueries(const QString& query, bool& isSuccessful) const;
    QnCameraAdvancedParamValueList parseParamsQueriesResult(
        const QMap<QString, QString>& queriesResult,
        const QList<QnCameraAdvancedParameter>& params) const;

    bool isMaintenanceParam(const QnCameraAdvancedParameter& param) const;

    friend class QnAxisPtzController;
    friend class AxisIOMessageBodyParser;
};

#endif // #ifdef ENABLE_AXIS
#endif //axis_resource_h_2215
