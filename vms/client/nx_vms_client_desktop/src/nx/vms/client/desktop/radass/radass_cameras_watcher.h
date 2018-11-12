#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/radass/radass_fwd.h>

#include <utils/common/connective.h>

class QnResourcePool;

namespace nx::vms::client::desktop {

/**
 * Watches for new cameras added to resource pool. Notifies controller if any of them has lags.
 */
class RadassCamerasWatcher: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    RadassCamerasWatcher(RadassController* controller,
        QnResourcePool* resourcePool,
        QObject* parent = nullptr);

    virtual ~RadassCamerasWatcher() override;

private:
    QPointer<RadassController> m_radassController;
};

} // namespace nx::vms::client::desktop
