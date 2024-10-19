// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/ptz/proxy_ptz_controller.h>
#include <core/ptz/ptz_constants.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx {
namespace client {
namespace mobile {

class PtzAvailabilityWatcher;

// TODO: #ynikitenkov Move to client core, use both in desktop and mobile client.
class ResourcePtzController: public QnProxyPtzController
{
    Q_OBJECT
    using base_type = QnProxyPtzController;

    Q_PROPERTY(QnResource* resource READ rawResource WRITE setRawResource NOTIFY resourceChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(int capabilities READ capabilities NOTIFY capabilitiesChanged)
    Q_PROPERTY(int auxTraits READ auxTraits NOTIFY auxTraitsChanged)
    Q_PROPERTY(int presetsCount READ presetsCount NOTIFY presetsCountChanged)
    Q_PROPERTY(int activePresetIndex READ activePresetIndex NOTIFY activePresetIndexChanged)

public:
    ResourcePtzController(QObject* parent = nullptr);
    virtual ~ResourcePtzController() override;

public: // Properties section
    bool available() const;

    Ptz::Traits auxTraits() const;
    int presetsCount() const;
    int activePresetIndex() const;

    Ptz::Capabilities operationalCapabilities() const;
    int capabilities() const;

public:
    Q_INVOKABLE bool setAutoFocus();
    Q_INVOKABLE bool setPresetByIndex(int index);
    Q_INVOKABLE bool setPresetById(const QString& id);
    Q_INVOKABLE int indexOfPreset(const QString& id) const;

    Q_INVOKABLE bool continuousMove(const QVector3D& speed);

signals:
    void resourceChanged();
    void availableChanged();

    void capabilitiesChanged();
    void auxTraitsChanged();
    void presetsCountChanged();
    void activePresetIndexChanged();

private:
    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

private:
    QScopedPointer<PtzAvailabilityWatcher> m_availabilityWatcher;
};

} // namespace mobile
} // namespace client
} // namespace nx
