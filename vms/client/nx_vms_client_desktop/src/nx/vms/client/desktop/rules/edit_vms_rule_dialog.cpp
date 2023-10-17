// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edit_vms_rule_dialog.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/panel.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/dialogs/week_time_schedule_dialog.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/events/debug_event.h>
#include <ui/common/palette.h>

#include "dialog_details/action_type_picker_widget.h"
#include "dialog_details/event_type_picker_widget.h"
#include "dialog_details/styled_frame.h"
#include "params_widgets/editor_factory.h"
#include "utils/confirmation_dialogs.h"

namespace {

static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight16Color, "light17"}}},
    {QIcon::Selected, {{kLight16Color, "light15"}}},
};

} // namespace

namespace nx::vms::client::desktop::rules {

EditVmsRuleDialog::EditVmsRuleDialog(QWidget* parent):
    QnSessionAwareButtonBoxDialog{parent}
{
    resize(985, 704);
    setMinimumSize(QSize(800, 600));

    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dark7"));

    auto mainLayout = new QVBoxLayout{this};
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(style::Metrics::kDefaultChildMargins);

    {
        auto headerWidget = new Panel;

        auto headerLayout = new QHBoxLayout{headerWidget};
        headerLayout->setContentsMargins(
            style::Metrics::kDefaultTopLevelMargin,
            20,
            style::Metrics::kDefaultTopLevelMargin,
            20);
        headerLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        m_editableLabel = new EditableLabel;
        m_editableLabel->setButtonIcon(qnSkin->pixmap("text_buttons/edit.png"));
        connect(
            m_editableLabel,
            &EditableLabel::textChanged,
            this,
            &EditVmsRuleDialog::onCommentChanged);
        headerLayout->addWidget(m_editableLabel);

        headerLayout->addStretch();

        auto scheduleButton = new QPushButton;
        scheduleButton->setText(tr("Schedule"));
        scheduleButton->setIcon(qnSkin->icon("text_buttons/calendar_20.svg", kIconSubstitutions));
        scheduleButton->setFlat(true);
        connect(
            scheduleButton,
            &QPushButton::clicked,
            this,
            &EditVmsRuleDialog::onScheduleClicked);
        headerLayout->addWidget(scheduleButton);

        m_deleteButton = new QPushButton;
        m_deleteButton->setText(tr("Delete"));
        m_deleteButton->setIcon(qnSkin->icon("text_buttons/delete_20_deprecated.svg", kIconSubstitutions));
        m_deleteButton->setFlat(true);
        connect(
            m_deleteButton,
            &QPushButton::clicked,
            this,
            &EditVmsRuleDialog::onDeleteClicked);
        headerLayout->addWidget(m_deleteButton);

        mainLayout->addWidget(headerWidget);
    }

    {
        m_contentWidget = new QWidget;
        auto contentLayout = new QHBoxLayout{m_contentWidget};

        auto eventLayout = new QVBoxLayout;
        eventLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.height());

        auto eventLabel = new QLabel;
        eventLabel->setText(
            QString{"%1 %2"}
                .arg(common::html::colored(tr("WHEN"), core::colorTheme()->color("light4")))
                .arg(common::html::colored(tr("EVENT"), core::colorTheme()->color("light10"))));
        eventLayout->addWidget(eventLabel);

        auto eventFrame = new StyledFrame;
        eventFrame->setContentsMargins(
            0,
            style::Metrics::kDefaultTopLevelMargin,
            0,
            style::Metrics::kDefaultTopLevelMargin);

        auto eventFrameLayout = new QVBoxLayout{eventFrame};
        eventFrameLayout->setSpacing(0);
        m_eventTypePicker = new EventTypePickerWidget{this->systemContext()};
        connect(
            m_eventTypePicker,
            &EventTypePickerWidget::eventTypePicked,
            this,
            &EditVmsRuleDialog::onEventTypeChanged);
        eventFrameLayout->addWidget(m_eventTypePicker);

        m_eventEditorWidget = new QWidget;
        eventFrameLayout->addWidget(m_eventEditorWidget);

        eventFrameLayout->addStretch();

        eventLayout->addWidget(eventFrame);

        eventLayout->addStretch(1);

        contentLayout->addLayout(eventLayout);

        auto separator = new QFrame;
        separator->setFrameShape(QFrame::VLine);
        separator->setFrameShadow(QFrame::Sunken);
        contentLayout->addWidget(separator);

        auto actionLayout = new QVBoxLayout;
        actionLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        auto actionLabel = new QLabel;
        actionLabel->setText(
            QString{"%1 %2"}
                .arg(common::html::colored(tr("DO"), core::colorTheme()->color("light4")))
                .arg(common::html::colored(tr("ACTION"), core::colorTheme()->color("light10"))));
        actionLayout->addWidget(actionLabel);

        auto actionFrame = new StyledFrame;
        actionFrame->setContentsMargins(
            0,
            style::Metrics::kDefaultTopLevelMargin,
            0,
            style::Metrics::kDefaultTopLevelMargin);

        auto actionFrameLayout = new QVBoxLayout{actionFrame};
        actionFrameLayout->setSpacing(0);
        m_actionTypePicker = new ActionTypePickerWidget{this->systemContext()};
        connect(
            m_actionTypePicker,
            &ActionTypePickerWidget::actionTypePicked,
            this,
            &EditVmsRuleDialog::onActionTypeChanged);
        actionFrameLayout->addWidget(m_actionTypePicker);

        m_actionEditorWidget = new QWidget;
        actionFrameLayout->addWidget(m_actionEditorWidget);

        actionFrameLayout->addStretch();

        actionLayout->addWidget(actionFrame);

        actionLayout->addStretch(1);

        contentLayout->addLayout(actionLayout);

        mainLayout->addWidget(m_contentWidget);
    }

    {
        auto buttonBoxLine = new QFrame;
        buttonBoxLine->setFrameShape(QFrame::HLine);
        buttonBoxLine->setFrameShadow(QFrame::Sunken);
        mainLayout->addWidget(buttonBoxLine);
    }

    {
        auto footerLayout = new QHBoxLayout;
        footerLayout->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);
        footerLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        m_enabledButton = new QPushButton;
        m_enabledButton->setCheckable(true);
        m_enabledButton->setFlat(true);
        m_enabledButton->setText(tr("Enabled"));
        connect(
            m_enabledButton,
            &QPushButton::clicked,
            this,
            &EditVmsRuleDialog::onEnabledButtonClicked);
        footerLayout->addWidget(m_enabledButton);

        auto buttonBox = new QDialogButtonBox;
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        buttonBox->setCenterButtons(false);
        footerLayout->addWidget(buttonBox);

        mainLayout->addLayout(footerLayout);
    }

    mainLayout->setStretch(1, 1);
}

void EditVmsRuleDialog::setRule(std::shared_ptr<vms::rules::Rule> rule)
{
    if (!NX_ASSERT(!m_rule) || !NX_ASSERT(rule))
        return;

    m_rule = std::move(rule);

    displayComment();
    displayRule();
    displayState();
    displayControls();
}

void EditVmsRuleDialog::accept()
{
    done(QDialogButtonBox::Ok);
}

void EditVmsRuleDialog::reject()
{
    done(QDialogButtonBox::Cancel);
}

void EditVmsRuleDialog::displayComment()
{
    m_editableLabel->setText(m_rule->comment());
}

void EditVmsRuleDialog::displayRule()
{
    if (!NX_ASSERT(!m_rule->actionBuilders().empty()) || !NX_ASSERT(!m_rule->eventFilters().empty()))
        return;

    {
        const QSignalBlocker eventPickerBlocker{m_eventTypePicker};
        m_eventTypePicker->setEventType(m_rule->eventFilters().first()->eventType());
        const QSignalBlocker actionPickerBlocker{m_actionTypePicker};
        m_actionTypePicker->setActionType(m_rule->actionBuilders().first()->actionType());
    }

    displayEventEditor(m_rule->eventFilters().first()->eventType());
    displayActionEditor(m_rule->actionBuilders().first()->actionType());
}

void EditVmsRuleDialog::displayActionEditor(const QString& actionType)
{
    const auto actionDescriptor = systemContext()->vmsRulesEngine()->actionDescriptor(actionType);
    if (!NX_ASSERT(actionDescriptor))
        return;

    auto actionEditorWidget =
        ActionEditorFactory::createWidget(actionDescriptor.value(), windowContext());
    if (!NX_ASSERT(actionEditorWidget))
        return;

    auto replacedLayoutItem = m_actionEditorWidget->parentWidget()->layout()->replaceWidget(
        m_actionEditorWidget, actionEditorWidget);
    if (NX_ASSERT(replacedLayoutItem))
    {
        delete replacedLayoutItem->widget();
        delete replacedLayoutItem;
    }

    m_actionEditorWidget = actionEditorWidget;

    actionEditorWidget->setRule(m_rule);
}

void EditVmsRuleDialog::displayEventEditor(const QString& eventType)
{
    const auto eventDescriptor = systemContext()->vmsRulesEngine()->eventDescriptor(eventType);
    if (!NX_ASSERT(eventDescriptor))
        return;

    auto eventEditorWidget =
        EventEditorFactory::createWidget(eventDescriptor.value(), windowContext());
    if (!NX_ASSERT(eventEditorWidget))
        return;

    auto replacedLayoutItem = m_eventEditorWidget->parentWidget()->layout()->replaceWidget(
        m_eventEditorWidget, eventEditorWidget);
    if (NX_ASSERT(replacedLayoutItem))
    {
        delete replacedLayoutItem->widget();
        delete replacedLayoutItem;
    }

    m_eventEditorWidget = eventEditorWidget;

    eventEditorWidget->setRule(m_rule);
}

void EditVmsRuleDialog::displayState()
{
    m_enabledButton->setChecked(m_rule->enabled());
}

void EditVmsRuleDialog::displayControls()
{
    if (!systemContext()->vmsRulesEngine()->rule(m_rule->id()))
        m_deleteButton->setVisible(false);
}

void EditVmsRuleDialog::onCommentChanged(const QString& comment)
{
    m_rule->setComment(comment);
}

void EditVmsRuleDialog::onDeleteClicked()
{
    if (ConfirmationDialogs::confirmDelete(this))
        done(QDialogButtonBox::Reset);
}

void EditVmsRuleDialog::onScheduleClicked()
{
    WeekTimeScheduleDialog dialog(this, /*isEmptyAllowed*/ false);
    dialog.setSchedule(m_rule->schedule());
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_rule->setSchedule(dialog.schedule());
}

void EditVmsRuleDialog::onActionTypeChanged(const QString& actionType)
{
    m_rule->takeActionBuilder(0);

    auto actionBuilder = systemContext()->vmsRulesEngine()->buildActionBuilder(actionType);
    m_rule->addActionBuilder(std::move(actionBuilder));

    displayRule();
}

void EditVmsRuleDialog::onEventTypeChanged(const QString& eventType)
{
    m_rule->takeEventFilter(0);

    auto eventFilter = systemContext()->vmsRulesEngine()->buildEventFilter(eventType);
    m_rule->addEventFilter(std::move(eventFilter));

    displayRule();
}

void EditVmsRuleDialog::onEnabledButtonClicked(bool checked)
{
    m_rule->setEnabled(checked);
}

} // namespace nx::vms::client::desktop::rules
