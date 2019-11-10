#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>
#include <nx/vms/api/data/network_block_data.h>

#include <nx/utils/uuid.h>
#include <nx/utils/impl_ptr.h>


namespace nx::vms::client::core {

class PoEController: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QnUuid resourceId READ resourceId WRITE setResourceId
        NOTIFY resourceIdChanged)
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate
        NOTIFY autoUpdateChanged)
    Q_PROPERTY(bool updatingPoweringModes READ updatingPoweringModes
        NOTIFY updatingPoweringModesChanged)
public:
    PoEController(QObject* parent = nullptr);
    virtual ~PoEController();

    void setResourceId(const QnUuid& value);
    QnUuid resourceId() const;

    void setAutoUpdate(bool value);
    bool autoUpdate() const;

    using BlockData = nx::vms::api::NetworkBlockData;
    using OptionalBlockData = std::optional<BlockData>;
    OptionalBlockData blockData() const;

    using PortPoweringMode = nx::vms::api::NetworkPortWithPoweringMode;
    using PowerModes = QList<PortPoweringMode>;
    void setPowerModes(const PowerModes& value);

    bool updatingPoweringModes() const;

signals:
    void resourceIdChanged();
    void autoUpdateChanged();
    void updated();
    void updatingPoweringModesChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
