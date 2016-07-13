#pragma once

#include <QtCore/QMap>
#include <utils/thread/mutex.h>
#include <atomic>

#include <core/resource/security_cam_resource.h>
#include <utils/common/timermanager.h>
#include <modbus/modbus_async_client.h>
#include <plugins/common_interfaces/abstract_io_manager.h>


class QnAdamResource : public QnSecurityCamResource
{
    Q_OBJECT
public:
    static const QString kManufacture;

    QnAdamResource();
    virtual ~QnAdamResource();

    virtual void setIframeDistance(int frames, int timeMs) override
    {
        QN_UNUSED(frames);
        QN_UNUSED(timeMs);
    }

    virtual QString getDriverName() const override;

    virtual QnIOPortDataList getRelayOutputList() const override;

    virtual QnIOPortDataList getInputPortList() const override;

    virtual bool setRelayOutputState(
        const QString& outputID,
        bool isActive,
        unsigned int autoResetTimeoutMS ) override;

protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override { return nullptr; }

    virtual bool startInputPortMonitoringAsync(
            std::function<void(bool)>&& completionHandler ) override;

    virtual void stopInputPortMonitoringAsync() override;

    virtual bool isInputPortMonitored() const override;

private:
    std::unique_ptr<QnAbstractIOManager> m_ioManager;


};
