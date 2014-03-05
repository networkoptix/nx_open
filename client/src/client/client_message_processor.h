#ifndef QN_CLIENT_MESSAGE_PROCESSOR_H
#define QN_CLIENT_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/camera_history.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_control_message.h>

#include <licensing/license.h>

class QnClientMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnClientMessageProcessor();

    virtual void run() override;

signals:
    void videoWallControlMessageReceived(const QnVideoWallControlMessage &message);

protected:
    virtual void loadRuntimeInfo(const QnMessage &message) override;
    virtual void handleConnectionOpened(const QnMessage &message) override;
    virtual void handleMessage(const QnMessage &message) override;

private slots:

    void at_serverIfFound(const QnMediaServerResourcePtr &resource, const QString & url, const QString& origApiUrl);

private:
    void init();
    void determineOptimalIF(const QnMediaServerResourcePtr &resource);
    bool updateResource(QnResourcePtr resource, bool insert, bool updateLayouts);
    void processResources(const QnResourceList& resources);
    void processLicenses(const QnLicenseList& licenses);
    void processCameraServerItems(const QnCameraHistoryList& cameraHistoryList);
    void updateHardwareIds(const QnMessage& message);

private:
    quint32 m_seqNumber;
};

#endif // _client_event_manager_h
