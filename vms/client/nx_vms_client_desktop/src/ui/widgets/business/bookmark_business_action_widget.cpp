// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_business_action_widget.h"
#include "ui_bookmark_business_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

using namespace nx;
using namespace nx::vms::client::desktop;

namespace {

    enum {
        msecPerSecond = 1000
    };

}

QnBookmarkBusinessActionWidget::QnBookmarkBusinessActionWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::BookmarkBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->fixedDurationCheckBox, &QCheckBox::toggled, ui->durationWidget, &QWidget::setEnabled);

    connect(ui->tagsLineEdit, &QLineEdit::textChanged,      this, &QnBookmarkBusinessActionWidget::paramsChanged);
    connect(ui->fixedDurationCheckBox, &QCheckBox::clicked, this, &QnBookmarkBusinessActionWidget::paramsChanged);
    connect(ui->durationSpinBox, QnSpinboxIntValueChanged,  this, &QnBookmarkBusinessActionWidget::paramsChanged);
    connect(ui->recordBeforeSpinBox, QnSpinboxIntValueChanged,  this, &QnBookmarkBusinessActionWidget::paramsChanged);
    connect(ui->recordAfterSpinBox, QnSpinboxIntValueChanged,  this, &QnBookmarkBusinessActionWidget::paramsChanged);

    connect(ui->fixedDurationCheckBox,  &QCheckBox::toggled, this, [this](bool checked)
    {
        // Prolonged type of event has changed. In case of instant
        // action event state should be updated
        if (checked && (model()->eventType() == vms::api::EventType::userDefinedEvent))
            model()->setEventState(vms::api::EventState::undefined);

        ui->recordAfterWidget->setEnabled(!checked);
    });
}

QnBookmarkBusinessActionWidget::~QnBookmarkBusinessActionWidget()
{}

void QnBookmarkBusinessActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->fixedDurationCheckBox);
    setTabOrder(ui->fixedDurationCheckBox, ui->durationSpinBox);
    setTabOrder(ui->durationSpinBox, ui->recordBeforeSpinBox);
    setTabOrder(ui->recordBeforeSpinBox, ui->recordAfterSpinBox);
    setTabOrder(ui->recordAfterSpinBox, ui->tagsLineEdit);
    setTabOrder(ui->tagsLineEdit, after);
}

void QnBookmarkBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    base_type::at_model_dataChanged(fields);
    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::eventType))
    {
        bool hasToggleState = nx::vms::event::hasToggleState(
            model()->eventType(),
            model()->eventParams(),
            systemContext());
        if (!hasToggleState)
            ui->fixedDurationCheckBox->setChecked(true);
        setReadOnly(ui->fixedDurationCheckBox, !hasToggleState);
    }

    if (fields.testFlag(Field::actionParams))
    {
        const auto params = model()->actionParams();
        ui->tagsLineEdit->setText(params.tags);

        ui->fixedDurationCheckBox->setChecked(params.durationMs > 0);
        if (ui->fixedDurationCheckBox->isChecked())
            ui->durationSpinBox->setValue(params.durationMs / msecPerSecond);

        ui->recordBeforeSpinBox->setValue(params.recordBeforeMs / msecPerSecond);
        ui->recordAfterSpinBox->setValue(params.recordAfter / msecPerSecond);
    }

    ui->recordAfterWidget->setEnabled(!ui->fixedDurationCheckBox->isChecked());
}

void QnBookmarkBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    vms::event::ActionParameters params = model()->actionParams();
    params.tags = ui->tagsLineEdit->text();
    params.durationMs = (ui->fixedDurationCheckBox->isChecked() ? ui->durationSpinBox->value() * msecPerSecond : 0);
    params.recordBeforeMs = ui->recordBeforeSpinBox->value() * msecPerSecond;
    params.recordAfter = (ui->fixedDurationCheckBox->isChecked() ? 0 : ui->recordAfterSpinBox->value() * msecPerSecond);
    model()->setActionParams(params);
}
