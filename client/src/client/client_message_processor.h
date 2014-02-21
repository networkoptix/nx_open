#ifndef QN_CLIENT_MESSAGE_PROCESSOR_H
#define QN_CLIENT_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/camera_history.h>
#include <core/resource/resource_fwd.h>

class QnClientMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnClientMessageProcessor();

protected:
    virtual void onResourceStatusChanged(QnResourcePtr resource, QnResource::Status status) override;
    virtual void updateResource(QnResourcePtr resource) override;
    virtual void onGotInitialNotification(const ec2::QnFullResourceData& fullData) override;
private:
    void determineOptimalIF(const QnMediaServerResourcePtr &resource);
    void processLicenses(const QnLicenseList& licenses);
    void processResources(const QnResourceList& resources);
    void updateHardwareIds(const ec2::QnFullResourceData& fullData);
    void processCameraServerItems(const QnCameraHistoryList& cameraHistoryList);
};

#endif // _client_event_manager_h
