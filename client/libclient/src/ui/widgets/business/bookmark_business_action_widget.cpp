#include "bookmark_business_action_widget.h"
#include "ui_bookmark_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <utils/common/scoped_value_rollback.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace {

    enum {
        msecPerSecond = 1000
    };

}

QnBookmarkBusinessActionWidget::QnBookmarkBusinessActionWidget(QWidget *parent) :
    base_type(parent),
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
        if (checked && (model()->eventType() == QnBusiness::UserDefinedEvent))
            model()->setEventState(QnBusiness::UndefinedState);

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

void QnBookmarkBusinessActionWidget::at_model_dataChanged(QnBusiness::Fields fields)
{
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields.testFlag(QnBusiness::EventTypeField))
    {
        bool hasToggleState = QnBusiness::hasToggleState(model()->eventType());
        if (!hasToggleState)
            ui->fixedDurationCheckBox->setChecked(true);
        setReadOnly(ui->fixedDurationCheckBox, !hasToggleState);
    }

    if (fields.testFlag(QnBusiness::ActionParamsField))
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

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnBusinessActionParameters params = model()->actionParams();
    params.tags = ui->tagsLineEdit->text();
    params.durationMs = (ui->fixedDurationCheckBox->isChecked() ? ui->durationSpinBox->value() * msecPerSecond : 0);
    params.recordBeforeMs = ui->recordBeforeSpinBox->value() * msecPerSecond;
    params.recordAfter = (ui->fixedDurationCheckBox->isChecked() ? 0 : ui->recordAfterSpinBox->value() * msecPerSecond);
    model()->setActionParams(params);
}
