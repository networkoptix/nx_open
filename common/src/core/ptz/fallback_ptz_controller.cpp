#include "fallback_ptz_controller.h"

#include <cassert>

#include <core/resource/resource.h>

#include <utils/common/event_loop.h>
#include <utils/common/warnings.h>

QnFallbackPtzController::QnFallbackPtzController(const QnPtzControllerPtr &mainController, const QnPtzControllerPtr &fallbackController):
    base_type(mainController->resource()),
    m_mainIsValid(true),
    m_mainController(mainController),
    m_fallbackController(fallbackController)
{
    assert(qnHasEventLoop(thread()));

    if(mainController->resource() != fallbackController->resource())
        qnWarning("Fallback controller was created with two different resources ('%1' != '%2').", mainController->resource()->getName(), fallbackController->resource()->getName());

    connect(mainController,     &QnAbstractPtzController::finished,             this, &QnFallbackPtzController::baseFinished);
    connect(fallbackController, &QnAbstractPtzController::finished,             this, &QnFallbackPtzController::baseFinished);
    connect(mainController,     &QnAbstractPtzController::capabilitiesChanged,  this, &QnFallbackPtzController::baseCapabilitiesChanged);
    connect(fallbackController, &QnAbstractPtzController::capabilitiesChanged,  this, &QnFallbackPtzController::baseCapabilitiesChanged);

    baseCapabilitiesChanged();
}

QnFallbackPtzController::~QnFallbackPtzController() {
    return;
}

void QnFallbackPtzController::baseCapabilitiesChanged() {
    m_mainIsValid = m_mainController->getCapabilities() != Qn::NoPtzCapabilities;

    emit capabilitiesChanged();
}
