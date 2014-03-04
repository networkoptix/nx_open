#ifndef _API_PB_SERIALIZER_H_
#define _API_PB_SERIALIZER_H_

#include "serializer.h"

/**
 * Serialize resource to protobuf
 */
class QnApiPbSerializer : public QnApiSerializer {
    Q_DECLARE_TR_FUNCTIONS(QnApiPbSerializer)
public:
    const char* format() const { return "pb"; }

    void deserializeCameras(QnNetworkResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory) override;
    void deserializeServers(QnMediaServerResourceList& servers, const QByteArray& data, QnResourceFactory& resourceFactory) override;
    void deserializeLayout(QnLayoutResourcePtr& layout, const QByteArray& data, QList<QnLayoutItemDataList>* orderedItems = 0) override;
    void deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data, QList<QnLayoutItemDataList>* orderedItems = 0) override;
    void deserializeUsers(QnUserResourceList& users, const QByteArray& data) override;
    void deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory) override;
    void deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data) override;
    void deserializeLicenses(QnLicenseList& licenses, const QByteArray& data) override;
    void deserializeCameraHistoryList(QnCameraHistoryList& cameraServerItems, const QByteArray& data) override;
    void deserializeConnectInfo(QnConnectionInfoPtr& connectInfo, const QByteArray& data) override;
    void deserializeBusinessRules(QnBusinessEventRuleList& businessRules, const QByteArray& data) override;
    void deserializeBusinessAction(QnAbstractBusinessActionPtr& businessAction, const QByteArray& data) override;
    void deserializeBusinessActionVector(QnBusinessActionDataListPtr &businessActionList, const QByteArray& data) override;
    void deserializeKvPairs(QnKvPairListsById& kvPairs, const QByteArray& data) override;
    void deserializeSettings(QnKvPairList& kvPairs, const QByteArray& data) override;
    void deserializeVideoWalls(QnVideoWallResourceList& videoWalls, const QByteArray& data) override;

    void serializeLayouts(const QnLayoutResourceList& layouts, QByteArray& data) override;
    void serializeLayout(const QnLayoutResourcePtr& resource, QByteArray& data) override;
    void serializeCameras(const QnVirtualCameraResourceList& cameras, QByteArray& data) override;
    void serializeLicense(const QnLicensePtr& license, QByteArray& data) override;
    void serializeLicenses(const QList<QnLicensePtr>& licenses, QByteArray& data) override;
    void serializeCameraServerItem(const QnCameraHistoryItem& cameraHistory, QByteArray& data) override;
    void serializeBusinessRules(const QnBusinessEventRuleList&, QByteArray& data) override;
    void serializeBusinessRule(const QnBusinessEventRulePtr&, QByteArray& data) override;
    void serializeEmail(const QStringList& to, const QString& subject, const QString& message, const QnEmailAttachmentList& attachments, int timeout, QByteArray& data);
    void serializeBusinessAction(const QnAbstractBusinessActionPtr& action, QByteArray& data) override;
    void serializeBusinessActionList(const QnAbstractBusinessActionList &businessActions, QByteArray& data) override;
    void serializeKvPair(const QnResourcePtr& resource, const QnKvPair& kvPair, QByteArray& data) override;
    void serializeKvPairs(int resourceId, const QnKvPairList& kvPairs, QByteArray& data) override;
    void serializeSettings(const QnKvPairList& kvPairs, QByteArray& data) override;
    void serializeVideoWall(const QnVideoWallResourcePtr& videoWall, QByteArray& data) override;
    void serializeVideoWallControl(const QnVideoWallControlMessage &message, QByteArray& data) override;

private:
    void serializeCamera(const QnVirtualCameraResourcePtr& resource, QByteArray& data) override;
    void serializeServer(const QnMediaServerResourcePtr& resource, QByteArray& data) override;
    void serializeUser(const QnUserResourcePtr& resource, QByteArray& data) override;
};

#endif // _API_PB_SERIALIZER_H_
