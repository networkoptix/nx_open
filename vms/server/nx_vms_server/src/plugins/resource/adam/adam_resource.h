#pragma once

#ifdef ENABLE_ADVANTECH

#include <QtCore/QMap>
#include <atomic>

#include <modbus/modbus_async_client.h>
#include <nx/vms/server/resource/camera.h>
#include <nx/utils/safe_direct_connection.h>
#include <plugins/common_interfaces/abstract_io_manager.h>

class QnAdamResource:
    public nx::vms::server::resource::Camera,
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

    QnAdamResource(QnMediaServerModule* serverModule);
    virtual ~QnAdamResource();

    virtual void setIframeDistance(int frames, int timeMs) override;

    virtual QString getDriverName() const override;

    virtual QnIOStateDataList ioPortStates() const override;

    virtual bool setOutputPortState(
        const QString& outputID,
        bool isActive,
        unsigned int autoResetTimeoutMS ) override;

public slots:
    void at_propertyChanged(const QnResourcePtr& res, const QString& key);

protected:
    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override { return nullptr; }

    virtual void startInputPortStatesMonitoring() override;
    virtual void stopInputPortStatesMonitoring() override;

private:
    void setPortDefaultStates();

    Qn::IOPortType portType(const QString& portId) const;

private:
    std::unique_ptr<QnAbstractIOManager> m_ioManager;
    std::map<quint64, PortTimerEntry> m_autoResetTimers;
    std::map<QString, Qn::IOPortType> m_portTypes;
    mutable QnMutex m_mutex;

};

#endif //< ENABLE_ADVANTECH
