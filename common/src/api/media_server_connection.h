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
#include <core/ptz/ptz_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <business/business_fwd.h>

#include "abstract_connection.h"

typedef QList<QPair<QString, bool> > QnStringBoolPairList;
typedef QList<QPair<QString, QVariant> > QnStringVariantPairList;

Q_DECLARE_METATYPE(QnStringBoolPairList);
Q_DECLARE_METATYPE(QnStringVariantPairList);


// TODO: #MSAPI move to api/model or even to common_globals, 
// add lexical serialization (see QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS)
// 
// Check serialization/deserialization in QnMediaServerConnection::doRebuildArchiveAsync
// and in handler.
// 
// And name them sanely =)
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
    QnMediaServerConnection(QnMediaServerResource* mserver, const QnUuid& videowallGuid = QnUuid(), QObject *parent = NULL);
    virtual ~QnMediaServerConnection();

    int getTimePeriodsAsync(
        const QnNetworkResourceList &list,
        qint64 startTimeMs, 
        qint64 endTimeMs, 
        qint64 detail, 
        Qn::TimePeriodContent periodsType,
        const QString &filter,
        QObject *target, 
        const char *slot);

    // TODO: #MSAPI 
    // move to api/model or even to common_globals, 
    // add lexical serialization (see QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS)
    // 
    // And name them sanely =)
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
     * Check \a list of cameras for discovery. Return new list with contains only accessible cameras.
     * 
     * Returns immediately. On request completion \a slot of object \a target 
     * is called with signature <tt>(int status, QImage reply, int handle)</tt>.
     * Status is 0 in case of success, in other cases it holds error code.
     * 
     * \param cameras
     * \param target
     * \param slot
     * \returns                         Request handle.
     */
    int checkCameraList(const QnNetworkResourceList &cameras, QObject *target, const char *slot);

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
        QnBusiness::EventType eventType, 
        QnBusiness::ActionType actionType,
        QnUuid businessRuleId, 
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
    int searchCameraAsyncStatus(const QnUuid &processUuid, QObject *target, const char *slot);
    int searchCameraAsyncStop(const QnUuid &processUuid, QObject *target = NULL, const char *slot = NULL);

    int addCameraAsync(const QStringList &urls, const QStringList &manufacturers, const QString &username, const QString &password, QObject *target, const char *slot);

    int ptzContinuousMoveAsync(const QnNetworkResourcePtr &camera, const QVector3D &speed, const QnUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int ptzContinuousFocusAsync(const QnNetworkResourcePtr &camera, qreal speed, QObject *target, const char *slot);
    int ptzAbsoluteMoveAsync(const QnNetworkResourcePtr &camera, Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed, const QnUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int ptzViewportMoveAsync(const QnNetworkResourcePtr &camera, qreal aspectRatio, const QRectF &viewport, qreal speed, const QnUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot);
    int ptzGetPositionAsync(const QnNetworkResourcePtr &camera, Qn::PtzCoordinateSpace space, QObject *target, const char *slot);

    int ptzCreatePresetAsync(const QnNetworkResourcePtr &camera, const QnPtzPreset &preset, QObject *target, const char *slot);
    int ptzUpdatePresetAsync(const QnNetworkResourcePtr &camera, const QnPtzPreset &preset, QObject *target, const char *slot);
    int ptzRemovePresetAsync(const QnNetworkResourcePtr &camera, const QString &presetId, QObject *target, const char *slot);
    int ptzActivatePresetAsync(const QnNetworkResourcePtr &camera, const QString &presetId, qreal speed, QObject *target, const char *slot);
    int ptzGetPresetsAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);

    int ptzCreateTourAsync(const QnNetworkResourcePtr &camera, const QnPtzTour &tour, QObject *target, const char *slot);
    int ptzRemoveTourAsync(const QnNetworkResourcePtr &camera, const QString &tourId, QObject *target, const char *slot);
    int ptzActivateTourAsync(const QnNetworkResourcePtr &camera, const QString &tourId, QObject *target, const char *slot);
    int ptzGetToursAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);

    int ptzGetActiveObjectAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);
    int ptzUpdateHomeObjectAsync(const QnNetworkResourcePtr &camera, const QnPtzObject &homePosition, QObject *target, const char *slot);
    int ptzGetHomeObjectAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);

    int ptzGetAuxilaryTraitsAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot);
    int ptzRunAuxilaryCommandAsync(const QnNetworkResourcePtr &camera, const QnPtzAuxilaryTrait &trait, const QString &data, QObject *target, const char *slot);

    int ptzGetDataAsync(const QnNetworkResourcePtr &camera, Qn::PtzDataFields query, QObject *target, const char *slot);

    int getStorageSpaceAsync(QObject *target, const char *slot);

    int getStorageStatusAsync(const QString &storageUrl, QObject *target, const char *slot);

    int getTimeAsync(QObject *target, const char *slot);

    //!Requests name of system, mediaserver is currently connected to
    /*!
        \param slot Slot MUST have signature (int, QString, int)
    */
    int getSystemNameAsync( QObject* target, const char* slot );
    //!Request server to run camera \a cameraID diagnostics step following \a previousStep
    /*!
        \param slot Slot MUST have signature (int, QnCameraDiagnosticsReply, int)
        \returns Request handle
    */
    int doCameraDiagnosticsStepAsync(
        const QnUuid& cameraID, CameraDiagnostics::Step::Value previousStep,
        QObject* target, const char* slot );

    /**
        \param slot Slot MUST have signature (int, QnRebuildArchiveReply, int)
        \returns Request handle
     */
    int doRebuildArchiveAsync(RebuildAction action, QObject *target, const char *slot);

    int addBookmarkAsync(const QnNetworkResourcePtr &camera, const QnCameraBookmark &bookmark, QObject *target, const char *slot);
    int updateBookmarkAsync(const QnNetworkResourcePtr &camera, const QnCameraBookmark &bookmark, QObject *target, const char *slot);
    int deleteBookmarkAsync(const QnNetworkResourcePtr &camera, const QnCameraBookmark &bookmark, QObject *target, const char *slot);
    int getBookmarksAsync(const QnNetworkResourcePtr &camera, const QnCameraBookmarkSearchFilter &filter, QObject *target, const char *slot);

    int installUpdate(const QString &updateId, const QByteArray &data, QObject *target, const char *slot);

    int restart(QObject *target, const char *slot);

    int configureAsync(bool wholeSystem, const QString &systemName, const QString &password, const QByteArray &passwordHash, const QByteArray &passwordDigest, int port, QObject *target, const char *slot);

    int pingSystemAsync(const QUrl &url, const QString &password, QObject *target, const char *slot);
    int mergeSystemAsync(const QUrl &url, const QString &password, bool ownSettings, QObject *target, const char *slot);

    int testEmailSettingsAsync(const QnEmail::Settings &settings, QObject *target, const char *slot);
protected:
    virtual QnAbstractReplyProcessor *newReplyProcessor(int object) override;

    static QnRequestParamList createGetParamsRequest(const QnNetworkResourcePtr &camera, const QStringList &params);
    static QnRequestParamList createSetParamsRequest(const QnNetworkResourcePtr &camera, const QnStringVariantPairList &params);

private:
    QString m_proxyAddr;
    int m_proxyPort;
};


#endif // __VIDEO_SERVER_CONNECTION_H_
