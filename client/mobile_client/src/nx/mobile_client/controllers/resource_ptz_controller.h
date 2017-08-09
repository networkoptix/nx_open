#pragma once

#include <core/ptz/ptz_constants.h>
#include <core/ptz/proxy_ptz_controller.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace mobile {

class ResourcePtzController: public QnProxyPtzController
{
    Q_OBJECT
    using base_type = QnProxyPtzController;

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(int capabilities READ capabilities NOTIFY capabilitiesChanged)
    Q_PROPERTY(Ptz::Traits auxTraits READ auxTraits NOTIFY auxTraitsChanged)
    Q_PROPERTY(int presetsCount READ presetsCount NOTIFY presetsCountChanged)
    Q_PROPERTY(int activePresetIndex READ activePresetIndex NOTIFY activePresetIndexChanged)

public:
    ResourcePtzController(QObject* parent = nullptr);

public: // Properties section
    QString resourceId() const;
    void setResourceId(const QString& value);

    bool available() const;

    Ptz::Traits auxTraits() const;
    int presetsCount() const;
    int activePresetIndex() const;

    int capabilities() const;

public:
    Q_INVOKABLE bool setAutoFocus();
    Q_INVOKABLE bool setPresetByIndex(int index);
    Q_INVOKABLE bool setPresetById(const QString& id);
    Q_INVOKABLE int indexOfPreset(const QString& id) const;

signals:
    void resourceIdChanged();
    void availableChanged();

    void capabilitiesChanged();
    void auxTraitsChanged();
    void presetsCountChanged();
    void activePresetIndexChanged();

private:
    QnUuid m_resourceId;
};

} // namespace mobile
} // namespace client
} // namespace nx
