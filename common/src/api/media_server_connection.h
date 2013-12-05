#ifndef QN_MEDIA_SERVER_CONNECTION_H
#define QN_MEDIA_SERVER_CONNECTION_H

#include <QtCore/QUrl>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtGui/QVector3D>
#include <QtGui/QRegion>

#include <utils/camera/camera_diagnostics.h>
#include <utils/common/id.h>

#include <api/api_fwd.h>
#include <api/model/camera_diagnostics_reply.h>
#include <api/model/storage_space_reply.h>
#include <api/model/storage_status_reply.h>
#include <api/model/statistics_reply.h>
#include <api/model/time_reply.h>
#include <api/model/rebuild_archive_reply.h>
#include <api/model/manual_camera_seach_reply.h>

#include <core/ptz/ptz_preset.h>
#include <core/resource/resource_fwd.h>
#include <business/business_fwd.h>
#include <recording/time_period_list.h>

#include "abstract_connection.h"

class QnMediaServerResource;

typedef QList<QPair<QString, bool> > QnStringBoolPairList;
typedef QList<QPair<QString, QVariant> > QnStringVariantPairList;

Q_DECLARE_METATYPE(QnStringBoolPairList);
Q_DECLARE_METATYPE(QnStringVariantPairList);


class QnMediaServerReplyProcessor: public QnAbstractReplyProcessor {
    Q_OBJECT

public:
    QnMediaServerReplyProcessor(int object): QnAbstractReplyProcessor(object) {}

    virtual void processReply(const QnHTTPRawResponse &response, int handle) override;

signals:
    void finished(int status, const QnRebuildArchiveReply &reply, int handle);
    void finished(int status, const QnStorageStatusReply &reply, int handle);
    void finished(int status, const QnStorageSpaceReply &reply, int handle);
    void finished(int status, const QnTimePeriodList &reply, int handle);
    void finished(int status, const QnStatisticsReply &reply, int handle);
    void finished(int status, const QVector3D &reply, int handle);
    void finished(int status, const QnStringVariantPairList &reply, int handle);
    void finished(int status, const QnStringBoolPairList &reply, int handle);
    void finished(int status, const QnTimeReply &reply, int handle);
    void finished(int status, const QnCameraDiagnosticsReply &reply, int handle);
    void finished(int status, const QnManualCameraSearchReply &reply, int handle);
    void finished(int status, const QnBusinessActionDataListPtr &reply, int handle);
    void finished(int status, const QImage &reply, int handle);
    void finished(int status, const QnPtzPreset &reply, int handle);
    void finished(int status, const QnPtzPresetList &reply, int handle);

private:
    friend class QnAbstractReplyProcessor;
};


enum RebuildAction
{
    RebuildAction_ShowProgress,
    RebuildAction_Start,
    RebuildAction_Cancel
};

class QnMediaServerConnection: public QnAbstractConnection {
    Q_OBJECT
    typedef QnAbstractConnection base_type;

public:
    QnMediaServerConnection(QnMediaServerResource* mserver, QObject *parent = NULL);
    virtual ~QnMediaServerConnection();

    void setProxyAddr(const QUrl &apiUrl, const QString &addr, int port);
    int getProxyPort() { return m_proxyPort; }
    QString getProxyHost() { return m_proxyAddr; }

    int getTimePeriodsAsync(const QnNetworkResourceList &list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QList<QRegion> &motionRegions, QObject *target, const char *slot);


    enum RoundMethod { IFrameBeforeTime, Precise, IFrameAfterTime };
	/** 
     * Get \a camera thumbnail for specified time. 
     * 
     * Returns immediately. On request completion \a slot of object \a target 
     * is called with signature <tt>(int status, QImage reply, int handle)</tt>.
     * Status is 0 in case of success, in other cases it holds error code.
     * 
     * \param camera
     * \param timeUsec                  Requested time in usecs. Can be DATE_TIME_NOW for live video or -1 for request latest available image.
     * \param size                      Can be filled partially: only width or height. In this case other dimension is auto detected.
     * \param imageFormat               Can be 'jpeg', 'tiff', 'png', etc...
     * \param method                    If parameter is 'before' or 'after' server returns nearest I-frame before or after time.
     * \param target
     * \param slot
     * \returns                         Request handle.
	 */
    int getThumbnailAsync(const QnNetworkResourcePtr &camera, qint64 timeUsec, const QSize& size, const QString& imageFormat, RoundMethod method, QObject *target, const char *slot);

	/** 
     * Get \a camera params. 
     * 
     * Returns immediately. On request completion \a slot of object \a target 
     * is called with signature <tt>(int httpStatusCode, const QList<QPair<QString, QVariant> > &params)</tt>.
     * \a status is 0 in case of success, in other cases it holds error code 
     * 
     * \returns                         Request handle.
	 */
    int getParamsAsync(const QnNetworkResourcePtr &camera, const QStringList &keys, QObject *target, const char *slot);

	/** 
     * Get \a event log. 
     * 
     * Returns immediately. On request completion \a slot of object \a target 
     * is called with signature <tt>(int handle, int httpStatusCode, const QList<QnAbstractBusinessAction> &events)</tt>.
     * \a status is 0 in case of success, in other cases it holds error code 
     * 
     * \param dateFrom                  Start timestamp in msec.
     * \param dateTo                    End timestamp in msec. Can be <tt>DATETIME_NOW</tt>.
     * \param cameras                   Filter events by camera. Optional.
     * \param businessRuleId            Filter events by specified business rule. Optional.
     * 
     * \returns                         Request handle.
	 */
    int getEventLogAsync(
        qint64 dateFrom, 
        qint64 dateTo, 
        QnResourceList cameras,
        BusinessEventType::Value eventType, 
        BusinessActionType::Value actionType,
        QnId businessRuleId, 
        QObject *target, 
        const char *slot);

    /**
     * \returns                         Http response status (200 in case of success).
     */
    int getParamsSync(const QnNetworkResourcePtr &camera, const QStringList &keys, QnStringVariantPairList *reply);

	/** 
	 * Set \a camera params.
	 * 
	 * Returns immediately. On request completion \a slot of object \a target is 
	 * called with signature <tt>(int httpStatusCode, const QList<QPair<QString, bool> > &operationResult)</tt>
	 * \a status is 0 in case of success, in other cases it holds error code
	 * 
     * \returns                         Request handle.
	 */
    int setParamsAsync(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QObject *target, const char *slot);

    /**
     * \returns                         Http response status (200 in case of success).
     */
    int setParamsSync(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params, QnStringBoolPairList *reply);

    /** 
     * \returns                         Request handle. 
     */
    int getStatisticsAsync(QObject *target, const char *slot);

    int searchCameraAsyncStart(const QString &startAddr, const QString &endAddr, const QString &username, const QString &password, int port, QObject *target, const char *slot);
    int searchCameraAsyncStatus(const QUuid &processUuid, QObject *target, const char *slot);
    int searchCameraAsyncStop(const QUuid &processUuid, QObject *target = NULL, const char *slot = NULL);

    int addCameraAsync(const QStringList &urls, const QStringList &manufacturers, const QString &username, const QString &password, QObject *target, const char *slot);

    int ptzContinuousMoveAsync(const QnNetworkResourcePtr &camera, const QVector3D &speed, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int ptzAbsoluteMoveAsync(const QnNetworkResourcePtr &camera, Qn::PtzCoordinateSpace space, const QVector3D &position, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int ptzRelativeMoveAsync(const QnNetworkResourcePtr &camera, qreal aspectRatio, const QRectF &viewport, const QUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int ptzGetPositionAsync(const QnNetworkResourcePtr &camera, Qn::PtzCoordinateSpace space, QObject *target, const char *slot);

    int ptzCreatePresetAsync(const QnNetworkResourcePtr &camera, const QnPtzPreset &preset, QObject *target, const char *slot);
    int ptzRemovePresetAsync(const QnNetworkResourcePtr &camera, const QnPtzPreset &preset, QObject *target, const char *slot);
    int ptzActivatePresetAsync(const QnNetworkResourcePtr &camera, const QnPtzPreset &preset, QObject *target, const char *slot);
    int ptzGetPresetsAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);

    int getStorageSpaceAsync(QObject *target, const char *slot);

    int getStorageStatusAsync(const QString &storageUrl, QObject *target, const char *slot);

    int getTimeAsync(QObject *target, const char *slot);

    //!Request server to run camera \a cameraID diagnostics step following \a previousStep
    /*!
        \param slot Slot MUST have signature (int, QnCameraDiagnosticsReply, int)
        \returns Request handle
    */
    int doCameraDiagnosticsStepAsync(
        const QnId& cameraID, CameraDiagnostics::Step::Value previousStep,
        QObject* target, const char* slot );

    /**
        \param slot Slot MUST have signature (int, QnRebuildArchiveReply, int)
        \returns Request handle
     */
    int doRebuildArchiveAsync(RebuildAction action, QObject *target, const char *slot);

protected:
    virtual QnAbstractReplyProcessor *newReplyProcessor(int object) override;

    static QnRequestParamList createTimePeriodsRequest(const QnNetworkResourceList &list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QList<QRegion> &motionRegions);
    static QnRequestParamList createGetParamsRequest(const QnNetworkResourcePtr &camera, const QStringList &params);
    static QnRequestParamList createSetParamsRequest(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params);

private:
    QString m_proxyAddr;
    int m_proxyPort;
};


#endif // __VIDEO_SERVER_CONNECTION_H_
