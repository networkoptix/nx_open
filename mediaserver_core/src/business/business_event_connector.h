#ifndef __BUSINESS_EVENT_CONNECTOR_H__
#define __BUSINESS_EVENT_CONNECTOR_H__

#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <nx/streaming/abstract_data_packet.h>

#include <business/business_fwd.h>
#include "business/business_event_parameters.h"
#include <server/server_globals.h>
#include "health/system_health.h"
#include <common/common_module_aware.h>
#include <nx/utils/singleton.h>

struct QnModuleInformation;

/*
* This class listening various logic events, covert these events to business events and send it to businessRuleProcessor
*/

class QnBusinessEventConnector:
    public QObject,
    public QnCommonModuleAware,
    public Singleton<QnBusinessEventConnector>
{
    Q_OBJECT

public:
    QnBusinessEventConnector(QnCommonModule* commonModule);
    ~QnBusinessEventConnector();

public slots:
    /*!
        \param metadata region where is motion occured
        \param value true, if motion started or motion in progress. false, if motion finished. If motion is finished metadata is null
    */
    void at_motionDetected(const QnResourcePtr &resource, bool value, qint64 timeStamp, const QnConstAbstractDataPacketPtr& metadata);

    /*! Camera goes to offline state
    */
    void at_cameraDisconnected(const QnResourcePtr &resource, qint64 timeStamp);

    /*! Some problem with storage
    */
    void at_storageFailure(const QnResourcePtr &mServerRes, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QnResourcePtr &storageRes);
    void at_storageFailure(const QnResourcePtr &mServerRes, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString &storageUrl);


    /*! Some problem with network
    */
    void at_networkIssue(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString &reasonParamsEncoded);

    /*!
        \param inputPortID device-specific ID of input port
        \param value true, if input activated. false, if deactivated
    */
    void at_cameraInput(const QnResourcePtr &resource, const QString& inputPortID, bool value, qint64 timeStampUsec);

    void at_customEvent(const QString &resourceName, const QString& caption, const QString& description, const QnEventMetaData& metadata, QnBusiness::EventState eventState, qint64 timeStamp);

    /*!
        \param timeStamp microseconds from 1970-01-01 (UTC)
    */
    void at_mserverFailure(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);

    void at_mserverStarted(const QnResourcePtr &resource, qint64 timeStamp);

    void at_licenseIssueEvent(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);

    void at_cameraIPConflict(const QnResourcePtr& resource, const QHostAddress& hostAddress, const QStringList& macAddrList, qint64 timeStamp);

    void at_mediaServerConflict(const QnResourcePtr& resource, qint64 timeStamp, const QnCameraConflictList& conflicts);

    void at_mediaServerConflict(const QnResourcePtr& resource, qint64 timeStamp, const QnModuleInformation& conflictModule, const QUrl &url);

    void at_NoStorages(const QnResourcePtr& resource);

    void at_archiveRebuildFinished(const QnResourcePtr& resource, QnSystemHealth::MessageType msgType);

    void at_archiveBackupFinished(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);

    void at_softwareTrigger(const QnResourcePtr& resource, const QString& triggerId, const QnUuid& userId,
        qint64 timeStamp, QnBusiness::EventState toggleState);

    bool createEventFromParams(const QnBusinessEventParameters& params, QnBusiness::EventState eventState,
        const QnUuid& userId = QnUuid(), QString* errMessage = nullptr);

private slots:
    void onNewResource(const QnResourcePtr &resource);
};

#define qnBusinessRuleConnector QnBusinessEventConnector::instance()

#endif // __BUSINESS_EVENT_CONNECTOR_H__
