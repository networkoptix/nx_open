#include "show_text_overlay_action_widget.h"
#include "ui_show_text_overlay_action_widget.h"

#include <business/business_action_parameters.h>

#include <utils/common/scoped_value_rollback.h>
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

    connect(ui->fixedDurationCheckBox, &QCheckBox::toggled, ui->durationWidget, &QWidget::setEnabled);

    connect(ui->tagsLineEdit, &QLineEdit::textChanged, this, &QnShowTextOverlayActionWidget::paramsChanged);
    connect(ui->fixedDurationCheckBox, &QCheckBox::clicked, this, &QnShowTextOverlayActionWidget::paramsChanged);
    connect(ui->durationSpinBox, QnSpinboxIntValueChanged, this, &QnShowTextOverlayActionWidget::paramsChanged);
}

QnShowTextOverlayActionWidget::~QnShowTextOverlayActionWidget()
{}

void QnShowTextOverlayActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->tagsLineEdit);

    if (ui->durationWidget->isVisible()) {
        setTabOrder(ui->tagsLineEdit, ui->durationSpinBox);
        setTabOrder(ui->durationSpinBox, after);
    } else {
        setTabOrder(ui->tagsLineEdit, after);
    }
}

void QnShowTextOverlayActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields.testFlag(QnBusiness::EventTypeField)) {
        bool hasToggleState = QnBusiness::hasToggleState(model->eventType());
        ui->fixedDurationCheckBox->setEnabled(hasToggleState);
        if (!hasToggleState)
            ui->fixedDurationCheckBox->setChecked(true);
    }

    if (fields.testFlag(QnBusiness::ActionParamsField)) {
        ui->tagsLineEdit->setText(model->actionParams().tags);

        ui->fixedDurationCheckBox->setChecked(model->actionParams().durationMs > 0);
        if (ui->fixedDurationCheckBox->isChecked())
            ui->durationSpinBox->setValue(model->actionParams().durationMs / msecPerSecond);
    }
}

void QnShowTextOverlayActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnBusinessActionParameters params = model()->actionParams();
    params.tags = ui->tagsLineEdit->text();
    params.durationMs = ui->fixedDurationCheckBox->isChecked() ? ui->durationSpinBox->value() * msecPerSecond : 0;
    model()->setActionParams(params);
}
