#pragma once

#ifdef ENABLE_ADVANTECH

#include <QtCore/QMap>
#include <atomic>

#include <core/resource/security_cam_resource.h>
#include <core/resource/camera_resource.h>
#include <modbus/modbus_async_client.h>
#include <plugins/common_interfaces/abstract_io_manager.h>
#include <utils/common/safe_direct_connection.h>

class QnAdamResource :
    public QnPhysicalCameraResource,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

    struct PortTimerEntry
    {
        QString portId;
        bool state;
    };

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

    virtual QnIOStateDataList ioStates() const override;

    virtual bool setRelayOutputState(
        const QString& outputID,
        bool isActive,
        unsigned int autoResetTimeoutMS ) override;

public slots:
    void at_propertyChanged(const QnResourcePtr & res, const QString & key);

protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override { return nullptr; }

    virtual bool startInputPortMonitoringAsync(
            std::function<void(bool)>&& completionHandler ) override;

    virtual void stopInputPortMonitoringAsync() override;

    virtual bool isInputPortMonitored() const override;

private:
    QnIOPortDataList mergeIOPortData(
        const QnIOPortDataList& fromDevice,
        const QnIOPortDataList& saved) const;

    void setPortDefaultStates();


private:
    std::unique_ptr<QnAbstractIOManager> m_ioManager;
    std::map<quint64, PortTimerEntry> m_autoResetTimers;



    mutable QnMutex m_mutex;


};

#endif //< ENABLE_ADVANTECH
