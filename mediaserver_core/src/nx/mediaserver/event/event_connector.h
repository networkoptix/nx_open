#pragma once

#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <server/server_globals.h>
#include <health/system_health.h>
#include <common/common_module_aware.h>
#include <nx/streaming/abstract_data_packet.h>
#include <nx/utils/singleton.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/event/events/events_fwd.h>

struct QnModuleInformation;

namespace nx {
namespace mediaserver {
namespace event {

/*
* This class listening various logic events, covert these events to business events and send it to businessRuleProcessor
*/

class EventConnector:
    public QObject,
    public QnCommonModuleAware,
    public Singleton<EventConnector>
{
    Q_OBJECT

public:
    EventConnector(QnCommonModule* commonModule);
    virtual ~EventConnector() override;

public slots:
    /*!
        \param metadata region where is motion occured
        \param value true, if motion started or motion in progress. false, if motion finished. If motion is finished metadata is null
    */
    void at_motionDetected(const QnResourcePtr& resource, bool value,
        qint64 timeStamp, const QnConstAbstractDataPacketPtr& metadata);

    /*! Camera goes to offline state
    */
    void at_cameraDisconnected(const QnResourcePtr& resource, qint64 timeStamp);

    /*! Some problem with storage
    */
    void at_storageFailure(const QnResourcePtr& server, qint64 timeStamp,
        vms::event::EventReason reasonCode, const QnResourcePtr& storage);
    void at_storageFailure(const QnResourcePtr& server, qint64 timeStamp,
        vms::event::EventReason reasonCode, const QString &storageUrl);

    /*! Some problem with network
    */
    void at_networkIssue(const QnResourcePtr& resource, qint64 timeStamp,
        vms::event::EventReason reasonCode, const QString& reasonParamsEncoded);

    /*!
        \param inputPortID device-specific ID of input port
        \param value true, if input activated. false, if deactivated
    */
    void at_cameraInput(const QnResourcePtr& resource, const QString& inputPortID,
        bool value, qint64 timeStampUsec);

    /** Some objects have been detected on the scene */
    void at_analyticsEventStart(
        const QnResourcePtr& resource,
        const QString& caption,
        const QString& description,
        qint64 timestamp);

    /** No objects detected on the scene */
    void at_analyticsEventEnd(
        const QnResourcePtr& resource,
        const QString& caption,
        const QString& description,
        qint64 timestamp);

    void at_customEvent(const QString& resourceName, const QString& caption,
        const QString& description, const vms::event::EventMetaData& metadata,
        vms::event::EventState eventState, qint64 timeStamp);

    /*!
        \param timeStamp microseconds from 1970-01-01 (UTC)
    */
    void at_serverFailure(const QnResourcePtr& resource, qint64 timeStamp,
        vms::event::EventReason reasonCode, const QString& reasonText);

    void at_serverStarted(const QnResourcePtr& resource, qint64 timeStamp);

    void at_licenseIssueEvent(const QnResourcePtr& resource, qint64 timeStamp,
        vms::event::EventReason reasonCode, const QString& reasonText);

    void at_cameraIPConflict(const QnResourcePtr& resource, const QHostAddress& hostAddress,
        const QStringList& macAddrList, qint64 timeStamp);

    void at_serverConflict(const QnResourcePtr& resource, qint64 timeStamp,
        const QnCameraConflictList& conflicts);

    void at_serverConflict(const QnResourcePtr& resource, qint64 timeStamp,
        const QnModuleInformation& conflictModule, const QUrl &url);

    void at_noStorages(const QnResourcePtr& resource);

    void at_archiveRebuildFinished(const QnResourcePtr& resource,
        QnSystemHealth::MessageType msgType);

    void at_archiveBackupFinished(const QnResourcePtr& resource, qint64 timeStamp,
        vms::event::EventReason reasonCode, const QString& reasonText);

    void at_remoteArchiveSyncStarted(const QnResourcePtr& resource);

    void at_remoteArchiveSyncFinished(const QnResourcePtr &resource);

    void at_remoteArchiveSyncError(const QnResourcePtr &resource, const QString& error);

    void at_remoteArchiveSyncProgress(const QnResourcePtr &resource, double progress);

    void at_softwareTrigger(const QnResourcePtr& resource, const QString& triggerId,
        const QnUuid& userId, qint64 timeStamp, vms::event::EventState toggleState);

    void at_analyticsSdkEvent(const QnResourcePtr& resource,
        const QnUuid& driverId,
        const QnUuid& eventId,
        vms::event::EventState toggleState,
        const QString& caption,
        const QString& description,
        const QString& auxiliaryData,
        qint64 timeStampUsec);

    void at_analyticsSdkEvent(const nx::vms::event::AnalyticsSdkEventPtr& event);

    void at_FileIntegrityCheckFailed(const QnResourcePtr& resource);

    bool createEventFromParams(const nx::vms::event::EventParameters& params,
        vms::event::EventState eventState, const QnUuid& userId = QnUuid(),
        QString* errorMessage = nullptr);

private slots:
    void onNewResource(const QnResourcePtr& resource);
};

#define qnEventRuleConnector nx::mediaserver::event::EventConnector::instance()

} // namespace event
} // namespace mediaserver
} // namespace nx
