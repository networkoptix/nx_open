#ifndef __BUSINESS_EVENT_CONNECTOR_H__
#define __BUSINESS_EVENT_CONNECTOR_H__

#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <core/datapacket/abstract_data_packet.h>

#include <business/business_fwd.h>

/*
* This class listening various logic events, covert these events to business events and send it to businessRuleProcessor
*/

class QnBusinessEventConnector: public QObject
{
    Q_OBJECT

public:
    QnBusinessEventConnector();
    ~QnBusinessEventConnector();

    static void initStaticInstance( QnBusinessEventConnector* );
    static QnBusinessEventConnector* instance();

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


    /*! Some problem with network
    */
    void at_networkIssue(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString &reasonParamsEncoded);

    /*!
        \param inputPortID device-specific ID of input port
        \param value true, if input activated. false, if deactivated
    */
    void at_cameraInput(const QnResourcePtr &resource, const QString& inputPortID, bool value, qint64 timeStamp);

    void at_mserverFailure(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);

    void at_mserverStarted(const QnResourcePtr &resource, qint64 timeStamp);

    void at_licenseIssueEvent(const QnResourcePtr &resource, qint64 timeStamp, QnBusiness::EventReason reasonCode, const QString& reasonText);

    void at_cameraIPConflict(const QnResourcePtr& resource, const QHostAddress& hostAddress, const QStringList& macAddrList, qint64 timeStamp);

    void at_mediaServerConflict(const QnResourcePtr& resource, qint64 timeStamp, const QnCameraConflictList& conflicts);

    void at_NoStorages(const QnResourcePtr& resource);

    void at_archiveRebuildFinished(const QnResourcePtr& resource);
private slots:
    void onNewResource(const QnResourcePtr &resource);
};

#define qnBusinessRuleConnector QnBusinessEventConnector::instance()

#endif // __BUSINESS_EVENT_CONNECTOR_H__
