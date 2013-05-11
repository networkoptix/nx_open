#ifndef _API_PB_SERIALIZER_H_
#define _API_PB_SERIALIZER_H_

#include "serializer.h"

/**
  * Serialize resource to protobuf
  */
class QnApiPbSerializer : public QnApiSerializer
{
public:
    const char* format() const { return "pb"; }

    void deserializeCameras(QnVirtualCameraResourceList& cameras, const QByteArray& data, QnResourceFactory& resourceFactory) override;
    void deserializeServers(QnMediaServerResourceList& servers, const QByteArray& data, QnResourceFactory& resourceFactory) override;
    void deserializeLayout(QnLayoutResourcePtr& layout, const QByteArray& data) override;
    void deserializeLayouts(QnLayoutResourceList& layouts, const QByteArray& data) override;
    void deserializeUsers(QnUserResourceList& users, const QByteArray& data) override;
    void deserializeResources(QnResourceList& resources, const QByteArray& data, QnResourceFactory& resourceFactory) override;
    void deserializeResourceTypes(QnResourceTypeList& resourceTypes, const QByteArray& data) override;
    void deserializeLicenses(QnLicenseList& licenses, const QByteArray& data) override;
    void deserializeCameraHistoryList(QnCameraHistoryList& cameraServerItems, const QByteArray& data) override;
    void deserializeConnectInfo(QnConnectInfoPtr& connectInfo, const QByteArray& data) override;
    void deserializeBusinessRules(QnBusinessEventRules& businessRules, const QByteArray& data) override;
    void deserializeBusinessAction(QnAbstractBusinessActionPtr& businessAction, const QByteArray& data) override;
    void deserializeBusinessActionList(QnAbstractBusinessActionList &businessActionList, const QByteArray& data) override;
    void deserializeKvPairs(QnKvPairs& kvPairs, const QByteArray& data);
    void deserializeSettings(QnKvPairList& kvPairs, const QByteArray& data);

    void serializeLayouts(const QnLayoutResourceList& layouts, QByteArray& data) override;
    void serializeLayout(const QnLayoutResourcePtr& resource, QByteArray& data) override;
    void serializeCameras(const QnVirtualCameraResourceList& cameras, QByteArray& data) override;
    void serializeLicense(const QnLicensePtr& license, QByteArray& data) override;
	void serializeLicenses(const QList<QnLicensePtr>& licenses, QByteArray& data) override;
    void serializeCameraServerItem(const QnCameraHistoryItem& cameraHistory, QByteArray& data) override;
    void serializeBusinessRules(const QnBusinessEventRules&, QByteArray& data) override;
    void serializeBusinessRule(const QnBusinessEventRulePtr&, QByteArray& data) override;
    void serializeEmail(const QStringList& to, const QString& subject, const QString& message, int timeout, QByteArray& data) override;
    void serializeBusinessAction(const QnAbstractBusinessActionPtr& action, QByteArray& data) override;
    void serializeBusinessActionList(const QnAbstractBusinessActionList &businessActions, QByteArray& data) override;
    void serializeKvPair(const QnResourcePtr& resource, const QnKvPair& kvPair, QByteArray& data);
    void serializeKvPairs(const QnResourcePtr& resource, const QnKvPairList& kvPairs, QByteArray& data);
    void serializeSettings(const QnKvPairList& kvPairs, QByteArray& data);

private:
    void serializeCamera(const QnVirtualCameraResourcePtr& resource, QByteArray& data) override;
    void serializeServer(const QnMediaServerResourcePtr& resource, QByteArray& data) override;
    void serializeUser(const QnUserResourcePtr& resource, QByteArray& data) override;
};

#endif // _API_PB_SERIALIZER_H_
