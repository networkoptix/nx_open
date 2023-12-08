// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "business_rule_item_delegate.h"

#include <QtGui/QPainter>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>

// TODO: #vkutin Think about proper location and namespace.
#include <business/business_resource_validation.h>
#include <business/business_types_comparator.h>
#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/rules/event_action_subtype.h>
#include <nx/vms/client/desktop/rules/nvr_events_actions_access.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/delegates/select_cameras_delegate_editor_button.h>
#include <ui/delegates/select_servers_delegate_editor_button.h>
#include <ui/delegates/select_users_delegate_editor_button.h>
#include <ui/models/business_rules_view_model.h>
#include <ui/models/notification_sound_model.h>
#include <ui/widgets/business/aggregation_widget.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <utils/math/color_transformations.h>

using namespace nx;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

namespace {
enum { comboBoxMaxVisibleItems = 100 };
}

QnBusinessRuleItemDelegate::QnBusinessRuleItemDelegate(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_lexComparator(new QnBusinessTypesComparator()),
    m_businessStringsHelper(new vms::event::StringsHelper(systemContext()))
{
}

QnBusinessRuleItemDelegate::~QnBusinessRuleItemDelegate()
{
}

int QnBusinessRuleItemDelegate::optimalWidth(Column column, const QFontMetrics& metrics) const
{
    const int kExtraSpace =
        style::Metrics::kStandardPadding //< dropdown text indent
        + style::Metrics::kButtonHeight; //< dropdown arrow

    switch (Column(column))
    {
        case Column::event:
        {
            vms::event::StringsHelper helper(systemContext());
            auto eventWidth = [&metrics, &helper](vms::api::EventType eventType)
                {
                    return metrics.horizontalAdvance(helper.eventName(eventType));
                };
            int result = -1;
            for (vms::api::EventType eventType: vms::event::allEvents())
                result = qMax(result, eventWidth(eventType));
            return kExtraSpace + result;
        }
        case Column::action:
        {
            vms::event::StringsHelper helper(systemContext());
            auto actionWidth = [&metrics, &helper](vms::api::ActionType actionType)
                {
                    return metrics.horizontalAdvance(helper.actionName(actionType));
                };
            int result = -1;
            for (vms::api::ActionType actionType: vms::event::allActions())
                result = qMax(result, actionWidth(actionType));
            return kExtraSpace + result;
        }
        case Column::aggregation:
        {
            return nx::vms::client::desktop::AggregationWidget::optimalWidth();
        }
        default:
            break;
    }
    return -1;
}

void QnBusinessRuleItemDelegate::initStyleOption(QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    using namespace nx::vms::client::core;

    base_type::initStyleOption(option, index);

    if (index.data(Qn::DisabledRole).toBool())
    {
        if (auto vopt = qstyleoption_cast<QStyleOptionViewItem*>(option))
            vopt->state &= ~QStyle::State_Enabled;

        option->palette.setColor(QPalette::Highlight, colorTheme()->color("brand_core", 77));
    }
    else if (!index.data(Qn::ValidRole).toBool())
    {
        static const auto kErrorHoverOpacity = 0.35;
        option->palette.setColor(QPalette::Midlight, toTransparent(
            option->palette.color(QPalette::Midlight), kErrorHoverOpacity));

        option->palette.setColor(QPalette::Highlight, colorTheme()->color("brand_core", 77));
    }
}

void QnBusinessRuleItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (!option.state.testFlag(QStyle::State_Selected))
    {
        if (!index.data(Qn::DisabledRole).toBool() && !index.data(Qn::ValidRole).toBool())
            painter->fillRect(option.rect, nx::vms::client::core::colorTheme()->color("red_core", 77));
    }

    base_type::paint(painter, option, index);
}

QWidget* QnBusinessRuleItemDelegate::createEditor(
    QWidget* parent,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    switch (Column(index.column()))
    {
        case Column::source:
            return createSourceEditor(parent, index);
        case Column::target:
            return createTargetEditor(parent, index);
        case Column::event:
            return createEventEditor(parent, index);
        case Column::action:
            return createActionEditor(parent, index);
        case Column::aggregation:
            return createAggregationEditor(parent, index);
        default:
            break;
    }

    return base_type::createEditor(parent, option, index);
}


void QnBusinessRuleItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    switch (Column(index.column()))
    {
        case Column::source:
        {
            if (QnSelectResourcesDialogButton* button =
                dynamic_cast<QnSelectResourcesDialogButton*>(editor))
            {
                button->setResources(index.data(Qn::EventResourcesRole).value<QnUuidSet>());
                button->setText(index.data(Qn::ShortTextRole).toString());
                return;
            }
            break;
        }
        case Column::target:
        {
            if (QnSelectResourcesDialogButton* button =
                dynamic_cast<QnSelectResourcesDialogButton*>(editor))
            {
                button->setResources(index.data(Qn::ActionResourcesRole).value<QnUuidSet>());
                button->setText(index.data(Qn::ShortTextRole).toString());
                return;
            }
            break;
        }
        case Column::event:
            if (QComboBox* comboBox = dynamic_cast<QComboBox*>(editor))
            {
                comboBox->setCurrentIndex(comboBox->findData(
                    index.data(Qn::EventTypeRole).value<vms::api::EventType>()));
                connect(comboBox, QnComboboxCurrentIndexChanged,
                    this, &QnBusinessRuleItemDelegate::emitCommitData);
            }
            return;
        case Column::action:
            if (QComboBox* comboBox = dynamic_cast<QComboBox*>(editor))
            {
                comboBox->setCurrentIndex(comboBox->findData(
                    index.data(Qn::ActionTypeRole).value<vms::api::ActionType>()));
                connect(comboBox, QnComboboxCurrentIndexChanged,
                    this, &QnBusinessRuleItemDelegate::emitCommitData);
            }
            return;
        case Column::aggregation:
            if (AggregationWidget* aggregationWidget = dynamic_cast<AggregationWidget*>(editor))
            {
                aggregationWidget->setValue(index.data(Qt::EditRole).toInt());
                connect(aggregationWidget, &AggregationWidget::valueChanged,
                    this, &QnBusinessRuleItemDelegate::emitCommitData);
            }
            return;
        default:
            break;
    }

    base_type::setEditorData(editor, index);
}

void QnBusinessRuleItemDelegate::setModelData(
    QWidget* editor,
    QAbstractItemModel* model,
    const QModelIndex& index) const
{
    switch (Column(index.column()))
    {
        case Column::source:
        case Column::target:
        {
            if (QnSelectResourcesDialogButton* btn =
                dynamic_cast<QnSelectResourcesDialogButton*>(editor))
            {
                model->setData(index, QVariant::fromValue(btn->getResources()));
                return;
            }
            break;
        }
        case Column::event:
        case Column::action:
            if (QComboBox* comboBox = dynamic_cast<QComboBox*>(editor))
            {
                model->setData(index, comboBox->itemData(comboBox->currentIndex()));
            }
            return;
        case Column::aggregation:
            if (nx::vms::client::desktop::AggregationWidget* widget =
                dynamic_cast<nx::vms::client::desktop::AggregationWidget*>(editor))
            {
                model->setData(index, widget->value());
            }
            return;
        default:
            break;
    }
    base_type::setModelData(editor, model, index);
}

bool QnBusinessRuleItemDelegate::eventFilter(QObject* object, QEvent* event)
{
    QComboBox* editor = qobject_cast<QComboBox*>(object);
    if (editor && event->type() == QEvent::FocusOut)
        return false;
    return base_type::eventFilter(object, event);
}

QWidget* QnBusinessRuleItemDelegate::createSourceEditor(QWidget* parent,
    const QModelIndex& index) const
{
    const auto eventType = index.data(Qn::EventTypeRole).value<vms::api::EventType>();

    if (nx::vms::event::requiresCameraResource(eventType))
    {
        auto button = new QnSelectCamerasDialogButton(parent);

        connect(button, &QnSelectCamerasDialogButton::commit,
            this, &QnBusinessRuleItemDelegate::emitCommitData);

        if (eventType == EventType::cameraMotionEvent)
            button->setResourcePolicy<QnCameraMotionPolicy>();
        else if (eventType == EventType::cameraInputEvent)
            button->setResourcePolicy<QnCameraInputPolicy>();
        else if (eventType == EventType::analyticsSdkEvent)
            button->setResourcePolicy<QnCameraAnalyticsPolicy>();
        else if (eventType == EventType::analyticsSdkObjectDetected)
            button->setResourcePolicy<QnCameraAnalyticsPolicy>();

        return button;
    }

    if (nx::vms::event::requiresServerResource(eventType))
    {
        auto button = new SelectServersDialogButton(parent);

        connect(button, &SelectServersDialogButton::commit,
            this, &QnBusinessRuleItemDelegate::emitCommitData);

        if (eventType == EventType::poeOverBudgetEvent)
            button->setResourcePolicy<QnPoeOverBudgetPolicy>();
        else if (eventType == EventType::fanErrorEvent)
            button->setResourcePolicy<QnFanErrorPolicy>();

        return button;
    }

    return nullptr;
}

QWidget* QnBusinessRuleItemDelegate::createTargetEditor(QWidget* parent,
    const QModelIndex& index) const
{
    const auto actionType = index.data(Qn::ActionTypeRole).value<vms::api::ActionType>();
    const auto model = index.data(Qn::RuleModelRole).value<QnBusinessRuleViewModelPtr>();

    QnSelectResourcesDialogButton* editorButton = nullptr;

    if (actionType == ActionType::cameraRecordingAction)
    {
        auto camerasButton = new QnSelectCamerasDialogButton(parent);
        camerasButton->setResourcePolicy<QnCameraRecordingPolicy>();
        editorButton = camerasButton;
    }
    else if (actionType == ActionType::bookmarkAction)
    {
        auto camerasButton = new QnSelectCamerasDialogButton(parent);
        camerasButton->setResourcePolicy<QnBookmarkActionPolicy>();
        editorButton = camerasButton;
    }
    else if (actionType == ActionType::cameraOutputAction)
    {
        auto camerasButton = new QnSelectCamerasDialogButton(parent);
        camerasButton->setResourcePolicy<QnCameraOutputPolicy>();
        editorButton = camerasButton;
    }
    else if (actionType == ActionType::executePtzPresetAction)
    {
        auto camerasButton = new QnSelectCamerasDialogButton(parent);
        camerasButton->setResourcePolicy<QnExecPtzPresetPolicy>();
        editorButton = camerasButton;
    }
    else if (actionType == ActionType::sendMailAction)
    {
        auto usersButton = new QnSelectUsersDialogButton(parent);
        usersButton->setDialogDelegate(new QnSendEmailActionDelegate(usersButton));
        editorButton = usersButton;
    }
    else if (actionType == ActionType::openLayoutAction)
    {
        auto usersButton = new QnSelectUsersDialogButton(parent);
        auto validator = new QnLayoutAccessValidationPolicy{};
        validator->setLayout(
            resourcePool()->getResourceById<QnLayoutResource>(
                model->actionParams().actionResourceId));
        usersButton->setSubjectValidationPolicy(validator);
        editorButton = usersButton;
    }
    else if (actionType == ActionType::showPopupAction)
    {
        auto usersButton = new QnSelectUsersDialogButton(parent);
        if (model->actionParams().needConfirmation)
        {
            auto validator = new QnRequiredAccessRightPolicy(
                nx::vms::api::AccessRight::manageBookmarks);
            validator->setCameras({
                resourcePool()->getResourceById<QnVirtualCameraResource>(
                    model->eventParams().eventResourceId)});
            usersButton->setSubjectValidationPolicy(validator);
        }
        else
        {
            usersButton->setSubjectValidationPolicy(new QnDefaultSubjectValidationPolicy());
        }
        editorButton = usersButton;
    }
    else if (actionType == ActionType::pushNotificationAction)
    {
        auto usersButton = new QnSelectUsersDialogButton(parent);
        usersButton->setSubjectValidationPolicy(new QnCloudUsersValidationPolicy());
        usersButton->setDialogOptions(SubjectSelectionDialog::CustomizableOptions::cloudUsers());
        editorButton = usersButton;
    }
    else if (actionType == ActionType::playSoundAction ||
        actionType == ActionType::playSoundOnceAction ||
        actionType == ActionType::sayTextAction)
    {
        auto camerasButton = new QnSelectCamerasDialogButton(parent);
        camerasButton->setResourcePolicy<QnCameraAudioTransmitPolicy>();
        editorButton = camerasButton;
    }
    else if (actionType == ActionType::fullscreenCameraAction)
    {
        auto camerasButton = new QnSelectCamerasDialogButton(parent);
        camerasButton->setResourcePolicy<QnFullscreenCameraPolicy>();
        editorButton = camerasButton;
    }
    else if (actionType == ActionType::buzzerAction)
    {
        auto serversButton = new SelectServersDialogButton(parent);
        serversButton->setResourcePolicy<QnBuzzerPolicy>();
        editorButton = serversButton;
    }

    if (editorButton)
    {
        connect(editorButton, &QnSelectResourcesDialogButton::commit,
            this, &QnBusinessRuleItemDelegate::emitCommitData);
    }

    // TODO: #vbreus Create proper editor for exitFullscreenAction
    return editorButton;
}

QWidget* QnBusinessRuleItemDelegate::createEventEditor(QWidget* parent,
    const QModelIndex& /*index*/) const
{
    auto comboBox = new QComboBox(parent);
    comboBox->setMaxVisibleItems(comboBoxMaxVisibleItems);

    auto addItem =
        [this, comboBox](vms::api::EventType eventType)
        {
            comboBox->addItem(m_businessStringsHelper->eventName(eventType), eventType);
        };


    const auto accessibleEvents = NvrEventsActionsAccess::removeInacessibleNvrEvents(
        nx::vms::event::allEvents(), resourcePool());

    const auto userEvents = filterEventsBySubtype(accessibleEvents, EventSubtype::user);
    const auto failureEvents = filterEventsBySubtype(accessibleEvents, EventSubtype::failure);
    const auto successEvents = filterEventsBySubtype(accessibleEvents, EventSubtype::success);

    for (const auto eventType: m_lexComparator->lexSortedEvents(userEvents))
        addItem(eventType);
    comboBox->insertSeparator(comboBox->count());
    for (const auto eventType: m_lexComparator->lexSortedEvents(failureEvents))
        addItem(eventType);
    comboBox->insertSeparator(comboBox->count());
    for (const auto eventType: m_lexComparator->lexSortedEvents(successEvents))
        addItem(eventType);

    return comboBox;
}

QWidget* QnBusinessRuleItemDelegate::createActionEditor(QWidget* parent,
    const QModelIndex& index) const
{
    auto eventType = index.data(Qn::EventTypeRole).value<vms::api::EventType>();
    auto eventParams = index.data(Qn::EventParametersRole).value<nx::vms::event::EventParameters>();
    bool isInstantOnly = !vms::event::hasToggleState(eventType, eventParams, systemContext());

    QComboBox* comboBox = new QComboBox(parent);
    comboBox->setMaxVisibleItems(comboBoxMaxVisibleItems);

    const auto accessibleActions = NvrEventsActionsAccess::removeInacessibleNvrActions(
        vms::event::userAvailableActions(), resourcePool());

    for (const auto actionType: m_lexComparator->lexSortedActions(accessibleActions))
    {
        if (isInstantOnly && !vms::event::canBeInstant(actionType))
            continue;
        comboBox->addItem(m_businessStringsHelper->actionName(actionType), actionType);
    }

    return comboBox;
}

QWidget* QnBusinessRuleItemDelegate::createAggregationEditor(QWidget* parent,
    const QModelIndex& /*index*/) const
{
    nx::vms::client::desktop::AggregationWidget* aggregationWidget =
        new nx::vms::client::desktop::AggregationWidget(parent);
    aggregationWidget->setShort(true);

    return aggregationWidget;
}

void QnBusinessRuleItemDelegate::emitCommitData()
{
    if (QWidget* widget = dynamic_cast<QWidget*>(sender()))
        emit commitData(widget);
}

void QnBusinessRuleItemDelegate::updateEditorGeometry(
    QWidget* editor,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    switch (Column(index.column()))
    {
        case Column::event:
        case Column::action:
            editor->setGeometry(option.rect);
            break;

        default:
            base_type::updateEditorGeometry(editor, option, index);
    }
}
