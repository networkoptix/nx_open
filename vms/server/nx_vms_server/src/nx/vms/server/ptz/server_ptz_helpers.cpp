#include "server_ptz_helpers.h"

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

#include <core/ptz/activity_ptz_controller.h>
#include <core/ptz/home_ptz_controller.h>
#include <core/ptz/mapped_ptz_controller.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/tour_ptz_controller.h>
#include <core/ptz/viewport_ptz_controller.h>
#include <core/ptz/workaround_ptz_controller.h>

#include <nx/core/ptz/overriden_capabilities_ptz_controller.h>
#include <nx/core/ptz/utils/continuous_move_sequence_executor.h>
#include <nx/core/ptz/relative/relative_move_workaround_controller.h>
#include <nx/core/ptz/relative/relative_continuous_move_mapping.h>
#include <nx/core/ptz/relative/relative_continuous_move_sequence_maker.h>

namespace nx {
namespace vms::server {
namespace ptz {

namespace core_ptz = nx::core::ptz;

namespace {

QnPtzControllerPtr wrap(
    const QnPtzControllerPtr& controller,
    ControllerWrapper wrapper,
    Ptz::Capabilities capabilitiesToAdd,
    Ptz::Capabilities capabilitiesToRemove)
{
    const auto requiredCapabilities = overridenCapabilities(
        controller,
        capabilitiesToAdd,
        capabilitiesToRemove);

    QnPtzControllerPtr overridenController = controller;
    if (requiredCapabilities != capabilities(controller))
    {
        overridenController.reset(new core_ptz::OverridenCapabilitiesPtzController(
            controller,
            requiredCapabilities));
    }

    auto wrappedController = overridenController;
    wrapper(&wrappedController);
    if (wrappedController == overridenController)
        return controller;

    NX_VERBOSE(controller.get(), "Is wrapped by %1", wrappedController.get());
    return wrappedController;
}

} // namespace

Ptz::Capabilities capabilities(const QnPtzControllerPtr& controller)
{
    if (!controller)
    {
        NX_ASSERT(false, lit("Controller should exist"));
        return Ptz::NoPtzCapabilities;
    }
    return controller->getCapabilities();
}

Ptz::Capabilities overridenCapabilities(
    const QnPtzControllerPtr& controller,
    Ptz::Capabilities capabilitiesToAdd,
    Ptz::Capabilities capabilitiesToRemove)
{
    if (!controller)
    {
        NX_ASSERT(false, lit("Controller should exist"));
        return Ptz::NoPtzCapabilities;
    }

    const auto controllerCapabilities = capabilities(controller);
    NX_ASSERT(
        (capabilitiesToAdd & capabilitiesToRemove) == Ptz::NoPtzCapabilities,
        lit("Wrong parameters. There are the same capabilities to add and to remove."));
    return (controllerCapabilities | capabilitiesToAdd) & ~capabilitiesToRemove;
}

QnPtzControllerPtr wrapController(
    const QnPtzControllerPtr& controller,
    const ControllerWrappingParameters& parameters)
{
    const std::vector<ControllerWrapper> wrappers = {
        [](QnPtzControllerPtr* original)
        {
            if (QnWorkaroundPtzController::extends(capabilities(*original)))
                original->reset(new QnWorkaroundPtzController(*original));
        },

        [&parameters](QnPtzControllerPtr* original)
        {
            if (!parameters.absoluteMoveMapper
                || !QnMappedPtzController::extends(capabilities(*original)))
            {
                return;
            }

            original->reset(new QnMappedPtzController(parameters.absoluteMoveMapper, *original));
        },

        [](QnPtzControllerPtr* original)
        {
            if (QnViewportPtzController::extends(capabilities(*original)))
                original->reset(new QnViewportPtzController(*original));
        },

        [&parameters](QnPtzControllerPtr* original)
        {
            if (QnPresetPtzController::extends(
                capabilities(*original),
                parameters.preferSystemPresets))
            {
               original->reset(new QnPresetPtzController(*original));
            }
        },

        [&parameters](QnPtzControllerPtr* original)
        {
            if (!parameters.ptzPool)
                return;

            if (QnTourPtzController::extends(capabilities(*original)))
            {
                original->reset(new QnTourPtzController(
                    *original,
                    parameters.ptzPool->commandThreadPool(),
                    parameters.ptzPool->executorThread()));
            }
        },

        [&parameters](QnPtzControllerPtr* original)
        {
            if (QnActivityPtzController::extends(capabilities(*original)))
            {
                original->reset(new QnActivityPtzController(
                    parameters.commonModule,
                    QnActivityPtzController::Server,
                    *original));
            }
        },

        [&parameters](QnPtzControllerPtr* original)
        {
            if (QnHomePtzController::extends(capabilities(*original)))
            {
                original->reset(new QnHomePtzController(
                    *original,
                    parameters.ptzPool->executorThread()));
            }
        },

        [&parameters](QnPtzControllerPtr* original)
        {
            if (core_ptz::RelativeMoveWorkaroundController::extends(capabilities(*original)))
            {
                original->reset(new core_ptz::RelativeMoveWorkaroundController(
                    *original,
                    std::make_shared<core_ptz::RelativeContinuousMoveSequenceMaker>(
                        parameters.relativeMoveMapping),
                    std::make_shared<core_ptz::ContinuousMoveSequenceExecutor>(
                        original->data(),
                        parameters.ptzPool->commandThreadPool())));
            }
        }
    };

    auto wrapped = controller;
    for (auto wrapper: wrappers)
    {
        wrapped = wrap(
            wrapped,
            wrapper,
            parameters.capabilitiesToAdd,
            parameters.capabilitiesToRemove);
    }

    return wrapped;
}

} // namespace ptz
} // namespace vms::server
} // namespace nx
