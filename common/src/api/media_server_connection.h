#pragma once

#include <QtCore/QUrl>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtGui/QVector3D>
#include <QtGui/QRegion>

#include <api/helpers/request_helpers_fwd.h>
#include <api/helpers/thumbnail_request_data.h> //< For RoundMethod, delete when /api/image is removed.

#include <common/common_globals.h>

#include <utils/camera/camera_diagnostics.h>
#include <utils/common/id.h>

#include <core/ptz/ptz_fwd.h>
#include <utils/common/ldap_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/event/event_fwd.h>

#include "abstract_connection.h"
#include "model/manual_camera_seach_reply.h"

class QnMediaServerConnection: public QnAbstractConnection
{
    Q_OBJECT
    typedef QnAbstractConnection base_type;

public:
    QnMediaServerConnection(
        QnCommonModule* commonModule,
        const QnMediaServerResourcePtr& mserver,
        const QnUuid& videowallGuid = QnUuid(),
        bool enableOfflineRequests = false);

    virtual ~QnMediaServerConnection();

    int getTimePeriodsAsync(
        const QnVirtualCameraResourcePtr& camera,
        qint64 startTimeMs,
        qint64 endTimeMs,
        qint64 detail,
        Qn::TimePeriodContent periodsType,
        const QString& filter,
        QObject* target,
        const char* slot);

    /**
     * Check the list of cameras for discovery. Forms a new list which contains only accessible
     * cameras.
     *
     * Returns immediately. On request completion the specified slot of the specified target is
     * called with signature <tt>(int status, QImage reply, int handle)</tt>.
     * Status is 0 in case of success, in other cases it holds error code.
     *
     * @return Request handle.
     */
    int checkCameraList(const QnNetworkResourceList& cameras, QObject* target, const char* slot);

    /**
     * Get camera params.
     *
     * Returns immediately. On request completion the specified slot of the specified target is
     * called with signature <tt>(int httpStatusCode, const QList<QPair<QString, QVariant>>& params)</tt>.
     * Status is 0 in case of success, in other cases it holds error code.
     *
	 * @param keys List of parameter ids that are requested.
     * @return Request handle.
     */
    int getParamsAsync(const QnNetworkResourcePtr& camera, const QStringList& keys,
        QObject* target, const char* slot);

	/**
     * Set camera params.
     *
     * Returns immediately. On request completion the specified slot of the specified target is
     * called with signature <tt>(int httpStatusCode, const QList<QPair<QString, bool>>& operationResult)</tt>
     * Status is 0 in case of success, in other cases it holds error code.
     *
     * @return Request handle.
     */
    int setParamsAsync(const QnNetworkResourcePtr& camera,
        const QnCameraAdvancedParamValueList& params, QObject* target, const char* slot);

    /**
     * @return Request handle.
     */
    int getStatisticsAsync(QObject* target, const char* slot);

    int searchCameraAsyncStart(
        const QString& startAddr, const QString& endAddr, const QString& username,
        const QString& password, int port, QObject* target, const char* slot);
    int searchCameraAsyncStatus(const QnUuid& processUuid, QObject* target, const char* slot);
    int searchCameraAsyncStop(const QnUuid& processUuid, QObject* target = nullptr,
        const char* slot = nullptr );

    int addCameraAsync(
        const QnManualResourceSearchList& cameras, const QString& username,
        const QString& password, QObject* target, const char* slot);

    int ptzContinuousMoveAsync(const QnNetworkResourcePtr& camera,
        const QVector3D& speed, const QnUuid& sequenceId, int sequenceNumber, QObject* target,
        const char* slot);
    int ptzContinuousFocusAsync(const QnNetworkResourcePtr& camera,
        qreal speed, QObject* target, const char* slot);
    int ptzAbsoluteMoveAsync(const QnNetworkResourcePtr& camera,
        Qn::PtzCoordinateSpace space, const QVector3D& position, qreal speed,
        const QnUuid& sequenceId, int sequenceNumber, QObject* target, const char* slot);
    int ptzViewportMoveAsync(const QnNetworkResourcePtr& camera,
        qreal aspectRatio, const QRectF& viewport, qreal speed, const QnUuid& sequenceId,
        int sequenceNumber, QObject* target, const char* slot);
    int ptzGetPositionAsync(const QnNetworkResourcePtr& camera,
        Qn::PtzCoordinateSpace space, QObject* target, const char* slot);
    int ptzCreatePresetAsync(const QnNetworkResourcePtr& camera,
        const QnPtzPreset& preset, QObject* target, const char* slot);
    int ptzUpdatePresetAsync(const QnNetworkResourcePtr& camera,
        const QnPtzPreset& preset, QObject* target, const char* slot);
    int ptzRemovePresetAsync(const QnNetworkResourcePtr& camera,
        const QString& presetId, QObject* target, const char* slot);
    int ptzActivatePresetAsync(const QnNetworkResourcePtr& camera,
        const QString& presetId, qreal speed, QObject* target, const char* slot);
    int ptzGetPresetsAsync(const QnNetworkResourcePtr& camera,
        QObject* target, const char* slot);
    int ptzCreateTourAsync(const QnNetworkResourcePtr& camera,
        const QnPtzTour& tour, QObject* target, const char* slot);
    int ptzRemoveTourAsync(const QnNetworkResourcePtr& camera,
        const QString& tourId, QObject* target, const char* slot);
    int ptzActivateTourAsync(const QnNetworkResourcePtr& camera,
        const QString& tourId, QObject* target, const char* slot);
    int ptzGetToursAsync(const QnNetworkResourcePtr& camera,
        QObject* target, const char* slot);
    int ptzGetActiveObjectAsync(const QnNetworkResourcePtr& camera,
        QObject* target, const char* slot);
    int ptzUpdateHomeObjectAsync(const QnNetworkResourcePtr& camera,
        const QnPtzObject& homePosition, QObject* target, const char* slot);
    int ptzGetHomeObjectAsync(const QnNetworkResourcePtr& camera,
        QObject* target, const char* slot);
    int ptzGetAuxilaryTraitsAsync(const QnNetworkResourcePtr& camera,
        QObject* target, const char* slot);
    int ptzRunAuxilaryCommandAsync(const QnNetworkResourcePtr& camera,
        const QnPtzAuxilaryTrait& trait, const QString& data, QObject* target, const char* slot);
    int ptzGetDataAsync(const QnNetworkResourcePtr& camera,
        Qn::PtzDataFields query, QObject* target, const char* slot);

    /**
     * @param fastRequest Request information about existing storages only. Getting full info may
     *     be quite slow.
     * @return information about storages space.
     */
    int getStorageSpaceAsync(bool fastRequest, QObject* target, const char* slot);

    int getStorageStatusAsync(const QString& storageUrl, QObject* target, const char* slot);

    int getTimeAsync(QObject* target, const char* slot);
    int mergeLdapUsersAsync(QObject* target, const char* slot);

    /**
     * Request the name of a system the mediaserver is currently connected to.
     * @param slot Slot MUST have signature (int, QString, int).
     * @return Request handle. -1 In case of failure to start async request.
     */
    int getSystemIdAsync(QObject* target, const char* slot);

    /**
     * Request the server to run the camera diagnostics step following previousStep.
     * @param slot Slot MUST have signature (int, QnCameraDiagnosticsReply, int).
     * @return Request handle.
     */
    int doCameraDiagnosticsStepAsync(
        const QnUuid& cameraId, CameraDiagnostics::Step::Value previousStep,
        QObject* target, const char* slot );

    /**
     * @param slot Slot MUST have signature (int, QnStorageScanData, int).
     * @return Request handle. -1 In case of failure to start async request.
     */
    int doRebuildArchiveAsync(
        Qn::RebuildAction action, bool isMainPool, QObject* target, const char* slot);

    int backupControlActionAsync(Qn::BackupAction action, QObject* target, const char* slot);

    int acknowledgeEventAsync(
        const QnCameraBookmark& bookmark,
        const QnUuid& eventRuleId,
        QObject* target,
        const char* slot);

    int addBookmarkAsync(
        const QnCameraBookmark& bookmark,
        QObject* target,
        const char* slot);

    int updateBookmarkAsync(const QnCameraBookmark& bookmark, QObject* target, const char* slot);
    int deleteBookmarkAsync(const QnUuid& bookmarkId, QObject* target, const char* slot);

    int installUpdate(const QString& updateId, bool delayed, QObject* target, const char* slot);
    int installUpdateUnauthenticated(
        const QString& updateId, bool delayed, QObject* target, const char* slot);
    int uploadUpdateChunk(const QString& updateId,
        const QByteArray& data, qint64 offset, QObject* target, const char* slot);

    int restart(QObject* target, const char* slot);

    int configureAsync(
        bool wholeSystem, const QString& systemName, const QString& password,
        const QByteArray& passwordHash, const QByteArray& passwordDigest,
        const QByteArray& cryptSha512Hash, int port, QObject* target, const char* slot);

    int pingSystemAsync(const nx::utils::Url &url, const QString& getKey, QObject* target, const char* slot);
    int getNonceAsync(const nx::utils::Url& url, QObject* target, const char* slot);
    int getRecordingStatisticsAsync(
        qint64 bitrateAnalizePeriodMs, QObject* target, const char* slot);
    int getAuditLogAsync(qint64 startTimeMs, qint64 endTimeMs, QObject* target, const char* slot);
    int mergeSystemAsync(const nx::utils::Url &url, const QString& getKey, const QString& postKey, bool ownSettings,
        bool oneServer, bool ignoreIncompatible, QObject* target, const char* slot);

    int testEmailSettingsAsync(const QnEmailSettings& settings, QObject* target, const char* slot);
    int testLdapSettingsAsync(const QnLdapSettings& settings, QObject* target, const char* slot);

    int modulesInformation(QObject* target, const char* slot);

    int cameraHistory(const QnChunksRequestData& request, QObject* target, const char* slot);

    int recordedTimePeriods(const QnChunksRequestData& request, QObject* target, const char* slot);
    int getBookmarksAsync(
        const QnGetBookmarksRequestData& request, QObject* target, const char* slot);
    int getBookmarkTagsAsync(
        const QnGetBookmarkTagsRequestData& request, QObject* target, const char* slot);

protected:
    virtual QnAbstractReplyProcessor* newReplyProcessor(int object, const QString& serverId) override;
    virtual bool isReady() const override;

    int sendAsyncGetRequestLogged(
        int object,
        const QnRequestParamList& params,
        const char* replyTypeName,
        QObject* target,
        const char* slot);

    int sendAsyncPostRequestLogged(
        int object,
        nx_http::HttpHeaders headers,
        const QnRequestParamList& params,
        const QByteArray& data,
        const char* replyTypeName,
        QObject* target,
        const char* slot);

    void trace(int handle, int obj, const QString& message = QString());

private:
    void addOldVersionPtzParams(const QnNetworkResourcePtr& camera, QnRequestParamList& params);

private:
    QnSoftwareVersion m_serverVersion;
    QString m_serverId; // for debug purposes so storing in string to avoid conversions
    QString m_proxyAddr;
    int m_proxyPort;
    bool m_enableOfflineRequests;
};
