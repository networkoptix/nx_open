#pragma once

#include <memory>
#include <functional>

#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_fwd.h>
#include <core/ptz/ptz_constants.h>
#include <core/ptz/ptz_controller_pool.h>

#include <nx/core/ptz/relative/relative_continuous_move_mapping.h>

namespace nx {
namespace vms::server {
namespace ptz {

struct ControllerWrappingParameters
{
    Ptz::Capabilities capabilitiesToAdd;
    Ptz::Capabilities capabilitiesToRemove;

    QnPtzMapperPtr absoluteMoveMapper;
    nx::core::ptz::RelativeContinuousMoveMapping relativeMoveMapping;

    bool preferSystemPresets = false;

    const QnPtzControllerPool* ptzPool = nullptr;
    QnCommonModule* commonModule = nullptr;

    QString toString() const
    {
        return lm("ControllerWrappingParameters("
            "capabilitiesToAdd=%1, "
            "capabilitiesToRemove=%2, "
            "areNativePresetsDisabled=%3)")
            .args(
                capabilitiesToAdd,
                capabilitiesToRemove,
                preferSystemPresets).toQString();
    };
};

using ControllerWrapper = std::function<void(QnPtzControllerPtr* inOutPtr)>;

Ptz::Capabilities capabilities(const QnPtzControllerPtr& controller);

Ptz::Capabilities overridenCapabilities(
    const QnPtzControllerPtr& controller,
    Ptz::Capabilities capabilitiesToAdd,
    Ptz::Capabilities capabilitiesToRemove);

QnPtzControllerPtr wrapController(
    const QnPtzControllerPtr& resource,
    const ControllerWrappingParameters& parameters);

} // namespace ptz
} // namespace vms::server
} // namespace nx
