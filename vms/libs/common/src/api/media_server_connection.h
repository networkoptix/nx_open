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
#include <utils/common/ldap_fwd.h>
#include <utils/email/email_fwd.h>

#include <core/ptz/ptz_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/core/ptz/vector.h>
#include <nx/core/ptz/options.h>

#include "abstract_connection.h"
#include "model/manual_camera_seach_reply.h"

class QnMediaServerConnection: public QnAbstractConnection
{
    Q_OBJECT
    typedef QnAbstractConnection base_type;

public:
    QnMediaServerConnection(
        QnCommonModule* commonModule,
        const QnMediaServerResourcePtr& server,
        const QnUuid& videowallGuid = QnUuid(),
        bool enableOfflineRequests = false);

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

    int addCameraAsync(
        const QnManualResourceSearchList& cameras, const QString& username,
        const QString& password, QObject* target, const char* slot);

    /**
     * @param fastRequest Request information about existing storages only. Getting full info may
     *     be quite slow.
     * @return information about storages space.
     */
    int getStorageSpaceAsync(bool fastRequest, QObject* target, const char* slot);

    int getStorageStatusAsync(const QString& storageUrl, QObject* target, const char* slot);

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

    int pingSystemAsync(const nx::utils::Url &url, const QString& getKey, QObject* target, const char* slot);
    int getNonceAsync(const nx::utils::Url& url, QObject* target, const char* slot);
    int getRecordingStatisticsAsync(
        qint64 bitrateAnalyzePeriodMs, QObject* target, const char* slot);
    int getAuditLogAsync(qint64 startTimeMs, qint64 endTimeMs, QObject* target, const char* slot);

    int testEmailSettingsAsync(const QnEmailSettings& settings, QObject* target, const char* slot);
    int testLdapSettingsAsync(const QnLdapSettings& settings, QObject* target, const char* slot);
    int recordedTimePeriods(const QnChunksRequestData& request, QObject* target, const char* slot);

protected:
    virtual nx::utils::Url url() const override;
    virtual QnAbstractReplyProcessor* newReplyProcessor(int object, const QString& serverId) override;
    virtual bool isReady() const override;

    int sendAsyncGetRequestLogged(
        int object,
        const QnRequestParamList& params,
        const char* replyTypeName,
        QObject* target,
        const char* slot,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    int sendAsyncPostRequestLogged(
        int object,
        const QnRequestParamList& params,
        const char* replyTypeName,
        QObject* target,
        const char* slot,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    int sendAsyncPostRequestLogged(
        int object,
        nx::network::http::HttpHeaders headers,
        const QnRequestParamList& params,
        const QByteArray& data,
        const char* replyTypeName,
        QObject* target,
        const char* slot,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    void trace(int handle, int obj, const QString& message = QString());

private:
    QWeakPointer<QnMediaServerResource> m_server;
    nx::vms::api::SoftwareVersion m_serverVersion;
    QString m_serverId; // for debug purposes so storing in string to avoid conversions
    QString m_proxyAddr;
    int m_proxyPort;
    bool m_enableOfflineRequests;
};
