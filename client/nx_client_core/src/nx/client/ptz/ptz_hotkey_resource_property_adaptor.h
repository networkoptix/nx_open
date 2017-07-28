#pragma once

#include <api/resource_property_adaptor.h>
#include <nx/client/ptz/ptz_types.h>

namespace nx {
namespace client {
namespace core {
namespace ptz {

class PtzHotkeysResourcePropertyAdaptor: public QnJsonResourcePropertyAdaptor<QnPtzIdByHotkeyHash>
{
    Q_OBJECT
    using base_type = QnJsonResourcePropertyAdaptor<QnPtzIdByHotkeyHash>;

public:
    PtzHotkeysResourcePropertyAdaptor(QObject* parent = nullptr);
};

} // namespace ptz
} // namespace core
} // namespace client
} // namespace nx
