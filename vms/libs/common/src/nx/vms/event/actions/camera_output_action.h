#pragma once

#include <nx/vms/event/actions/abstract_action.h>

#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class CameraOutputAction: public AbstractAction
{
    using base_type = AbstractAction;

public:
    explicit CameraOutputAction(const EventParameters& runtimeParams);

    QString getRelayOutputId() const;
    int getRelayAutoResetTimeout() const;

    QString getExternalUniqKey() const;
};

} // namespace event
} // namespace vms
} // namespace nx
