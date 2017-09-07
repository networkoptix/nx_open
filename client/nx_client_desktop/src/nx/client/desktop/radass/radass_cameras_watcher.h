#pragma once

#include <QtCore/QObject>

#include <nx/client/desktop/radass/radass_fwd.h>

#include <utils/common/connective.h>

class QnResourcePool;

namespace nx {
namespace client {
namespace desktop {

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
};

} // namespace desktop
} // namespace client
} // namespace nx