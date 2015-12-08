#include "show_text_overlay_action_widget.h"
#include "ui_show_text_overlay_action_widget.h"

#include <business/business_action_parameters.h>

#include <utils/common/scoped_value_rollback.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace {

    enum {
        msecPerSecond = 1000
    };

}

QnShowTextOverlayActionWidget::QnShowTextOverlayActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::ShowTextOverlayActionWidget)
{
    ui->setupUi(this);

    connect(ui->fixedDurationCheckBox,  &QCheckBox::toggled, ui->durationWidget, &QWidget::setEnabled);
    connect(ui->customTextCheckBox,     &QCheckBox::toggled, ui->customTextEdit, &QWidget::setEnabled);

    connect(ui->fixedDurationCheckBox,  &QCheckBox::clicked,            this, &QnShowTextOverlayActionWidget::paramsChanged);
    connect(ui->customTextCheckBox,     &QCheckBox::clicked,            this, &QnShowTextOverlayActionWidget::paramsChanged);
    connect(ui->durationSpinBox,        QnSpinboxIntValueChanged,       this, &QnShowTextOverlayActionWidget::paramsChanged);
    connect(ui->customTextEdit,         &QPlainTextEdit::textChanged,   this, &QnShowTextOverlayActionWidget::paramsChanged);

    connect(ui->fixedDurationCheckBox,  &QCheckBox::toggled, this, [this](bool checked)
    {
        // Prolonged type of event has changed. In case of instant
        // action event state should be updated
        if (checked)
            model()->setEventState(QnBusiness::UndefinedState);
    });
}

QnShowTextOverlayActionWidget::~QnShowTextOverlayActionWidget()
{}

void QnShowTextOverlayActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->fixedDurationCheckBox);
    setTabOrder(ui->fixedDurationCheckBox, ui->durationSpinBox);
    setTabOrder(ui->durationSpinBox, ui->customTextCheckBox);
    setTabOrder(ui->customTextCheckBox, ui->customTextEdit);
    setTabOrder(ui->customTextEdit, after);
}

void QnShowTextOverlayActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields.testFlag(QnBusiness::EventTypeField)) {
        bool hasToggleState = QnBusiness::hasToggleState(model->eventType());
        if (!hasToggleState)
            ui->fixedDurationCheckBox->setChecked(true);
        setReadOnly(ui->fixedDurationCheckBox, !hasToggleState);
    }

    if (fields.testFlag(QnBusiness::ActionParamsField)) {
        const auto params = model->actionParams();

        ui->fixedDurationCheckBox->setChecked(params.durationMs > 0);
        if (ui->fixedDurationCheckBox->isChecked())
            ui->durationSpinBox->setValue(params.durationMs / msecPerSecond);

        ui->customTextCheckBox->setChecked(!params.text.isEmpty());
        ui->customTextEdit->setPlainText(params.text);
    }
}

void QnShowTextOverlayActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnBusinessActionParameters params = model()->actionParams();

    params.durationMs = ui->fixedDurationCheckBox->isChecked() ? ui->durationSpinBox->value() * msecPerSecond : 0;
    params.text = ui->customTextCheckBox->isChecked() ? ui->customTextEdit->toPlainText() : QString();

    model()->setActionParams(params);
}
