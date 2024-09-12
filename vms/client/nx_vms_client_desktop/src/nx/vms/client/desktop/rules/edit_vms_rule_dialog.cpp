// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edit_vms_rule_dialog.h"

#include <QtCore/QJsonDocument>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/rules/action_builder.h>
#include <nx/vms/api/rules/event_filter.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/panel.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/dialogs/week_time_schedule_dialog.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_devices_field.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/utils/api.h>
#include <nx/vms/rules/utils/common.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/rule.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/session_aware_dialog.h>

#include "dialog_details/action_type_picker_widget.h"
#include "dialog_details/event_type_picker_widget.h"
#include "dialog_details/styled_frame.h"
#include "helpers/rule_compatibility_manager.h"
#include "params_widgets/action_parameters_widget.h"
#include "params_widgets/event_parameters_widget.h"
#include "utils/confirmation_dialogs.h"
#include "utils/strings.h"

namespace nx::vms::client::desktop::rules {

namespace {

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kThemeSubstitutions = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light17"}},
    {QIcon::Selected, {.primary = "light15"}},
};

NX_DECLARE_COLORIZED_ICON(kCalendarIcon, "20x20/Outline/calendar.svg", kThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kDeleteIcon, "20x20/Outline/delete.svg", kThemeSubstitutions)
NX_DECLARE_COLORIZED_ICON(kEditIcon, "20x20/Outline/edit.svg", kThemeSubstitutions)

QString indentedJson(const QByteArray& json)
{
    return QJsonDocument::fromJson(json).toJson();
}

} // namespace

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
        m_editableLabel->setButtonIcon(qnSkin->icon(kEditIcon));
        m_editableLabel->setPlaceholderText(tr("Add Title or Comment"));
        m_editableLabel->setFont(fontConfig()->xLarge());
        m_editableLabel->setValidator({});
        connect(
            m_editableLabel,
            &EditableLabel::textChanged,
            this,
            &EditVmsRuleDialog::onCommentChanged);
        headerLayout->addWidget(m_editableLabel);

        headerLayout->addStretch();

        auto scheduleButton = new QPushButton;
        scheduleButton->setText(tr("Schedule"));
        scheduleButton->setIcon(qnSkin->icon(kCalendarIcon));
        scheduleButton->setFlat(true);
        connect(
            scheduleButton,
            &QPushButton::clicked,
            this,
            &EditVmsRuleDialog::onScheduleClicked);
        headerLayout->addWidget(scheduleButton);

        m_deleteButton = new QPushButton;
        m_deleteButton->setText(tr("Delete"));
        m_deleteButton->setIcon(qnSkin->icon(kDeleteIcon));
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
        contentLayout->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);
        contentLayout->setSizeConstraint(QLayout::SetMinimumSize);

        auto eventLayout = new QVBoxLayout;
        eventLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.height());

        m_eventLabel = new QLabel;
        m_eventLabel->setText(
            QString{"%1 %2"}
                .arg(common::html::colored(tr("WHEN"), core::colorTheme()->color("light4")))
                .arg(common::html::colored(tr("EVENT"), core::colorTheme()->color("light10"))));
        eventLayout->addWidget(m_eventLabel);

        auto eventFrame = new StyledFrame;
        eventFrame->setContentsMargins(
            0,
            style::Metrics::kDefaultTopLevelMargin,
            0,
            style::Metrics::kDefaultTopLevelMargin);

        auto eventFrameLayout = new QVBoxLayout{eventFrame};
        eventFrameLayout->setSpacing(0);
        eventFrameLayout->setSizeConstraint(QLayout::SetMinimumSize);

        m_eventTypePicker = new EventTypePickerWidget{this->systemContext()};
        connect(
            m_eventTypePicker,
            &EventTypePickerWidget::eventTypePicked,
            this,
            [this](const QString& eventType)
            {
                m_ruleCompatibilityManager->changeEventType(eventType);
            });
        eventFrameLayout->addWidget(m_eventTypePicker);

        m_eventEditorWidget = new EventParametersWidget{windowContext()};
        eventFrameLayout->addWidget(m_eventEditorWidget);

        eventLayout->addWidget(eventFrame);
        eventLayout->addStretch(1);

        contentLayout->addLayout(eventLayout);

        auto separator = new QFrame;
        separator->setFrameShape(QFrame::VLine);
        separator->setFrameShadow(QFrame::Sunken);
        contentLayout->addWidget(separator);

        auto actionLayout = new QVBoxLayout;
        actionLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        m_actionLabel = new QLabel;
        m_actionLabel->setText(
            QString{"%1 %2"}
                .arg(common::html::colored(tr("DO"), core::colorTheme()->color("light4")))
                .arg(common::html::colored(tr("ACTION"), core::colorTheme()->color("light10"))));
        actionLayout->addWidget(m_actionLabel);

        auto actionFrame = new StyledFrame;
        actionFrame->setContentsMargins(
            0,
            style::Metrics::kDefaultTopLevelMargin,
            0,
            style::Metrics::kDefaultTopLevelMargin);

        auto actionFrameLayout = new QVBoxLayout{actionFrame};
        actionFrameLayout->setSpacing(0);
        actionFrameLayout->setSizeConstraint(QLayout::SetMinimumSize);

        m_actionTypePicker = new ActionTypePickerWidget{this->systemContext()};
        connect(
            m_actionTypePicker,
            &ActionTypePickerWidget::actionTypePicked,
            this,
            [this](const QString& actionType)
            {
                m_ruleCompatibilityManager->changeActionType(actionType);
            });
        actionFrameLayout->addWidget(m_actionTypePicker);

        m_actionEditorWidget = new ActionParametersWidget{windowContext()};
        actionFrameLayout->addWidget(m_actionEditorWidget);

        actionLayout->addWidget(actionFrame);
        actionLayout->addStretch(1);

        contentLayout->addLayout(actionLayout);
        contentLayout->setSizeConstraint(QLayout::SetMinimumSize);

        m_scrollArea = new QScrollArea;
        m_scrollArea->setWidget(m_contentWidget);
        m_scrollArea->setWidgetResizable(true);

        mainLayout->addWidget(m_scrollArea);
    }

    {
        auto buttonBoxLine = new QFrame;
        buttonBoxLine->setFrameShape(QFrame::HLine);
        buttonBoxLine->setFrameShadow(QFrame::Sunken);
        mainLayout->addWidget(buttonBoxLine);
    }

    {
        m_alertLabel = new AlertLabel{this}; //< Visible in dev mode only.
        m_alertLabel->setType(AlertLabel::Type::warning);
        m_alertLabel->setVisible(false);
        mainLayout->addWidget(m_alertLabel);
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

        m_buttonBox = new QDialogButtonBox;
        m_buttonBox->setOrientation(Qt::Horizontal);
        m_buttonBox->setStandardButtons(
            QDialogButtonBox::Cancel | QDialogButtonBox::Ok | QDialogButtonBox::Apply);
        m_buttonBox->setCenterButtons(false);
        footerLayout->addWidget(m_buttonBox);

        mainLayout->addLayout(footerLayout);
    }

    mainLayout->setStretch(1, 1);
}

void EditVmsRuleDialog::setRule(std::shared_ptr<vms::rules::Rule> rule, bool isNewRule)
{
    if (!NX_ASSERT(!m_rule) || !NX_ASSERT(rule))
        return;

    auto engine = systemContext()->vmsRulesEngine();
    if (rule->eventFilters().empty())
    {
        auto eventFilter = engine->buildEventFilter(m_eventTypePicker->eventType());

        if (!NX_ASSERT(eventFilter))
            return;

        rule->addEventFilter(std::move(eventFilter));
    }

    if (rule->actionBuilders().empty())
    {
        auto actionBuilder = engine->buildActionBuilder(m_actionTypePicker->actionType());
        if (!NX_ASSERT(actionBuilder))
            return;

        rule->addActionBuilder(std::move(actionBuilder));
    }

    m_rule = std::move(rule);
    m_eventDurationType =
        vms::rules::getEventDurationType(engine, m_rule->eventFilters().first());
    m_ruleCompatibilityManager =
        std::make_unique<RuleCompatibilityManager>(m_rule.get(), systemContext());

    connect(
        m_ruleCompatibilityManager.get(),
        &RuleCompatibilityManager::ruleModified,
        this,
        [this]
        {
            const auto eventDurationType = vms::rules::getEventDurationType(
                systemContext()->vmsRulesEngine(), m_rule->eventFilters().first());

            if (m_eventEditorWidget->eventType() != m_rule->eventFilters().first()->eventType()
                || m_actionEditorWidget->actionType() != m_rule->actionBuilders().first()->actionType()
                || m_eventDurationType != eventDurationType)
            {
                // UI rebuild required.

                m_eventDurationType = eventDurationType;

                displayRule();
                setHasChanges(true);

                return;
            }

            m_eventEditorWidget->updateUi();
            m_actionEditorWidget->updateUi();

            updateEnabledActions();
            setHasChanges(true);
            displayDeveloperModeInfo();
        },
        Qt::QueuedConnection);

    m_isNewRule = isNewRule;
    m_checkValidityOnChanges = !isNewRule;

    displayComment();
    displayRule();
    displayState();
    displayControls();
}

void EditVmsRuleDialog::accept()
{
    if (ruleValidity() == QValidator::State::Invalid)
        return;

    showWarningIfRequired();
    done(QDialogButtonBox::Ok);
}

void EditVmsRuleDialog::reject()
{
    done(QDialogButtonBox::Cancel);
}

void EditVmsRuleDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button)
{
    if (button == QDialogButtonBox::StandardButton::Apply
        || button == QDialogButtonBox::StandardButton::Ok)
    {
        if (!m_checkValidityOnChanges && ruleValidity() == QValidator::State::Invalid)
        {
            m_eventEditorWidget->setEdited();
            m_eventEditorWidget->updateUi();

            m_actionEditorWidget->setEdited();
            m_actionEditorWidget->updateUi();

            m_checkValidityOnChanges = true;

            updateButtonBox();

            return;
        }

        if (button == QDialogButtonBox::StandardButton::Apply)
        {
            showWarningIfRequired();
            emit accepted();
            setHasChanges(false);
            return;
        }
    }

    QnSessionAwareButtonBoxDialog::buttonBoxClicked(button);
}

void EditVmsRuleDialog::displayComment()
{
    m_editableLabel->setText(m_rule->comment());
}

void EditVmsRuleDialog::displayRule()
{
    if (!NX_ASSERT(m_rule->isValid()))
        return;

    m_eventTypePicker->setEventType(m_rule->eventFilters().first()->eventType());
    m_actionTypePicker->setActionType(m_rule->actionBuilders().first()->actionType());

    updateEnabledActions();

    displayEventEditor();
    displayActionEditor();
    displayDeveloperModeInfo();
}

void EditVmsRuleDialog::displayActionEditor()
{
    auto actionEditorWidget = new ActionParametersWidget{windowContext()};

    auto replacedLayoutItem = m_actionEditorWidget->parentWidget()->layout()->replaceWidget(
        m_actionEditorWidget, actionEditorWidget);
    if (NX_ASSERT(replacedLayoutItem))
    {
        delete replacedLayoutItem->widget();
        delete replacedLayoutItem;
    }

    m_actionEditorWidget = actionEditorWidget;

    m_actionEditorWidget->setRule(m_rule, m_isNewRule);

    if (m_checkValidityOnChanges)
        m_actionEditorWidget->setEdited();
}

void EditVmsRuleDialog::displayEventEditor()
{
    auto eventEditorWidget = new EventParametersWidget{windowContext()};

    auto replacedLayoutItem = m_eventEditorWidget->parentWidget()->layout()->replaceWidget(
        m_eventEditorWidget, eventEditorWidget);
    if (NX_ASSERT(replacedLayoutItem))
    {
        delete replacedLayoutItem->widget();
        delete replacedLayoutItem;
    }

    m_eventEditorWidget = eventEditorWidget;

    m_eventEditorWidget->setRule(m_rule, m_isNewRule);

    if (m_checkValidityOnChanges)
        eventEditorWidget->setEdited();
}

void EditVmsRuleDialog::displayDeveloperModeInfo()
{
    if (ini().developerMode)
    {
        const auto serializedEventFilter =
            QJson::serialized(serialize(m_rule->eventFilters().first()));
        m_eventLabel->setToolTip(QString{"%1:\n%2"}
                                    .arg(Strings::devModeInfoTitle())
                                    .arg(indentedJson(serializedEventFilter)));

        const auto serializedActionBuilder =
            QJson::serialized(serialize(m_rule->actionBuilders().first()));
        m_actionLabel->setToolTip(QString{"%1:\n%2"}
                                    .arg(Strings::devModeInfoTitle())
                                    .arg(indentedJson(serializedActionBuilder)));

        const auto validity = ruleValidity();
        if (validity.isValid())
        {
            m_alertLabel->setVisible(false);
        }
        else
        {
            m_alertLabel->setVisible(true);
            m_alertLabel->setText(QString{"%1\n%2"}
                .arg(Strings::devModeInfoTitle())
                .arg(validity.description));
        }
    }
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
    setHasChanges(true);
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
    setHasChanges(true);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_rule->setSchedule(dialog.schedule());
}

void EditVmsRuleDialog::onEnabledButtonClicked(bool checked)
{
    m_rule->setEnabled(checked);
    setHasChanges(true);
}

void EditVmsRuleDialog::updateButtonBox()
{
    // For the new rule, all the buttons must be visible. After clicking the Ok or Apply button,
    // the rule must be validated. If the rule is invalid, it must not be saved, and the dialog
    // must remain open, with the Ok and Apply buttons disabled until the rule has been fixed. For
    // existing rules, the Ok and apply buttons must be disabled if the rule is not valid.

    auto isOkButtonEnabled{true};
    auto isApplyButtonEnabled{m_hasChanges};

    if (m_checkValidityOnChanges && ruleValidity() == QValidator::State::Invalid)
    {
        isOkButtonEnabled = false;
        isApplyButtonEnabled = false;
    }

    m_buttonBox->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(isOkButtonEnabled);
    m_buttonBox->button(QDialogButtonBox::StandardButton::Apply)->setEnabled(isApplyButtonEnabled);
}

void EditVmsRuleDialog::updateEnabledActions()
{
    const auto eventDurationType =
        vms::rules::getEventDurationType(m_rule->engine(), m_rule->eventFilters().first());
    m_actionTypePicker->setProlongedActionsEnabled(
        eventDurationType != vms::rules::EventDurationType::instant);
}

void EditVmsRuleDialog::setHasChanges(bool hasChanges)
{
    m_hasChanges = hasChanges;

    updateButtonBox();
}

void EditVmsRuleDialog::showWarningIfRequired()
{
    if (m_hasChanges && ruleValidity() != QValidator::State::Acceptable)
    {
        QnMessageBox::warning(
            this,
            tr("The rule is not valid and may not work"),
            /*extras*/ {},
            QDialogButtonBox::Ok,
            QDialogButtonBox::Ok);
    }
}

vms::rules::ValidationResult EditVmsRuleDialog::ruleValidity() const
{
    return vms::rules::utils::visibleFieldsValidity(m_rule.get(), systemContext());
}

} // namespace nx::vms::client::desktop::rules
