// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "popup_business_action_widget.h"
#include "ui_popup_business_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <business/business_resource_validation.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

QnPopupBusinessActionWidget::QnPopupBusinessActionWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::PopupBusinessActionWidget)
{
    ui->setupUi(this);

    // It was decided not to include custom desktop notification text in v4.1.
    // TODO: remove the following lines in the next release.
    ui->customTextCheckBox->hide();
    ui->customTextEdit->hide();

    setHelpTopic(this, HelpTopic::Id::EventsActions_ShowDesktopNotification);

    ui->hintLabel->setHintText(tr("Notification will be shown until one of the users who see it "
        "creates bookmark with event description"));

    connect(ui->forceAcknoledgementCheckBox, &QCheckBox::toggled,
        this, &QnPopupBusinessActionWidget::parametersChanged);

    connect(ui->customTextCheckBox, &QCheckBox::clicked, this,
        [this]()
        {
            const bool useCustomText = ui->customTextCheckBox->isChecked();

            if (!useCustomText)
                m_lastCustomText = ui->customTextEdit->toPlainText();

            ui->customTextEdit->setPlainText(useCustomText ? m_lastCustomText : QString());

            parametersChanged();
        });
    connect(ui->customTextEdit, &QPlainTextEdit::textChanged,
        this, &QnPopupBusinessActionWidget::parametersChanged);

    ui->customTextEdit->setEnabled(ui->customTextCheckBox->isChecked());
    connect(ui->customTextCheckBox, &QCheckBox::toggled, ui->customTextEdit, &QWidget::setEnabled);

    setSubjectsButton(ui->selectUsersButton);
}

QnPopupBusinessActionWidget::~QnPopupBusinessActionWidget()
{
}

void QnPopupBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    if (fields.testFlag(Field::eventType))
    {
        const auto sourceCameraRequired =
            nx::vms::event::isSourceCameraRequired(model()->eventType());
        const auto allowForceAcknoledgement = sourceCameraRequired
            || model()->eventType() >= nx::vms::api::EventType::userDefinedEvent;
        ui->forceAcknoledgementCheckBox->setEnabled(allowForceAcknoledgement);
        ui->hintLabel->setEnabled(allowForceAcknoledgement);
    }

    if (fields.testFlag(Field::actionParams))
    {
        const auto params = model()->actionParams();

        ui->forceAcknoledgementCheckBox->setChecked(params.needConfirmation);

        const bool useCustomText = !params.text.isEmpty();
        m_lastCustomText = params.text;
        ui->customTextCheckBox->setChecked(useCustomText);
        ui->customTextEdit->setPlainText(useCustomText ? params.text : QString());
    }

    updateValidationPolicy();
    base_type::at_model_dataChanged(fields);
}

void QnPopupBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->forceAcknoledgementCheckBox);
    setTabOrder(ui->forceAcknoledgementCheckBox, ui->customTextCheckBox);
    setTabOrder(ui->customTextCheckBox, ui->customTextEdit);
    setTabOrder(ui->customTextEdit, after);
}

void QnPopupBusinessActionWidget::parametersChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    auto params = model()->actionParams();
    params.needConfirmation = ui->forceAcknoledgementCheckBox->isChecked();
    params.text = ui->customTextCheckBox->isChecked() ? ui->customTextEdit->toPlainText() : QString();
    model()->setActionParams(params);
    updateValidationPolicy();
    updateSubjectsButton();
}

void QnPopupBusinessActionWidget::updateValidationPolicy()
{
    const bool forceAcknowledgement = ui->forceAcknoledgementCheckBox->isEnabled()
        && ui->forceAcknoledgementCheckBox->isChecked();

    if (forceAcknowledgement)
    {
        auto validationPolicy = new QnRequiredAccessRightPolicy(
            nx::vms::api::AccessRight::manageBookmarks);
        validationPolicy->setCameras(
            resourcePool()->getResourcesByIds<QnVirtualCameraResource>(model()->eventResources()));
        setValidationPolicy(validationPolicy);
    }
    else
    {
        setValidationPolicy(new QnDefaultSubjectValidationPolicy());
    }
}
