#pragma once

#include <core/ptz/ptz_constants.h>
#include <core/ptz/proxy_ptz_controller.h>

namespace nx {
namespace client {
namespace mobile {

class ResourcePtzController: public QnProxyPtzController
{
    Q_OBJECT
    using base_type = QnProxyPtzController;

    Q_PROPERTY(QUuid uniqueResourceId READ uniqueResourceId
        WRITE setUniqueResourceId NOTIFY uniqueResourceIdChanged)

    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

    Q_PROPERTY(Ptz::Capabilities capabilities READ getCapabilities NOTIFY capabilitiesChanged)
    Q_PROPERTY(Ptz::Traits auxTraits READ auxTraits NOTIFY auxTraitsChanged)
    Q_PROPERTY(int presetsCount READ presetsCount NOTIFY presetsCountChanged)
    Q_PROPERTY(int activePresetIndex READ activePresetIndex NOTIFY activePresetIndexChanged)

public:
    ResourcePtzController(QObject* parent = nullptr);

public: // Properties section
    QUuid uniqueResourceId() const;
    void setUniqueResourceId(const QUuid& value);

    bool available() const;

    Ptz::Traits auxTraits() const;
    int presetsCount() const;
    int activePresetIndex() const;

public:
    Q_INVOKABLE bool setAutoFocus();
    Q_INVOKABLE bool setPresetByIndex(int index);
    Q_INVOKABLE bool setPresetById(const QString& id);

signals:
    void uniqueResourceIdChanged();
    void availableChanged();

    void capabilitiesChanged();
    void auxTraitsChanged();
    void presetsCountChanged();
    void activePresetIndexChanged();

private:
    QUuid m_uniqueResourceId;
};

} // namespace mobile
} // namespace client
} // namespace nx
