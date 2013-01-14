#ifndef __BUSINESS_EVENT_CONNECTOR_H__
#define __BUSINESS_EVENT_CONNECTOR_H__

#include "core/resource/resource_fwd.h"
#include "core/datapacket/media_data_packet.h"

/*
* This class listening various logic events, covert these events to business events and send it to businessRuleProcessor
*/

class QnBusinessEventConnector: public QObject
{
    Q_OBJECT
public:
    static QnBusinessEventConnector* instance();

public slots:
    /*!
        \param metadata region where is motion occured
        \param value true, if motion started or motion in progress. false, if motion finished. If motion is finished metadata is null
    */
    void at_motionDetected(const QnResourcePtr &resource, bool value, qint64 timeStamp, QnMetaDataV1Ptr metadata);

    /*! Camera goes to offline state
    */
    void at_cameraDisconnected(const QnResourcePtr &resource, qint64 timeStamp);

    /*! Some problem with storage
    */
    void at_storageFailure(const QnResourcePtr &resource, qint64 timeStamp, const QString& reason);

    /*!
        \param inputPortID device-specific ID of input port
        \param value true, if input activated. false, if deactivated
    */
    void at_cameraInput(const QnResourcePtr &resource, const QString& inputPortID, bool value, qint64 timeStamp);
};

#define qnBusinessRuleConnector QnBusinessEventConnector::instance()

#endif // __BUSINESS_EVENT_CONNECTOR_H__
