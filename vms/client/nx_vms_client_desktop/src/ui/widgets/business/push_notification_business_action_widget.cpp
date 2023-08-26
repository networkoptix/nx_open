// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_business_action_widget.h"
#include "ui_push_notification_business_action_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>

#include <business/business_resource_validation.h>
#include <core/resource/user_resource.h>
#include <nx/branding.h>
#include <nx/network/app_info.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;
using namespace nx::vms::common;

namespace {

const int kMaxHeaderLength = 100;
const int kMaxBodyLength = 500;
const double kWarningThreshold = 0.8; //< Warning is shown when text length exceeds Max*Threshold.

// FocusWatcher calls eventReceiver() function when a watched widget takes focus.
// This widget is passed as a parameter. When focus is lost, eventReceiver(nullptr) is called.
class FocusWatcher: public QObject
{
public:
    FocusWatcher(const std::function<void(QWidget*)>& eventReceiver, QObject* parent = nullptr):
        QObject(parent),
        m_receiver(eventReceiver)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::FocusIn)
            m_receiver(qobject_cast<QWidget*>(watched));

        if (event->type() == QEvent::FocusOut)
            m_receiver(nullptr);

        return false;
    }

private:
    const std::function<void(QWidget*)> m_receiver;
};

} // namespace

namespace nx::vms::client::desktop {

PushNotificationBusinessActionWidget::PushNotificationBusinessActionWidget(
    QnWorkbenchContext* context,
    QWidget* parent)
    :
    base_type(context->systemContext(), parent),
    ui(new Ui::PushNotificationBusinessActionWidget),
    m_context(context),
    m_aligner(new Aligner(this))
{
    ui->setupUi(this);

    ui->notConnectedLabel->setText(
        tr("The system is not connected to %1. "
            "Mobile notifications work only when the system is connected to %1.",
            "%1 here will be substituted with cloud name e.g. 'Nx Cloud'.")
            .arg(nx::branding::cloudName()));

    ui->cloudSettingsButton->setText(
        tr("%1 Settings", "%1 here will be substituted with cloud name e.g. 'Nx Cloud'.")
            .arg(nx::branding::cloudName()));

    setHelpTopic(this, HelpTopic::Id::EventsActions_SendMobileNotification);

    ui->languageButton->setIcon(qnSkin->icon("events/push_language.svg"));
    connect(ui->languageButton, &QPushButton::clicked, this,
        [this]
        {
            m_context->menu()->trigger(ui::action::SystemAdministrationAction);
        });

    m_aligner->addWidgets(
        {ui->usersLabel, ui->dummyLabel, ui->headerLabel, ui->bodyLabel, ui->dummyLabel2});

    setSubjectsButton(ui->selectUsersButton);

    setValidationPolicy(new QnCloudUsersValidationPolicy());

    setDialogOptions(ui::SubjectSelectionDialog::CustomizableOptions::cloudUsers());

    connect(
        ui->cloudSettingsButton,
        &QPushButton::clicked,
        m_context->action(action::PreferencesCloudTabAction),
        &QAction::triggered);

    connect(
        systemSettings(),
        &SystemSettings::cloudSettingsChanged,
        this,
        &PushNotificationBusinessActionWidget::updateCurrentTab);
    updateCurrentTab();

    connect(ui->customContentCheckBox, &QCheckBox::toggled, this,
        [this](bool useCustomContent)
        {
            ui->customContentWidget->setVisible(useCustomContent);
            parametersChanged();
        });
    ui->customContentWidget->setVisible(ui->customContentCheckBox->isChecked());

    connect(ui->headerText, &QLineEdit::textChanged,
        this, &PushNotificationBusinessActionWidget::parametersChanged);
    connect(ui->bodyText, &QPlainTextEdit::textChanged,
        this, &PushNotificationBusinessActionWidget::parametersChanged);
    connect(ui->addSourceNameCheckBox, &QCheckBox::clicked,
        this, &PushNotificationBusinessActionWidget::parametersChanged);

    auto filter = new FocusWatcher([this](QWidget *widget){ updateLimitInfo(widget); }, this);
    ui->headerText->installEventFilter(filter);
    ui->bodyText->installEventFilter(filter);

    connect(ui->headerText, &QLineEdit::textChanged, this,
        [this]()
        {
            setWarningStyleOn(ui->headerText, ui->headerText->text().length() > kMaxHeaderLength);
            updateLimitInfo(ui->headerText);
        });
    connect(ui->bodyText, &QPlainTextEdit::textChanged, this,
        [this]()
        {
            setWarningStyleOn(ui->bodyText, ui->bodyText->toPlainText().length() > kMaxBodyLength);
            updateLimitInfo(ui->bodyText);
        });

    updateLimitInfo(nullptr); //< Hide label.
}

PushNotificationBusinessActionWidget::~PushNotificationBusinessActionWidget()
{
}

void PushNotificationBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    if (fields.testFlag(Field::actionParams))
    {
        QScopedValueRollback<bool> guard(m_updating, true);
        const auto params = model()->actionParams();

        const bool useCustomContent = !params.useSource
            || !params.text.isEmpty() || !params.sayText.isEmpty();

        ui->headerText->setText(params.sayText);
        ui->bodyText->setPlainText(params.text);
        ui->addSourceNameCheckBox->setChecked(params.useSource);
        ui->customContentCheckBox->setChecked(useCustomContent);

        updateLimitInfo(nullptr);
    }

    base_type::at_model_dataChanged(fields);
}

void PushNotificationBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->languageButton);
    setTabOrder(ui->languageButton, ui->customContentCheckBox);
    setTabOrder(ui->customContentCheckBox, ui->headerText);
    setTabOrder(ui->headerText, ui->bodyText);
    setTabOrder(ui->bodyText, ui->addSourceNameCheckBox);
    setTabOrder(ui->addSourceNameCheckBox, after);
}

void PushNotificationBusinessActionWidget::parametersChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    auto params = model()->actionParams();

    const bool useCustomContent = ui->customContentCheckBox->isChecked();

    params.sayText = useCustomContent ? ui->headerText->text().left(kMaxHeaderLength) : QString();
    params.text = useCustomContent ? ui->bodyText->toPlainText().left(kMaxBodyLength) : QString();
    params.useSource = useCustomContent ? ui->addSourceNameCheckBox->isChecked() : true;
    model()->setActionParams(params);

    updateSubjectsButton();
}

void PushNotificationBusinessActionWidget::updateCurrentTab()
{
    ui->stackedWidget->setCurrentIndex(systemSettings()->cloudSystemId().isEmpty() ? 0 : 1);
}

void PushNotificationBusinessActionWidget::updateLimitInfo(QWidget *textField)
{
    int current = 0, max = 0;

    if (textField == ui->headerText)
    {
        current = ui->headerText->text().length();
        max = kMaxHeaderLength;
    }
    else if (textField == ui->bodyText)
    {
        current = ui->bodyText->toPlainText().length();
        max = kMaxBodyLength;
    }
    else
    {
        ui->maxLengthLabel->hide();
        return;
    }

    if (current >= kWarningThreshold * max)
    {
        ui->maxLengthLabel->show();
        ui->maxLengthLabel->setText(
            current > max
                ? tr("%n symbols over", "", current - max)
                : tr("%n symbols left", "", max - current));
        setWarningStyleOn(ui->maxLengthLabel, current > max);
    }
    else
    {
        ui->maxLengthLabel->hide();
    }
}

} // namespace nx::vms::client::desktop
