#include "play_sound_business_action_widget.h"
#include "ui_play_sound_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_value_rollback.h>

QnPlaySoundBusinessActionWidget::QnPlaySoundBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPlaySoundBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->pathLineEdit, SIGNAL(textChanged(QString)), this, SLOT(paramsChanged()));
}

QnPlaySoundBusinessActionWidget::~QnPlaySoundBusinessActionWidget()
{
}


void QnPlaySoundBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::ActionParamsField) {
        QString path = QnBusinessActionParameters::getSoundUrl(model->actionParams());
        if (ui->pathLineEdit->text() != path)
            ui->pathLineEdit->setText(path);
    }
}

void QnPlaySoundBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessParams params;
    QnBusinessActionParameters::setSoundUrl(&params, ui->pathLineEdit->text());
    model()->setActionParams(params);
}
