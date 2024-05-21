// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edit_vms_rule_dialog.h"

#include <QtCore/QJsonDocument>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
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
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/utils/api.h>
#include <nx/vms/rules/utils/common.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/session_aware_dialog.h>

#include "dialog_details/action_type_picker_widget.h"
#include "dialog_details/event_type_picker_widget.h"
#include "dialog_details/styled_frame.h"
#include "params_widgets/action_parameters_widget.h"
#include "params_widgets/event_parameters_widget.h"
#include "utils/confirmation_dialogs.h"

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

QSet<vms::rules::State> getAvailableStates(
    const vms::rules::ItemDescriptor& eventDescriptor,
    const vms::rules::ItemDescriptor& actionDescriptor)
{
    QSet<vms::rules::State> availableEventStates;

    if (eventDescriptor.flags.testFlag(vms::rules::ItemFlag::instant))
        availableEventStates.insert(vms::rules::State::instant);

    if (eventDescriptor.flags.testFlag(vms::rules::ItemFlag::prolonged))
    {
        availableEventStates.insert(vms::rules::State::none);
        availableEventStates.insert(vms::rules::State::started);
        availableEventStates.insert(vms::rules::State::stopped);
    }

    QSet<vms::rules::State> availableActionStates;

    if (actionDescriptor.flags.testFlag(vms::rules::ItemFlag::instant))
    {
        availableActionStates.insert(vms::rules::State::instant);
        availableActionStates.insert(vms::rules::State::started);
        availableActionStates.insert(vms::rules::State::stopped);
    }

    if (actionDescriptor.flags.testFlag(vms::rules::ItemFlag::prolonged))
        availableActionStates.insert({vms::rules::State::none});

    if (const auto durationFieldDescriptor =
        vms::rules::utils::fieldByName(vms::rules::utils::kDurationFieldName, actionDescriptor))
    {
        availableActionStates.insert(vms::rules::State::started);
        availableActionStates.insert(vms::rules::State::stopped);
    }

    return availableEventStates & availableActionStates;
}

std::chrono::seconds getDefaultDuration(vms::rules::OptionalTimeField* optionalTimeField)
{
    const auto defaultValue = optionalTimeField->properties().defaultValue;
    if (!NX_ASSERT(defaultValue != std::chrono::seconds::zero()))
    {
        constexpr auto kDefaultActionDuration = std::chrono::seconds{5};
        return kDefaultActionDuration;
    }

    return defaultValue;
}

void fixStateAndDuration(
    vms::rules::Engine* engine,
    vms::rules::EventFilter* eventFilter,
    vms::rules::ActionBuilder* actionBuilder)
{
    using namespace vms::rules;

    if (utils::isCompatible(engine, eventFilter, actionBuilder))
        return;

    auto actionDescriptor = engine->actionDescriptor(actionBuilder->actionType());
    auto eventDescriptor = engine->eventDescriptor(eventFilter->eventType());

    auto durationField = actionBuilder->fieldByName<OptionalTimeField>(utils::kDurationFieldName);
    auto stateField = eventFilter->fieldByName<StateField>(utils::kStateFieldName);

    if (stateField)
    {
        const auto availableStates =
            getAvailableStates(eventDescriptor.value(), actionDescriptor.value());
        if (!NX_ASSERT(!availableStates.isEmpty()))
            return;

        if (durationField)
        {
            if (durationField->value() != std::chrono::microseconds::zero())
            {
                if (availableStates.contains(State::instant))
                {
                    stateField->setValue(State::instant);
                }
                else if (availableStates.contains(State::started))
                {
                    stateField->setValue(State::started);
                }
                else
                {
                    stateField->setValue(State::none);
                    durationField->setValue(getDefaultDuration(durationField));
                }
            }
            else
            {
                if (availableStates.contains(State::none))
                {
                    stateField->setValue(State::none);
                }
                else
                {
                    stateField->setValue(*availableStates.cbegin());
                    durationField->setValue(getDefaultDuration(durationField));
                }
            }
        }
        else
        {
            if (!availableStates.contains(stateField->value()))
                stateField->setValue(*availableStates.cbegin());
        }
    }
    else if (durationField)
    {
        if (utils::isInstantOnly(eventDescriptor.value())
            && durationField->value() == std::chrono::microseconds::zero())
        {
            // Duration can not be zero for the instant event.
            durationField->setValue(getDefaultDuration(durationField));
        }
    }
}

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
        buttonBox->setStandardButtons(
            QDialogButtonBox::Cancel | QDialogButtonBox::Ok | QDialogButtonBox::Apply);
        buttonBox->setCenterButtons(false);
        footerLayout->addWidget(buttonBox);

        connect(this,
            &EditVmsRuleDialog::hasChangesChanged,
            [buttonBox, this]()
            {
                if (auto acceptButton = buttonBox->button(QDialogButtonBox::StandardButton::Apply))
                {
                    if (acceptButton->isEnabled() != m_hasChanges)
                        acceptButton->setEnabled(m_hasChanges);
                }
            });
        mainLayout->addLayout(footerLayout);
    }

    mainLayout->setStretch(1, 1);
}

void EditVmsRuleDialog::setRule(std::shared_ptr<vms::rules::Rule> rule)
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

    displayComment();
    displayRule();
    displayState();
    displayControls();
}

void EditVmsRuleDialog::accept()
{
    showWarningIfRequired();
    done(QDialogButtonBox::Ok);
}

void EditVmsRuleDialog::reject()
{
    done(QDialogButtonBox::Cancel);
}

void EditVmsRuleDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button)
{
    if (button == QDialogButtonBox::StandardButton::Apply)
    {
        showWarningIfRequired();
        emit accepted();
        setHasChanges(false);
        return;
    }

    QnSessionAwareButtonBoxDialog::buttonBoxClicked(button);
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

    if (ini().developerMode)
    {
        onEventFilterModified();
        onActionBuilderModified();

        m_scopedConnections.reset();

        m_scopedConnections << connect(
            m_rule->eventFilters().first(),
            &vms::rules::EventFilter::changed,
            this,
            &EditVmsRuleDialog::onEventFilterModified);

        m_scopedConnections << connect(
            m_rule->actionBuilders().first(),
            &vms::rules::ActionBuilder::changed,
            this,
            &EditVmsRuleDialog::onActionBuilderModified);
    }

    displayEventEditor();
    displayActionEditor();
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

    actionEditorWidget->setRule(m_rule);
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

void EditVmsRuleDialog::onActionTypeChanged(const QString& actionType)
{
    const auto engine = systemContext()->vmsRulesEngine();

    auto actionBuilder = engine->buildActionBuilder(actionType);
    auto eventFilter = m_rule->eventFilters().at(0);

    auto actionDescriptor = engine->actionDescriptor(actionType);
    auto eventDescriptor = engine->eventDescriptor(eventFilter->eventType());

    if (!vms::rules::utils::isCompatible(eventDescriptor.value(), actionDescriptor.value()))
    {
        QnSessionAwareMessageBox::warning(this,
            tr("Incompatible event-action pair is choosen."
                " Prolonged action without duration incompatible with the instant event"));
        return;
    }

    fixStateAndDuration(engine, eventFilter, actionBuilder.get());

    if (!NX_ASSERT(
        vms::rules::utils::isCompatible(engine, eventFilter, actionBuilder.get()),
        "Fixing rule compatibility failed"))
    {
        return;
    }

    m_rule->takeActionBuilder(0);
    m_rule->addActionBuilder(std::move(actionBuilder));
    setHasChanges(true);

    displayRule();
}

void EditVmsRuleDialog::onEventTypeChanged(const QString& eventType)
{
    const auto engine = systemContext()->vmsRulesEngine();

    auto eventFilter = engine->buildEventFilter(eventType);
    auto actionBuilder = m_rule->actionBuilders().at(0);

    auto actionDescriptor = engine->actionDescriptor(actionBuilder->actionType());
    auto eventDescriptor = engine->eventDescriptor(eventType);

    if (!vms::rules::utils::isCompatible(eventDescriptor.value(), actionDescriptor.value()))
    {
        QnSessionAwareMessageBox::warning(this,
            tr("Incompatible event-action pair is choosen."
                " Prolonged action without duration incompatible with the instant event"));
        return;
    }

    fixStateAndDuration(engine, eventFilter.get(), actionBuilder);

    if (!NX_ASSERT(
        vms::rules::utils::isCompatible(engine, eventFilter.get(), actionBuilder),
        "Fixing rule compatibility failed"))
    {
        return;
    }

    m_rule->takeEventFilter(0);
    m_rule->addEventFilter(std::move(eventFilter));
    setHasChanges(true);

    displayRule();
}

void EditVmsRuleDialog::onEnabledButtonClicked(bool checked)
{
    m_rule->setEnabled(checked);
    setHasChanges(true);
}

void EditVmsRuleDialog::onEventFilterModified()
{
    const auto serializedEventFilter =
        QJson::serialized(serialize(m_rule->eventFilters().first()));
    m_eventLabel->setToolTip(indentedJson(serializedEventFilter));
    setHasChanges(true);
}

void EditVmsRuleDialog::onActionBuilderModified()
{
    const auto serializedActionBuilder =
        QJson::serialized(serialize(m_rule->actionBuilders().first()));
    m_actionLabel->setToolTip(indentedJson(serializedActionBuilder));
    setHasChanges(true);
}

void EditVmsRuleDialog::setHasChanges(bool hasChanges)
{
    if (hasChanges == m_hasChanges)
        return;

    m_hasChanges = hasChanges;
    emit hasChangesChanged();
}

void EditVmsRuleDialog::showWarningIfRequired()
{
    if (m_hasChanges && m_rule->validity(systemContext()) != QValidator::State::Acceptable)
    {
        QnMessageBox::warning(
            this,
            tr("The rule is not valid and may not work"),
            /*extras*/ {},
            QDialogButtonBox::Ok,
            QDialogButtonBox::Ok);
    }
}

} // namespace nx::vms::client::desktop::rules
