#ifndef QN_API_SERIALIZER_H
#define QN_API_SERIALIZER_H

#include <QtCore/QByteArray>

#include <api/model/kvpair.h>
#include <api/model/email_attachment.h>
#include <api/model/connection_info.h>

#include <business/business_fwd.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_history.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/motion_window.h>

#include <licensing/license.h>

#include <utils/common/exception.h>


/*
 * Helper serialization functions. Not related to any specific serializarion format.
 */

void parseRegion(QRegion& region, const QString& regionString);
void parseRegionList(QList<QRegion>& regions, const QString& regionsString);

void parseMotionRegion(QnMotionRegion& region, const QByteArray& regionString);
QString serializeMotionRegion(const QnMotionRegion& region);

void parseMotionRegionList(QList<QnMotionRegion>& regions, const QByteArray& regionsString);
QString serializeMotionRegionList(const QList<QnMotionRegion>& regions);

QString serializeRegion(const QRegion& region);
QString serializeRegionList(const QList<QRegion>& regions);


/**
 * Base exception class for serialization-related errors.
 */
class QnSerializationException : public QnException {
public:
    QnSerializationException(const QString &message): QnException(message) {}
};


/**
 * Serialize resource.
 */
class QnApiSerializer
{
public:
    void serialize(const QnResourcePtr& resource, QByteArray& data);

    virtual const char* format() const = 0;
    virtual void deserializeCameras(QnNetworkResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory) = 0;
    virtual void deserializeServers(QnMediaServerResourceList& servers, const QByteArray& data, QnResourceFactory& resourceFactory) = 0;
    virtual void deserializeLayout(QnLayoutResourcePtr& layout, const QByteArray& data, QList<QnLayoutItemDataList>* orderedItems = 0) = 0;
    virtual void deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data, QList<QnLayoutItemDataList>* orderedItems = 0) = 0;
    virtual void deserializeUsers(QnUserResourceList& users, const QByteArray& data) = 0;
    virtual void deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory) = 0;
    virtual void deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data) = 0;
    virtual void deserializeLicenses(QnLicenseList& licenses, const QByteArray& data) = 0;
    virtual void deserializeCameraHistoryList(QnCameraHistoryList& cameraServerItems, const QByteArray& data) = 0;
    virtual void deserializeConnectInfo(QnConnectionInfoPtr& connectInfo, const QByteArray& data) = 0;
    virtual void deserializeBusinessRules(QnBusinessEventRuleList&, const QByteArray& data) = 0;
    virtual void deserializeBusinessAction(QnAbstractBusinessActionPtr& businessAction, const QByteArray& data) = 0;
    virtual void deserializeBusinessActionVector(QnBusinessActionDataListPtr& businessActionList, const QByteArray& data) = 0;
    virtual void deserializeKvPairs(QnKvPairListsById& kvPairs, const QByteArray& data) = 0;
    virtual void deserializeSettings(QnKvPairList& kvPairs, const QByteArray& data) = 0;

    virtual void serializeLayout(const QnLayoutResourcePtr& resource, QByteArray& data) = 0;
    virtual void serializeLayouts(const QnLayoutResourceList& layouts, QByteArray& data) = 0;
    virtual void serializeCameras(const QnVirtualCameraResourceList& cameras, QByteArray& data) = 0;
    virtual void serializeLicense(const QnLicensePtr& license, QByteArray& data) = 0;
    virtual void serializeLicenses(const QList<QnLicensePtr>& licenses, QByteArray& data) = 0;
    virtual void serializeCameraServerItem(const QnCameraHistoryItem& cameraHistory, QByteArray& data) = 0;
    virtual void serializeBusinessRules(const QnBusinessEventRuleList&, QByteArray& data) = 0;
    virtual void serializeBusinessRule(const QnBusinessEventRulePtr&, QByteArray& data) = 0;
    virtual void serializeEmail(const QStringList& to, const QString& subject, const QString& message, const QnEmailAttachmentList& attachments, int timeout, QByteArray& data) = 0;
    virtual void serializeBusinessAction(const QnAbstractBusinessActionPtr&, QByteArray& data) = 0;
    virtual void serializeBusinessActionList(const QnAbstractBusinessActionList &businessActions, QByteArray& data) = 0;
    virtual void serializeKvPair(const QnResourcePtr& resource, const QnKvPair& kvPair, QByteArray& data) = 0;
    virtual void serializeKvPairs(int resourceId, const QnKvPairList& kvPairs, QByteArray& data) = 0;
    virtual void serializeSettings(const QnKvPairList& kvPairs, QByteArray& data) = 0;

protected:
    virtual void serializeCamera(const QnVirtualCameraResourcePtr& resource, QByteArray& data) = 0;
    virtual void serializeServer(const QnMediaServerResourcePtr& resource, QByteArray& data) = 0;
    virtual void serializeUser(const QnUserResourcePtr& resource, QByteArray& data) = 0;
};

#endif // QN_API_SERIALIZER_H
