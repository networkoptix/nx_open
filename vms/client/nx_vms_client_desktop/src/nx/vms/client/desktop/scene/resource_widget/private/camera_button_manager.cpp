// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_button_manager.h"

#include <api/model/api_ioport_data.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/algorithm/comparator.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/camera/buttons/aggregated_camera_button_controller.h>
#include <nx/vms/client/core/camera/buttons/extended_output_camera_button_controller.h>
#include <nx/vms/client/core/camera/buttons/software_trigger_camera_button_controller.h>
#include <nx/vms/client/core/camera/buttons/two_way_audio_camera_button_controller.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/scene/resource_widget/overlays/rewind_overlay.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/graphics/items/controls/object_tracking_button.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/overlays/hud_overlay_widget.h>
#include <ui/graphics/items/overlays/scrollable_items_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/software_trigger_button.h>
#include <ui/graphics/items/resource/two_way_audio_button.h>

namespace nx::vms::client::desktop {

namespace {

enum class ActionType
{
    start,
    stop
};

enum ButtonGroup
{
    objectTracking,
    twoWayAudio,
    softwareTriggers,
    extendedOutputs
};

using State = CameraButton::State;
using CameraButtonData = core::CameraButtonData;
using AggregatedControllerPtr = std::unique_ptr<core::AggregatedCameraButtonController>;
using namespace nx::vms::client::desktop::menu;

static constexpr int kTriggersSpacing = 4;
static constexpr int kTriggerButtonSize = 40;
static constexpr int kTriggersMargin = 8;
static constexpr int kScrollLineHeight = 8;

QnScrollableItemsWidget* createContainer(QnMediaResourceWidget* mediaResourceWidget)
{
    const auto hudOverlay = mediaResourceWidget->hudOverlay();
    const auto rewindOverlay = mediaResourceWidget->rewindOverlay();

    QPointer<QnScrollableItemsWidget> container(new QnScrollableItemsWidget(hudOverlay->right()));

    qreal right = 0.0;
    hudOverlay->content()->getContentsMargins(nullptr, nullptr, &right, nullptr);

    const auto triggersMargin = kTriggersMargin - right;
    container->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    container->setLineHeight(kScrollLineHeight);
    container->setSpacing(kTriggersSpacing);
    container->setMaximumWidth(kTriggerButtonSize + triggersMargin);
    container->setContentsMargins(0, 0, triggersMargin, 0);

    const auto updateTriggersMinHeight =
        [content = QPointer<QnViewportBoundWidget>(hudOverlay->content()),
            rewindContent = QPointer<QnViewportBoundWidget>(rewindOverlay->content()),
            container]()
        {
            if (!content || !container || !rewindContent)
                return;

            // Calculate minimum height for downscale no more than kMaxDownscaleFactor times.
            static const qreal kMaxDownscaleFactor = 2.0;
            const qreal available = content->fixedSize().height() / content->sceneScale();
            const qreal desired = container->effectiveSizeHint(Qt::PreferredSize).height();
            const qreal extra = content->size().height() - container->size().height();
            const qreal min = qMin(desired, available * kMaxDownscaleFactor - extra);
            container->setMinimumHeight(min);

            rewindContent->setScale(content->sceneScale());
        };

    QObject::connect(hudOverlay->content(), &QnViewportBoundWidget::scaleChanged,
        container, updateTriggersMinHeight);

    QObject::connect(container.get(), &QnScrollableItemsWidget::contentHeightChanged,
        container, updateTriggersMinHeight);

    QObject::connect(rewindOverlay->content(), &QnViewportBoundWidget::scaleChanged,
        container, updateTriggersMinHeight);

    return container;
}

QnScrollableItemsWidget* createObjectTrackingContainer(QnMediaResourceWidget* mediaResourceWidget)
{
    const auto hudOverlay = mediaResourceWidget->hudOverlay();

    QnScrollableItemsWidget* result = new QnScrollableItemsWidget(hudOverlay->left());
    result->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    result->setLineHeight(kScrollLineHeight);
    result->setContentsMargins(8, 0, 0, 0);

    return result;
}

QString getToolTip(const CameraButtonData& data)
{
    if (!data.enabled)
        return CameraButtonManager::tr("Disabled by schedule");

    switch (data.type)
    {
        case CameraButtonData::Type::instant:
        {
            return data.name;
        }
        case CameraButtonData::Type::prolonged:
        {
            return data.hint.isEmpty()
                ? data.name
                : nx::format("%1 (%2)").args(data.name, data.hint).toQString();
        }
        case CameraButtonData::Type::checkable:
        {
            return data.checked
                ? data.checkedName
                : data.name;
        }
        default:
        {
            return {};
        }
    }
}

AggregatedControllerPtr createController()
{
    auto result = std::make_unique<core::AggregatedCameraButtonController>();

    result->addController<core::TwoWayAudioCameraButtonController>(ButtonGroup::twoWayAudio);

    using HintStyle = core::SoftwareTriggerCameraButtonController::HintStyle;
    result->addController<core::SoftwareTriggerCameraButtonController>(
        ButtonGroup::softwareTriggers, HintStyle::desktop);

    const auto commonOutputs =
        api::ExtendedCameraOutputs(api::ExtendedCameraOutput::heater)
        | api::ExtendedCameraOutput::wiper
        | api::ExtendedCameraOutput::powerRelay;
    result->addController<core::ExtendedOutputCameraButtonController>(
        ButtonGroup::extendedOutputs, commonOutputs);

    result->addController<core::ExtendedOutputCameraButtonController>(
        ButtonGroup::objectTracking, api::ExtendedCameraOutput::autoTracking);

    return result;
}

} // namespace

struct CameraButtonManager::Private: public QObject
{
    CameraButtonManager* const q;
    QnMediaResourceWidget* mediaResourceWidget;
    const AggregatedControllerPtr controller;
    QnScrollableItemsWidget* const container;
    QnScrollableItemsWidget* const objectTrackingContainer;

    Private(
        CameraButtonManager* q,
        QnMediaResourceWidget* mediaResourceWidget);

    void addObjectTrackingButton(const CameraButtonData& data);

    template<typename ButtonType, typename Container>
    ButtonType* insertButton(const CameraButtonData& data, Container* target);

    void handleButtonAdded(const CameraButtonData& data);
    void handleButtonChanged(const CameraButtonData& button, CameraButtonData::Fields fields);
    void handleButtonRemoved(const nx::Uuid& buttonId);

    void handleActionCompletion(const nx::Uuid& buttonId, bool success, ActionType type);
    void handleActionCancelled(const nx::Uuid& buttonId);

    bool isObjectTrackingButton(const nx::Uuid& buttonId);
    bool isObjectTrackingButton(const core::OptionalCameraButtonData& data);

    bool isTwoWayAudioButton(const core::CameraButtonData& data);
};

CameraButtonManager::Private::Private(
    CameraButtonManager* q,
    QnMediaResourceWidget* mediaResourceWidget)
    :
    q(q),
    mediaResourceWidget(mediaResourceWidget),
    controller(createController()),
    container(createContainer(mediaResourceWidget)),
    objectTrackingContainer(createObjectTrackingContainer(mediaResourceWidget))
{
    connect(controller.get(), &core::AbstractCameraButtonController::buttonAdded,
        this, &Private::handleButtonAdded);
    connect(controller.get(), &core::AbstractCameraButtonController::buttonChanged,
        this, &Private::handleButtonChanged);
    connect(controller.get(), &core::AbstractCameraButtonController::buttonRemoved,
        this, &Private::handleButtonRemoved);

    connect(controller.get(), &core::AbstractCameraButtonController::actionStarted, this,
        [this](const nx::Uuid& buttonId, bool success)
        {
            handleActionCompletion(buttonId, success, ActionType::start);
        });

    connect(controller.get(), &core::AbstractCameraButtonController::actionStopped, this,
        [this](const nx::Uuid& buttonId, bool success)
        {
            handleActionCompletion(buttonId, success, ActionType::stop);
        });

    connect(controller.get(), &core::AbstractCameraButtonController::actionCancelled,
        this, &Private::handleActionCancelled);

    controller->setResource(mediaResourceWidget->resource()->toResourcePtr());
}

void CameraButtonManager::Private::addObjectTrackingButton(const CameraButtonData& data)
{
    const auto button = insertButton<ObjectTrackingButton>(data, objectTrackingContainer);
    connect(button, &ObjectTrackingButton::clicked,
        [this, id = data.id]()
        {
            controller->startAction(id);
        });
}

template<typename ButtonType, typename Container>
ButtonType* CameraButtonManager::Private::insertButton(
    const CameraButtonData& data,
    Container* target)
{
    static const auto lessPredicate = nx::utils::algorithm::Comparator(
        /*ascending*/ true,
        &CameraButtonData::name,
        &CameraButtonData::iconName,
        &CameraButtonData::id);

    int index = 0;
    for (; index != target->count(); ++index)
    {
        const auto leftButtonId = target->itemId(index);
        const auto leftButton = controller->buttonData(leftButtonId);
        if (NX_ASSERT(leftButton) && lessPredicate(*leftButton, data))
            break;
    }

    const auto button = new ButtonType(mediaResourceWidget);
    target->insertItem(index, button, data.id);
    return button;
}

void CameraButtonManager::Private::handleButtonAdded(const CameraButtonData& data)
{
    if (isObjectTrackingButton(data))
    {
        /**
         * TODO: #ynikitenkov: Rework object tracking button and software trigger button
         * classes, make the inherited from the same top level base class to avoid all
         * related conditions in the code.
         */
        addObjectTrackingButton(data);
        return;
    }

    const auto button = isTwoWayAudioButton(data)
        ? static_cast<CameraButton*>(insertButton<TwoWayAudioButton>(data, container))
        : static_cast<CameraButton*>(insertButton<SoftwareTriggerButton>(data, container));
    button->setIconName(data.iconName);
    button->setToolTip(getToolTip(data));
    button->setProlonged(data.prolonged());
    button->setEnabled(data.enabled);

    connect(button, &SoftwareTriggerButton::clicked, q,
        [this, button, id = data.id]()
        {
            if (!button->isLive())
            {
                q->menu()->trigger(menu::JumpToLiveAction, mediaResourceWidget);
                return;
            }

            const auto data = controller->buttonData(id);
            if (data->prolonged()) //< Prolonged buttons does not react on click.
                return;

            // Change state of the checkable button or start action for the instant one.
            const bool isActivateAction = !data->checkable() || !data->checked;
            const bool success = isActivateAction
                ? controller->startAction(id)
                : controller->stopAction(id);

            button->setState(success
                ? (isActivateAction
                    ? State::WaitingForActivation
                    : State::WaitingForDeactivation)
                : State::Failure);
        });

    connect(button, &SoftwareTriggerButton::pressed, q,
        [this, button, id = data.id]()
        {
            if (!button->isLive())
                return;

            const auto data = controller->buttonData(id);
            if (!data->prolonged())
                return;

            button->setState(controller->startAction(data->id)
                ? State::WaitingForActivation
                : State::Failure);

            q->menu()->triggerIfPossible(SuspendCurrentShowreelAction);
        });

    connect(button, &SoftwareTriggerButton::released, q,
        [this, button, id = data.id]()
        {
            if (!button->isLive())
                return;

            const auto data = controller->buttonData(id);
            if (!data->prolonged())
                return;

            const nx::utils::ScopeGuard guard(
                [this]() { q->menu()->triggerIfPossible(ResumeCurrentShowreelAction); });

            /* In case of activation error don't try to deactivate: */
            if (!controller->actionIsActive(data->id))
                return;

            button->setState(controller->stopAction(data->id)
                ? State::WaitingForDeactivation
                : State::Failure);
        });
}

void CameraButtonManager::Private::handleButtonChanged(
    const CameraButtonData& data,
    CameraButtonData::Fields fields)
{
    if (isObjectTrackingButton(data))
        return;

    const auto button = dynamic_cast<CameraButton*>(container->item(data.id));
    if (!NX_ASSERT(button))
        return;

    using Field = CameraButtonData::Field;

    if (fields.testAnyFlags({Field::checked, Field::iconName, Field::checkedIconName}))
    {
        button->setToolTip(getToolTip(data));
        button->setIconName(data.checked
            ? data.checkedIconName
            : data.iconName);
    }

    if (fields.testAnyFlags({Field::name, Field::type}))
    {
        // Insert to the a place or update type.
        controller->cancelAction(data.id);

        handleButtonRemoved(data.id);
        handleButtonAdded(data);
    }

    if (fields.testFlag(Field::enabled))
    {
        button->setEnabled(data.enabled);
        button->setToolTip(getToolTip(data));
    }
}

void CameraButtonManager::Private::handleButtonRemoved(const nx::Uuid& buttonId)
{
    container->deleteItem(buttonId);
}

void CameraButtonManager::Private::handleActionCompletion(
    const nx::Uuid& buttonId,
    bool success,
    ActionType type)
{
    const auto data = controller->buttonData(buttonId);
    if (!NX_ASSERT(data))
        return;

    if (isObjectTrackingButton(data))
        return;

    const auto button = dynamic_cast<CameraButton*>(container->item(buttonId));
    if (!NX_ASSERT(button))
        return;

    if (!success)
    {
        button->setState(State::Failure);
        return;
    }

    button->setState(type == ActionType::start && !data->checkable()
        ? State::Activate
        : State::Default);
}

void CameraButtonManager::Private::handleActionCancelled(const nx::Uuid& buttonId)
{
    if (isObjectTrackingButton(buttonId))
        return;

    const auto button = dynamic_cast<CameraButton*>(container->item(buttonId));
    if (NX_ASSERT(button))
        button->setState(State::Failure);
}

bool CameraButtonManager::Private::isObjectTrackingButton(const nx::Uuid& buttonId)
{
    return isObjectTrackingButton(controller->buttonData(buttonId));
}

bool CameraButtonManager::Private::isObjectTrackingButton(const core::OptionalCameraButtonData& data)
{
    return NX_ASSERT(data) && data->group == ButtonGroup::objectTracking;
}

bool CameraButtonManager::Private::isTwoWayAudioButton(const core::CameraButtonData& data)
{
    return data.group == ButtonGroup::twoWayAudio;
}

//-------------------------------------------------------------------------------------------------

CameraButtonManager::CameraButtonManager(
    QnMediaResourceWidget* mediaResourceWidget,
    QObject* parent)
    :
    base_type(parent),
    WindowContextAware(mediaResourceWidget),
    d(new Private(this, mediaResourceWidget))
{
    NX_ASSERT(mediaResourceWidget->resource().dynamicCast<QnVirtualCameraResource>(),
        "Manager must not be created for non-camera resources");
}

CameraButtonManager::~CameraButtonManager()
{
}

QnScrollableItemsWidget* CameraButtonManager::container() const
{
    return d->container;
}

QnScrollableItemsWidget* CameraButtonManager::objectTrackingContainer() const
{
    return d->objectTrackingContainer;
}

} // namespace nx::vms::client::desktop
