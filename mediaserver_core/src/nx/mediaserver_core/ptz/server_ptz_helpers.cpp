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
#include <nx/core/ptz/realtive/relative_move_workaround_controller.h>
#include <nx/core/ptz/realtive/relative_continuous_move_mapping.h>
#include <nx/core/ptz/realtive/relative_continuous_move_sequence_maker.h>

namespace nx {
namespace mediaserver_core {
namespace ptz {

namespace core_ptz = nx::core::ptz;
namespace mediaserver_core = nx::mediaserver_core::ptz;

namespace {

QnPtzControllerPtr wrap(
    const QnPtzControllerPtr& controller,
    ControllerWrapper wrapper,
    Ptz::Capabilities capabilitiesToAdd,
    Ptz::Capabilities capabilitiesToRemove)
{
    auto wrappedController = controller;
    const auto requiredCapabilities = overridenCapabilities(
        controller,
        capabilitiesToAdd,
        capabilitiesToRemove);

    if (requiredCapabilities != capabilities(controller))
    {
        wrappedController.reset(new core_ptz::OverridenCapabilitiesPtzController(
            wrappedController,
            requiredCapabilities));
    }

    wrapper(&wrappedController);
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
    return controller->getCapabilities(core_ptz::Options());
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
                parameters.areNativePresetsDisabled))
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

        [](QnPtzControllerPtr* original)
        {
            if (QnWorkaroundPtzController::extends(capabilities(*original)))
                original->reset(new QnWorkaroundPtzController(*original));
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
} // namespace mediaserver_core
} // namespace nx
