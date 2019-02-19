#include "business_rule_item_delegate.h"

#include <QtGui/QPainter>

#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QComboBox>

// TODO: #vkutin Think about proper location and namespace.
#include <business/business_resource_validation.h>
#include <business/business_types_comparator.h>

#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/strings_helper.h>

#include <client_core/client_core_module.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/models/business_rules_view_model.h>
#include <ui/models/notification_sound_model.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>
#include <ui/widgets/business/aggregation_widget.h>
#include <ui/workbench/workbench_context.h>
#include <utils/math/color_transformations.h>

#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>

#include <ui/delegates/select_cameras_delegate_editor_button.h>
#include <ui/delegates/select_users_delegate_editor_button.h>

using namespace nx;
using namespace nx::vms::client::desktop::ui;

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

namespace {
enum { comboBoxMaxVisibleItems = 100 };
}

QnBusinessRuleItemDelegate::QnBusinessRuleItemDelegate(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_lexComparator(new QnBusinessTypesComparator(true)),
    m_businessStringsHelper(new vms::event::StringsHelper(qnClientCoreModule->commonModule()))
{
}

QnBusinessRuleItemDelegate::~QnBusinessRuleItemDelegate()
{
}

int QnBusinessRuleItemDelegate::optimalWidth(Column column, const QFontMetrics& metrics)
{
    const int kExtraSpace =
        style::Metrics::kStandardPadding //< dropdown text indent
        + style::Metrics::kButtonHeight; //< dropdown arrow

    switch (Column(column))
    {
        case Column::event:
        {
            vms::event::StringsHelper helper(qnClientCoreModule->commonModule());
            auto eventWidth = [&metrics, &helper](vms::api::EventType eventType)
                {
                    return metrics.width(helper.eventName(eventType));
                };
            int result = -1;
            for (vms::api::EventType eventType: vms::event::allEvents())
                result = qMax(result, eventWidth(eventType));
            return kExtraSpace + result;
        }
        case Column::action:
        {
            vms::event::StringsHelper helper(qnClientCoreModule->commonModule());
            auto actionWidth = [&metrics, &helper](vms::api::ActionType actionType)
                {
                    return metrics.width(helper.actionName(actionType));
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
    base_type::initStyleOption(option, index);

    if (index.data(Qn::DisabledRole).toBool())
    {
        if (auto vopt = qstyleoption_cast<QStyleOptionViewItem*>(option))
            vopt->state &= ~QStyle::State_Enabled;

        option->palette.setColor(QPalette::Highlight,
            qnGlobals->businessRuleDisabledHighlightColor());
    }
    else if (!index.data(Qn::ValidRole).toBool())
    {
        static const auto kErrorHoverOpacity = 0.35;
        option->palette.setColor(QPalette::Midlight, toTransparent(
            option->palette.color(QPalette::Midlight), kErrorHoverOpacity));

        option->palette.setColor(QPalette::Highlight,
            qnGlobals->businessRuleInvalidHighlightColor());
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
            painter->fillRect(option.rect, qnGlobals->businessRuleInvalidBackgroundColor());
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
        {
            QnSelectCamerasDialogButton* button = new QnSelectCamerasDialogButton(parent);
            // TODO: #GDM #Business server selection dialog?
            connect(button, SIGNAL(commit()), this, SLOT(at_editor_commit()));

            vms::api::EventType eventType =
                index.data(Qn::EventTypeRole).value<vms::api::EventType>();

            if (eventType == EventType::cameraMotionEvent)
                button->setResourcePolicy<QnCameraMotionPolicy>();
            else if (eventType == EventType::cameraInputEvent)
                button->setResourcePolicy<QnCameraInputPolicy>();
            else if (eventType == EventType::analyticsSdkEvent)
                button->setResourcePolicy<QnCameraAnalyticsPolicy>();

            return button;
        }
        case Column::target:
        {
            vms::api::ActionType actionType =
                index.data(Qn::ActionTypeRole).value<vms::api::ActionType>();

            auto model = index.data(Qn::RuleModelRole).value<QnBusinessRuleViewModelPtr>();

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
            else if (actionType == ActionType::showPopupAction)
            {
                auto usersButton = new QnSelectUsersDialogButton(parent);
                if (model->actionParams().needConfirmation)
                {
                    usersButton->setSubjectValidationPolicy(new QnRequiredPermissionSubjectPolicy(
                        GlobalPermission::manageBookmarks, tr("Manage Bookmarks")));
                }
                else
                {
                    usersButton->setSubjectValidationPolicy(new QnDefaultSubjectValidationPolicy());
                }
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

            if (editorButton)
                connect(editorButton, SIGNAL(commit()), this, SLOT(at_editor_commit()));

            // TODO: #vbreus Create proper editor for fullscreenCameraAction
            return editorButton;
        }
        case Column::event:
        {
            using EventSubType = QnBusinessTypesComparator::EventSubType;
            auto comboBox = new QComboBox(parent);
            auto addItem = [this, comboBox](vms::api::EventType eventType)
                {
                    comboBox->addItem(m_businessStringsHelper->eventName(eventType), eventType);
                };

            comboBox->setMaxVisibleItems(comboBoxMaxVisibleItems);
            for (const auto eventType: m_lexComparator->lexSortedEvents(EventSubType::user))
                addItem(eventType);
            comboBox->insertSeparator(comboBox->count());
            for (const auto eventType: m_lexComparator->lexSortedEvents(EventSubType::failure))
                addItem(eventType);
            comboBox->insertSeparator(comboBox->count());
            for (const auto eventType: m_lexComparator->lexSortedEvents(EventSubType::success))
                addItem(eventType);
            return comboBox;
        }
        case Column::action:
        {
            auto eventType = index.data(Qn::EventTypeRole).value<vms::api::EventType>();
            auto eventParams = index.data(Qn::EventParametersRole).value<nx::vms::event::EventParameters>();
            bool isInstantOnly = !vms::event::hasToggleState(eventType, eventParams, commonModule());
            QComboBox* comboBox = new QComboBox(parent);
            comboBox->setMaxVisibleItems(comboBoxMaxVisibleItems);
            for (vms::api::ActionType actionType : m_lexComparator->lexSortedActions())
            {
                if (isInstantOnly && !vms::event::canBeInstant(actionType))
                    continue;
                comboBox->addItem(m_businessStringsHelper->actionName(actionType), actionType);
            }
            return comboBox;
        }
        case Column::aggregation:
        {
            nx::vms::client::desktop::AggregationWidget* widget =
                new nx::vms::client::desktop::AggregationWidget(parent);
            widget->setShort(true);
            return widget;
        }
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
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
            }
            return;
        case Column::action:
            if (QComboBox* comboBox = dynamic_cast<QComboBox*>(editor))
            {
                comboBox->setCurrentIndex(comboBox->findData(
                    index.data(Qn::ActionTypeRole).value<vms::api::ActionType>()));
                connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_editor_commit()));
            }
            return;
        case Column::aggregation:
            if (nx::vms::client::desktop::AggregationWidget* widget =
                dynamic_cast<nx::vms::client::desktop::AggregationWidget*>(editor))
            {
                widget->setValue(index.data(Qt::EditRole).toInt());
                connect(widget, SIGNAL(valueChanged()), this, SLOT(at_editor_commit()));
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
                model->setData(index, qVariantFromValue(btn->getResources()));
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

void QnBusinessRuleItemDelegate::at_editor_commit()
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
